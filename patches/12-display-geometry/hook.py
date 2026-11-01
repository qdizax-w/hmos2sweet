"""Display geometry for the curtana panel (6.67" 1080x2400 LCD, centred punch hole).

Cutout from LineageOS sm6250-common (config_mainBuiltInDisplayCutout: 72 x 102 px, centred, from the top).

Takes the window manager config shipped in sys_prod and rewrites the values
that describe the screen, so other keys of newer HarmonyOS versions are kept.
"""
import re

PATH = "etc/window/resources/display_manager_config.xml"
VALUES = {
    "dpi": "460",
    "cutoutArea": "444 0 192 102",
    "defaultDisplayCutoutPath": "M504 0 L576 0 v 102 h -72 Z",
    "curvedScreenBoundary": "0 0 0 0",
}


def apply(ctx):
    xml = ctx.image_file("sys_prod", PATH)
    if xml is None:
        ctx.warn("  /sys_prod/%s not found: geometry patch skipped" % PATH)
        return
    s = xml.decode("utf-8")
    for tag, val in VALUES.items():
        if re.search(r"<%s>.*?</%s>" % (tag, tag), s, re.S):
            s = re.sub(r"<%s>.*?</%s>" % (tag, tag), "<%s>%s</%s>" % (tag, val, tag), s, flags=re.S)
        else:
            s = s.replace("</Configs>", "  <%s>%s</%s>\n</Configs>" % (tag, val, tag))
    s = re.sub(r'<isWaterfallDisplay enable="true"', '<isWaterfallDisplay enable="false"', s)
    s = s.replace("<Configs>", "<!-- hmos2curtana: Redmi Note 9S (curtana) geometry -->\n<Configs>", 1)
    ctx.vendor.write("etc/sweet/display_manager_config.xml", s.encode("utf-8"))
    ctx.info("  display_manager_config.xml: dpi 460, centred cutout")
