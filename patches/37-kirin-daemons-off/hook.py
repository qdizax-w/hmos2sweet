"""Drop init "wait" commands for Kirin-only device nodes.

The Huawei vendor cfgs wait up to 5 s each for paths that never appear on
sweet (sensorhub, IR, fb0, THP touch, NVE): about 25 s of extra boot time
during which init does nothing else.
"""
import os
import re

KIRIN_PATHS = ("/sys/devices/platform/huawei_sensor", "/sys/class/irda", "/sys/class/graphics/fb0",
               "/sys/touchscreen", "/dev/nve0", "/dev/block/by-name/oeminfo", "/dev/block/by-name/nvme")
WAIT = re.compile(r'\s*"wait\s+([^"\s]+)[^"]*"\s*,?\n')


def apply(ctx):
    root = ctx.vendor.path("etc")
    dropped = []
    for dp, dns, fns in os.walk(root):
        for fn in fns:
            if not fn.endswith(".cfg"):
                continue
            p = os.path.join(dp, fn)
            if os.path.islink(p):
                continue
            with open(p, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            if '"wait' not in text:
                continue

            def repl(m):
                if any(m.group(1).rstrip("/").startswith(k) for k in KIRIN_PATHS):
                    dropped.append("%s (%s)" % (m.group(1), fn))
                    return "\n"
                return m.group(0)
            new = WAIT.sub(repl, text)
            # a removed last element may leave a trailing comma before "]"
            new = re.sub(r",(\s*\])", r"\1", new)
            if new != text:
                with open(p, "w", encoding="utf-8") as fh:
                    fh.write(new)
    if dropped:
        ctx.info("  removed waits: %s" % ", ".join(dropped))
