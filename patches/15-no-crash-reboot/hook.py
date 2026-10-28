"""No reboots caused by crashing services.

1. AMS policy: HarmonyOS reboots the phone when SceneBoard crashes 4 times in
   4 minutes (supportSCBCrashReboot in /system/etc/ams_service_config.json).
   A copy with the flag off is bind-mounted over it at boot.
2. init "critical" services: when one of them crashes N times in M seconds init
   runs the "reboot" job (stop every service, unmount /data) and panics the
   kernel. On a port a missing driver is enough to make render_service,
   foundation or a vendor daemon crash, so the flag is cleared: in place in the
   vendor cfgs, and for system services by declaring them again, critical off,
   in /vendor/etc/init (system.img is not touched). The service is restarted
   instead (init still waits between restarts when it keeps crashing).
"""
import json
import os
import re

AMS = "system/etc/ams_service_config.json"
OVERRIDE_CFG = "etc/init/hmos2sweet_nocritical.cfg"
CRIT_LIST = re.compile(r'("critical"\s*:\s*\[\s*)1(\s*[,\]])')
CRIT_NUM = re.compile(r'("critical"\s*:\s*)1(\s*[,}\n])')


def _cfg_files(root, subdirs):
    for sub in subdirs:
        top = os.path.join(root, sub)
        if not os.path.isdir(top):
            continue
        for dp, dns, fns in os.walk(top):
            dns.sort()
            for fn in sorted(fns):
                if fn.endswith(".cfg"):
                    p = os.path.join(dp, fn)
                    if not os.path.islink(p):
                        yield p


def _uncritical(tree, subdirs, label, ctx):
    names = []
    for p in _cfg_files(tree.root, subdirs):
        with open(p, "rb") as fh:
            text = fh.read().decode("utf-8", "replace")
        if '"critical"' not in text:
            continue
        new = CRIT_NUM.sub(r"\g<1>0\2", CRIT_LIST.sub(r"\g<1>0\2", text))
        if new != text:
            with open(p, "wb") as fh:  # in place: owner, mode and label stay as they are
                fh.write(new.encode("utf-8"))
            names.append(os.path.basename(p))
    if names:
        ctx.info("  %s: critical flag cleared for %s" % (label, ", ".join(sorted(set(names)))))


def _system_overrides(ctx):
    """Services of system/sys_prod marked critical, re-declared with critical off.

    init (musl build) re-parses a service that is declared again in a later
    cfg ("Service %s already exists, updating"), and /vendor/etc/init is read
    after /system/etc/init, so system.img does not have to be rebuilt.
    """
    services = []
    for part, d in (("system", "system/etc/init"), ("sys_prod", "etc/init")):
        for name, is_dir in ctx.list_image_dir(part, d):
            if is_dir or not name.endswith(".cfg"):
                continue
            data = ctx.cat_image_file(part, d + "/" + name)
            if not data or b'"critical"' not in data:
                continue
            try:
                cfg = json.loads(data.decode("utf-8"))
            except ValueError:
                ctx.warn("  %s/%s: not plain JSON, left alone" % (d, name))
                continue
            for svc in cfg.get("services", []):
                crit = svc.get("critical")
                on = (isinstance(crit, list) and crit and crit[0] == 1) or crit == 1
                if not on:
                    continue
                svc = dict(svc)
                svc["critical"] = [0] + list(crit[1:]) if isinstance(crit, list) else 0
                services.append(svc)
    return services


def apply(ctx):
    data = ctx.image_file("system", AMS)
    if data is None:
        ctx.warn("  /%s not found: SceneBoard reboot policy left alone" % AMS)
    else:
        text = data.decode("utf-8")
        if "supportSCBCrashReboot" in text:
            text = re.sub(r'("supportSCBCrashReboot"\s*:\s*)true', r"\1false", text)
            ctx.vendor.write("etc/sweet/ams_service_config.json", text.encode("utf-8"))
            ctx.info("  supportSCBCrashReboot = false")
        else:
            ctx.info("  this HarmonyOS version has no supportSCBCrashReboot")
    _uncritical(ctx.vendor, ["etc"], "vendor", ctx)
    over = _system_overrides(ctx)
    if over:
        ctx.vendor.write(OVERRIDE_CFG, (json.dumps({"services": over}, indent=4) + "\n").encode("utf-8"))
        ctx.info("  system services no longer critical: %s" % ", ".join(s["name"] for s in over))
