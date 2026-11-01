"""Frame-rate policy for curtana's fixed 60 Hz LCD (no LTPO, no high refresh rate).

Rewrites the HGM policy of the OTA (sys_prod/etc/graphic/hgm_policy_config.xml):
every refresh-rate strategy becomes 60..60 without dynamic mode, LTPO and
adaptive sync are switched off and the 90/120 fps settings are removed.
The composer reports the panel's single 60 Hz DRM mode as it is (the virtual
60 Hz mode of hmos2sweet is only added for a lone 120 Hz mode).
"""
import re

PATH = "etc/graphic/hgm_policy_config.xml"
HZ = "60"


def _strategies(m):
    blk = m.group(0)
    blk = re.sub(r'min="\d+"', 'min="%s"' % HZ, blk)
    blk = re.sub(r'max="\d+"', 'max="%s"' % HZ, blk)
    blk = re.sub(r'dynamicMode="\d+"', 'dynamicMode="0"', blk)
    return re.sub(r' (Other_SF|AceAnimato|down|supportAS)="\d+"', "", blk)


def apply(ctx):
    xml = ctx.image_file("sys_prod", PATH)
    if xml is None:
        ctx.warn("  /sys_prod/%s not found: 60 Hz patch skipped" % PATH)
        return
    s = xml.decode("utf-8")
    s = re.sub(r'<Params name="refreshRate_strategy_config">.*?</Params>', _strategies, s, flags=re.S)
    s = re.sub(r'\s*<Setting name="(90|120|144)" id="\d+"/>', "", s)
    s = re.sub(r'(<Ltpo name="(?:switch|adaptiveSync)" value=")1(")', r"\g<1>0\2", s)
    s = re.sub(r'preferred_fps="\d+"', 'preferred_fps="%s"' % HZ, s)
    s = re.sub(r'(<Setting name="(?:animation_idle_fps|ui_animation|display_sync|ace_component|display_soloist)" '
               r'value=")\d+(")', r"\g<1>%s\2" % HZ, s)
    ctx.vendor.write("etc/sweet/hgm_policy_config.xml", s.encode("utf-8"))
    ctx.info("  hgm_policy_config.xml: fixed 60 Hz")
