"""Frame-rate policy for a fixed 120 Hz panel without LTPO.

Rewrites the HGM policy of the OTA (sys_prod/etc/graphic/hgm_policy_config.xml):
every refresh-rate strategy becomes 120..120 without dynamic mode, LTPO and
adaptive sync are switched off and the power-saving 60/90 fps targets become 120.
"""
import re

PATH = "etc/graphic/hgm_policy_config.xml"


def _strategies(m):
    blk = m.group(0)
    blk = re.sub(r'min="\d+"', 'min="120"', blk)
    blk = re.sub(r'max="\d+"', 'max="120"', blk)
    blk = re.sub(r'dynamicMode="\d+"', 'dynamicMode="0"', blk)
    return re.sub(r' (Other_SF|AceAnimato|down|supportAS)="\d+"', "", blk)


def apply(ctx):
    xml = ctx.image_file("sys_prod", PATH)
    if xml is None:
        ctx.warn("  /sys_prod/%s not found: 120 Hz patch skipped" % PATH)
        return
    s = xml.decode("utf-8")
    s = re.sub(r'<Params name="refreshRate_strategy_config">.*?</Params>', _strategies, s, flags=re.S)
    s = re.sub(r'\s*<Setting name="(60|90)" id="\d+"/>', "", s)
    s = re.sub(r'(<Ltpo name="(?:switch|adaptiveSync)" value=")1(")', r"\g<1>0\2", s)
    s = re.sub(r'preferred_fps="\d+"', 'preferred_fps="120"', s)
    s = re.sub(r'(<Setting name="(?:animation_idle_fps|ui_animation|display_sync|ace_component|display_soloist)" '
               r'value=")\d+(")', r"\g<1>120\2", s)
    ctx.vendor.write("etc/sweet/hgm_policy_config.xml", s.encode("utf-8"))
    ctx.info("  hgm_policy_config.xml: fixed 120 Hz")
