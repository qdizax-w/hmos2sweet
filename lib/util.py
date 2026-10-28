"""Small helpers shared by the hmos2sweet steps: logging, running tools, paths."""
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS_BIN = os.path.join(REPO, "tools", "bin")

_COLOR = sys.stdout.isatty() and os.name != "nt"


def _c(code, text):
    return "\033[%sm%s\033[0m" % (code, text) if _COLOR else text


def step(msg):
    print(_c("1;36", "==> ") + _c("1", msg), flush=True)


def info(msg):
    print("    " + msg, flush=True)


def warn(msg):
    print(_c("1;33", "    ! ") + msg, flush=True)


class Fatal(Exception):
    pass


def die(msg):
    raise Fatal(msg)


def tool(name):
    """Path of an external tool: tools/bin first (fetched by tools/fetch_tools.sh), then PATH."""
    local = os.path.join(TOOLS_BIN, name)
    if os.path.isfile(local) and os.access(local, os.X_OK):
        return local
    found = shutil.which(name)
    if not found:
        die("'%s' not found. Run tools/fetch_tools.sh (erofs tools) or install it "
            "(debugfs is in e2fsprogs)." % name)
    return found


def run(cmd, quiet=True, check=True, **kw):
    """Run a command; on failure show its output."""
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kw)
    out = p.stdout.decode(errors="replace")
    if check and p.returncode != 0:
        die("command failed (%d): %s\n%s" % (p.returncode, " ".join(cmd), out[-3000:]))
    if not quiet and out.strip():
        for line in out.strip().splitlines()[-5:]:
            info(line)
    return out


def mkdirs(path):
    os.makedirs(path, exist_ok=True)
    return path


def rmtree(path):
    """rm -rf that also works on trees with read-only directories (erofs extractions)."""
    if not os.path.lexists(path):
        return
    for root, dirs, _ in os.walk(path):
        for d in dirs:
            p = os.path.join(root, d)
            if not os.path.islink(p):
                try:
                    os.chmod(p, 0o755)
                except OSError:
                    pass
    try:
        os.chmod(path, 0o755)
    except OSError:
        pass
    shutil.rmtree(path)


def human(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return "%.1f %s" % (n, unit) if unit != "B" else "%d B" % n
        n /= 1024.0
