"""Display geometry for the sweet panel (1080x2400, centred 24 px punch hole at y=52).

Takes the window manager config shipped in sys_prod and rewrites the values
that describe the screen, so other keys of newer HarmonyOS versions are kept.
"""
import re

PATH = "etc/window/resources/display_manager_config.xml"
VALUES = {
    "dpi": "460",
    "cutoutArea": "444 26 192 52",
    "defaultDisplayCutoutPath": "M514 26 L566 26 v 52 h -52 Z",
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
    s = s.replace("<Configs>", "<!-- hmos2sweet: Redmi Note 10 Pro (sweet) geometry -->\n<Configs>", 1)
    ctx.vendor.write("etc/sweet/display_manager_config.xml", s.encode("utf-8"))
    ctx.info("  display_manager_config.xml: dpi 460, centred cutout")
