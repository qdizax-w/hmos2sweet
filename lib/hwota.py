"""Read HarmonyOS full OTA packages (update_full_base.zip -> update.bin).

update.bin layout (HarmonyOS 4/5 full packages):
  0x00   header (version, product, build date ...)
  0xb0   u16 type, u16 length of the component table
  0xb4   component table, 87 bytes per entry:
             +0   char[32] name ("/system", "/vendor", ...)
             +32  ...      flags / version
             +47  u32      size in bytes
             +55  u8[32]   sha256 of the image
  ...    signature blocks ("update/info.bin")
  then   the component images, back to back, in table order.

The images start right after the header, wherever the signature ends, so the
start of the data is computed as file size - sum of the component sizes.
"""
import hashlib
import os
import struct
import zipfile

from .util import die, info, human

ENTRY_SIZE = 87


class Component:
    def __init__(self, name, size, sha256, offset):
        self.name = name
        self.size = size
        self.sha256 = sha256
        self.offset = offset

    def __repr__(self):
        return "<%s %s @%d>" % (self.name, human(self.size), self.offset)


class HwOta:
    def __init__(self, path):
        if not os.path.isfile(path):
            die("OTA package not found: %s" % path)
        self.path = path
        self.zip = None
        if zipfile.is_zipfile(path):
            self.zip = zipfile.ZipFile(path)
            names = [i for i in self.zip.infolist() if i.filename.endswith("update.bin")]
            if not names:
                die("%s does not contain update.bin: is it a HarmonyOS *full* OTA package?" % path)
            self.member = max(names, key=lambda i: i.file_size)
            self.total = self.member.file_size
        else:  # a bare update.bin
            self.member = None
            self.total = os.path.getsize(path)
        self._parse()

    def _open(self):
        return self.zip.open(self.member) if self.zip else open(self.path, "rb")

    def _parse(self):
        with self._open() as f:
            head = f.read(64 * 1024)
        if len(head) < 0xb4:
            die("update.bin too small")
        _, length = struct.unpack_from("<HH", head, 0xb0)
        if length == 0 or length % ENTRY_SIZE or 0xb4 + length > len(head):
            die("unknown update.bin layout (component table length %d)" % length)
        entries = []
        p = 0xb4
        while p < 0xb4 + length:
            e = head[p:p + ENTRY_SIZE]
            name = e[:32].split(b"\0")[0].decode(errors="replace").strip("/")
            size = struct.unpack_from("<I", e, 47)[0]
            entries.append((name, size, e[55:87].hex()))
            p += ENTRY_SIZE
        data_start = self.total - sum(s for _, s, _ in entries)
        if not (0xb4 + length <= data_start <= 1024 * 1024):
            die("update.bin: component sizes do not match the file size (data start %d)" % data_start)
        self.components = []
        off = data_start
        for name, size, sha in entries:
            self.components.append(Component(name, size, sha, off))
            off += size
        self.by_name = {c.name: c for c in self.components}
        # version string, best effort
        self.version = ""
        try:
            kv = dict(l.split("=", 1) for l in self.read_small("package_info").decode(errors="replace").split()
                      if "=" in l)
            self.version = " ".join(x for x in (kv.get("deviceModel"), kv.get("versionCode")) if x)
        except Exception:
            pass

    def has(self, name):
        return name in self.by_name

    def read_small(self, name):
        c = self.by_name[name]
        with self._open() as f:
            f.seek(c.offset)
            return f.read(c.size)

    def extract(self, names, outdir, verify=True):
        """Extract the named components to outdir/<name>.img in one sequential pass."""
        want = [self.by_name[n] for n in names if n in self.by_name]
        want.sort(key=lambda c: c.offset)
        paths = {}
        with self._open() as f:
            pos = 0
            for c in want:
                dst = os.path.join(outdir, c.name + ".img")
                if os.path.isfile(dst) and os.path.getsize(dst) == c.size:
                    info("%-10s already extracted" % c.name)
                    paths[c.name] = dst
                    continue
                skip = c.offset - pos
                while skip > 0:  # zip members only stream forward efficiently
                    b = f.read(min(skip, 16 << 20))
                    if not b:
                        die("unexpected end of update.bin")
                    skip -= len(b)
                h = hashlib.sha256()
                left = c.size
                with open(dst + ".part", "wb") as o:
                    while left:
                        b = f.read(min(left, 16 << 20))
                        if not b:
                            die("unexpected end of update.bin while reading %s" % c.name)
                        h.update(b)
                        o.write(b)
                        left -= len(b)
                pos = c.offset + c.size
                if verify and h.hexdigest() != c.sha256:
                    # some components are signed differently; only warn
                    info("%-10s %s (sha256 differs from table)" % (c.name, human(c.size)))
                else:
                    info("%-10s %s" % (c.name, human(c.size)))
                os.replace(dst + ".part", dst)
                paths[c.name] = dst
        return paths
