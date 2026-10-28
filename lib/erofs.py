"""EROFS partition images: extract to a tree, edit, rebuild with the same metadata.

Uses the erofs-utils build with Android extensions (extract.erofs, mkfs.erofs
with --fs-config-file / --file-contexts, dump.erofs), see tools/fetch_tools.sh.

Paths inside a partition are written like on the device without the mount
point: "lib64/libfoo.so" for /vendor/lib64/libfoo.so, "system/app/X" for
/system/app/X on the system partition (system-as-root layout).
"""
import os
import re
import shutil
import subprocess

from .util import die, info, tool, run, mkdirs, rmtree

DEFAULT_LABELS = [  # used when no existing entry can be copied
    ("bin/", "u:object_r:{p}_bin_file:s0"),
    ("lib", "u:object_r:{p}_lib_file:s0"),
    ("etc/", "u:object_r:{p}_etc_file:s0"),
    ("firmware", "u:object_r:{p}_file:s0"),
]


class Tree:
    def __init__(self, image, workdir, name):
        self.image = image
        self.name = name
        self.work = workdir
        self.root = os.path.join(workdir, name)          # mkfs source directory
        self.cfg_dir = os.path.join(workdir, "config")
        self.fs_config = os.path.join(self.cfg_dir, name + "_fs_config")
        self.contexts = os.path.join(self.cfg_dir, name + "_file_contexts")
        self.options = os.path.join(self.cfg_dir, name + "_fs_options")

    # ------------------------------------------------------------ extract
    def extract(self):
        rmtree(self.work)
        mkdirs(self.work)
        info("extracting %s" % os.path.basename(self.image))
        p = subprocess.run([tool("extract.erofs"), "-i", self.image, "-x", "-o", self.work, "-s", "-f", "-T4"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        # extract.erofs names the tree after the image file; normalise it
        produced = [d for d in os.listdir(self.work) if d != "config" and os.path.isdir(os.path.join(self.work, d))]
        if len(produced) != 1:
            die("extract.erofs failed for %s\n%s" % (self.image, p.stdout.decode(errors="replace")[-2000:]))
        if produced[0] != self.name:
            os.rename(os.path.join(self.work, produced[0]), self.root)
            for f in os.listdir(self.cfg_dir):
                if f.startswith(produced[0] + "_"):
                    os.rename(os.path.join(self.cfg_dir, f),
                              os.path.join(self.cfg_dir, self.name + f[len(produced[0]):]))
            for path in (self.fs_config, self.contexts):
                with open(path) as fh:
                    lines = fh.read().splitlines()
                pre_cfg, pre_ctx = produced[0] + "/", "/" + produced[0] + "/"
                out = []
                for l in lines:
                    if path == self.fs_config and (l.startswith(pre_cfg) or l.split(" ")[0] == produced[0]):
                        l = self.name + l[len(produced[0]):]
                    elif path == self.contexts and (l.startswith(pre_ctx) or l.split(" ")[0] == "/" + produced[0]):
                        l = "/" + self.name + l[len(produced[0]) + 1:]
                    out.append(l)
                with open(path, "w") as fh:
                    fh.write("\n".join(out) + "\n")
        self._repair()
        self._read_options()

    def _repair(self):
        """extract.erofs cannot fill directories that are read-only (mode 0500)
        when not running as root: make them writable and pull the missing
        entries one by one with dump.erofs."""
        for r, dirs, _ in os.walk(self.root):
            for d in dirs:
                p = os.path.join(r, d)
                if not os.path.islink(p):
                    os.chmod(p, 0o755)
        missing = []
        with open(self.fs_config) as fh:
            for line in fh:
                key = line.split(" ")[0]
                rel = key[len(self.name):]
                if rel and not os.path.lexists(self.root + rel):
                    missing.append(rel)
        if not missing:
            return
        info("recovering %d entries from read-only directories" % len(missing))
        dump = tool("dump.erofs")
        for rel in missing:
            out = subprocess.run([dump, "--path=" + rel, self.image], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE).stdout.decode(errors="replace").splitlines()
            kind = out[1] if len(out) > 1 else ""
            dst = self.root + rel
            if "directory" in kind:
                os.makedirs(dst, exist_ok=True)
            elif "symlink" in kind:
                tgt = subprocess.run([dump, "--cat", "--path=" + rel, self.image], stdout=subprocess.PIPE).stdout
                mkdirs(os.path.dirname(dst))
                os.symlink(tgt.decode(), dst)
            elif "regular" in kind:
                mkdirs(os.path.dirname(dst))
                with open(dst, "wb") as o:
                    o.write(subprocess.run([dump, "--cat", "--path=" + rel, self.image],
                                           stdout=subprocess.PIPE).stdout)
            else:  # listed in fs_config but not in the image (lost+found): drop it
                self._drop_config(rel)

    def _read_options(self):
        self.uuid = None
        self.mount_point = "/" + self.name
        self.compression = "lz4hc"
        if os.path.isfile(self.options):
            txt = open(self.options).read()
            m = re.search(r"-U\s+([0-9a-f-]{36})", txt)
            if m:
                self.uuid = m.group(1)
            m = re.search(r"--mount-point=(\S+)", txt)
            if m:
                self.mount_point = m.group(1)
            m = re.search(r"-z(\S+)", txt)
            if m:
                self.compression = m.group(1)

    # ------------------------------------------------------------- labels
    def _ctx_key(self, rel):
        return "/" + self.name + "/" + rel.strip("/")

    def label_of(self, rel):
        key = self._ctx_key(rel) if rel.strip("/") else "/" + self.name
        with open(self.contexts) as fh:
            for line in fh:
                parts = line.split()
                if len(parts) >= 2 and parts[0].replace("\\", "") == key:
                    return parts[1]
        return None

    def _default_label(self, rel):
        for prefix, lab in DEFAULT_LABELS:
            if rel.startswith(prefix):
                return lab.format(p=self.name)
        return "u:object_r:%s_file:s0" % self.name

    def _has_config(self, rel):
        key = self.name + "/" + rel.strip("/")
        with open(self.fs_config) as fh:
            return any(l.split(" ")[0] == key for l in fh)

    def _add_config(self, rel, mode, uid=0, gid=2000, label=None, label_from=None):
        rel = rel.strip("/")
        if not self._has_config(rel):
            with open(self.fs_config, "a") as fh:
                fh.write("%s/%s %d %d %s\n" % (self.name, rel, uid, gid, mode))
        if self.label_of(rel) is None:
            lab = label or (label_from and self.label_of(label_from)) or self._nearest_label(rel)
            with open(self.contexts, "a") as fh:
                fh.write("%s %s\n" % (re.sub(r"([.+*?\[\]()])", r"\\\1", self._ctx_key(rel)), lab))

    def _nearest_label(self, rel):
        """Label of a sibling file of the same kind, else a sensible default."""
        d = os.path.dirname(rel)
        if os.path.isdir(self.path(rel)) and not os.path.islink(self.path(rel)):
            return self.label_of(d) or self._default_label(rel)
        if os.path.isdir(self.root + "/" + d):
            for f in sorted(os.listdir(self.root + "/" + d)):
                cand = (d + "/" + f).strip("/")
                if cand != rel and os.path.isfile(self.root + "/" + cand) and not os.path.islink(self.root + "/" + cand):
                    lab = self.label_of(cand)
                    if lab:
                        return lab
        return self._default_label(rel)

    def _drop_config(self, rel):
        rel = rel.strip("/")
        key = self.name + "/" + rel
        ctx = self._ctx_key(rel)
        for path, test in ((self.fs_config, lambda k: k == key or k.startswith(key + "/")),
                           (self.contexts, lambda k: k.replace("\\", "") == ctx or k.replace("\\", "").startswith(ctx + "/"))):
            with open(path) as fh:
                lines = fh.read().splitlines()
            with open(path, "w") as fh:
                fh.write("\n".join(l for l in lines if not (l.split() and test(l.split()[0]))) + "\n")

    # -------------------------------------------------------------- edits
    def path(self, rel):
        return self.root + "/" + rel.strip("/")

    def exists(self, rel):
        return os.path.lexists(self.path(rel))

    def add_dir(self, rel, mode="0755", label_from=None):
        for i in range(1, len(rel.strip("/").split("/")) + 1):
            part = "/".join(rel.strip("/").split("/")[:i])
            if not os.path.isdir(self.path(part)):
                os.makedirs(self.path(part), exist_ok=True)
            self._add_config(part, mode, label_from=label_from)

    def add_file(self, src, rel, mode="0644", label_from=None, data=None, uid=0, gid=2000):
        """Copy a file in (replacing a file or symlink with the same name)."""
        dst = self.path(rel)
        parent = os.path.dirname(rel.strip("/"))
        if parent and not os.path.isdir(self.path(parent)):
            self.add_dir(parent)
        if os.path.lexists(dst):
            os.remove(dst)
        if data is not None:
            with open(dst, "wb") as o:
                o.write(data)
        else:
            shutil.copyfile(src, dst)
        os.chmod(dst, int(mode, 8))
        self._add_config(rel, mode, uid=uid, gid=gid, label_from=label_from)

    def add_symlink(self, rel, target):
        dst = self.path(rel)
        parent = os.path.dirname(rel.strip("/"))
        if parent and not os.path.isdir(self.path(parent)):
            self.add_dir(parent)
        if os.path.lexists(dst):
            if os.path.isdir(dst) and not os.path.islink(dst):
                rmtree(dst)
            else:
                os.remove(dst)
        os.symlink(target, dst)
        self._add_config(rel, "0644", gid=0)

    def remove(self, rel):
        p = self.path(rel)
        if not os.path.lexists(p):
            return False
        if os.path.isdir(p) and not os.path.islink(p):
            rmtree(p)
        else:
            os.remove(p)
        self._drop_config(rel)
        return True

    def read(self, rel):
        with open(self.path(rel), "rb") as fh:
            return fh.read()

    def write(self, rel, data, mode="0644"):
        self.add_file(None, rel, mode, data=data if isinstance(data, bytes) else data.encode())

    # -------------------------------------------------------------- build
    def build(self, out_image):
        info("building %s" % os.path.basename(out_image))
        cmd = [tool("mkfs.erofs"), "-z" + self.compression, "-T", "0"]
        if self.uuid:
            cmd += ["-U", self.uuid]
        cmd += ["--mount-point=" + self.mount_point, "--fs-config-file=" + self.fs_config,
                "--file-contexts=" + self.contexts, out_image, self.root]
        if os.path.exists(out_image):
            os.remove(out_image)
        run(cmd)
        run([tool("fsck.erofs"), out_image])
        return out_image


def read_file(image, path, workdir):
    """Extract one file from an EROFS image (path as on the device without mount point)."""
    tmp = mkdirs(os.path.join(workdir, "_one"))
    run([tool("extract.erofs"), "-i", image, "-X", "/" + path.strip("/"), "-o", tmp, "-s", "-f"], check=False)
    for r, _, files in os.walk(tmp):
        if os.path.basename(path) in files and r.replace("\\", "/").endswith(os.path.dirname("/" + path.strip("/"))):
            with open(os.path.join(r, os.path.basename(path)), "rb") as fh:
                data = fh.read()
            rmtree(tmp)
            return data
    rmtree(tmp)
    return None


def list_dir(image, path):
    """Names in one directory of an EROFS image: [(name, is_dir)], [] if missing."""
    p = subprocess.run([tool("dump.erofs"), "--ls", "--path=/" + path.strip("/"), image],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True)
    out = []
    for line in p.stdout.splitlines():
        m = re.match(r"\s*(\d+)\s+(\d+)\s+(.+)$", line)
        if m and m.group(3) not in (".", ".."):
            out.append((m.group(3), m.group(2) == "2"))
    return out


def cat_file(image, path):
    """Contents of one (small) file of an EROFS image, None if missing."""
    p = subprocess.run([tool("dump.erofs"), "--cat", "--path=/" + path.strip("/"), image],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return p.stdout if p.returncode == 0 and p.stdout else None
