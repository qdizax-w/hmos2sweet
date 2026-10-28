"""Remove the HarmonyOS setup wizard (HwStartupGuide).

The app folder itself is removed through "system_remove" in patch.json; this
hook drops its entry from the preinstall list and keeps a copy of the original
list in <out>/removed/. sweet-oobedone (started from init) marks the first
boot setup as finished in the settings database, otherwise SceneBoard keeps
trying to start the wizard.
"""
import json
import re

LIST = "system/etc/app/install_list.json"


def apply(ctx):
    s = ctx.system()
    if not s.exists(LIST):
        ctx.warn("  /%s not found" % LIST)
        return
    raw = s.read(LIST)
    ctx.save_removed("system/" + LIST + ".orig", raw)
    try:
        data = json.loads(raw.decode("utf-8"))
        before = len(data.get("install_list", []))
        data["install_list"] = [e for e in data.get("install_list", [])
                                if "HwStartupGuide" not in e.get("app_dir", "")]
        if len(data["install_list"]) == before:
            ctx.info("  wizard not in install_list.json")
            return
        text = json.dumps(data, indent="\t", ensure_ascii=False)
    except ValueError:  # keep the original formatting if it is not strict JSON
        text = re.sub(r'\s*\{\s*"app_dir"\s*:\s*"/system/app/HwStartupGuide"[^}]*\},?', "", raw.decode("utf-8"))
    s.write(LIST, text.encode("utf-8"))
    ctx.info("  HwStartupGuide removed from install_list.json")
