"""Audio policy for the sweet sound card.

The OTA's /vendor/etc/audio/audio_policy_config.xml describes Huawei's HiFi
DSP, with MMAP ("fast"), offload and always-on hotword pipes. The sweet HAL
only has normal PCM playback/capture, so those pipes and the devices that only
use them are removed.
"""
import re

PATH = "etc/audio/audio_policy_config.xml"
DROP_PIPES = ("fast_output", "offload_output", "fast_input", "wakeup_input")


def _check_prebuilt(ctx):
    """hmos2curtana: the HAL must be the curtana build (build/build_audio.sh or the CI workflow)."""
    import os
    so = os.path.join(os.path.dirname(os.path.abspath(__file__)), "files", "lib64", "libaudio_primary_impl.z.so")
    with open(so, "rb") as fh:
        if b"mixer_paths_wcd937x.xml" not in fh.read():
            ctx.warn("  libaudio_primary_impl.z.so is still hmos2sweet's (sm6150) build: there will be NO SOUND on "
                     "curtana. Rebuild it with the GitHub workflow or build/build_audio.sh, then convert again.")


def apply(ctx):
    _check_prebuilt(ctx)
    v = ctx.vendor
    if not v.exists(PATH):
        ctx.warn("  /vendor/%s not found: audio policy left as is" % PATH)
        return
    s = v.read(PATH).decode("utf-8")
    a = s.find('<adapter name="primary"')
    b = s.find("</adapter>", a)
    if a < 0 or b < 0:
        ctx.warn("  no primary adapter in the audio policy: left as is")
        return
    prim = s[a:b]
    for p in DROP_PIPES:
        prim = re.sub(r'\n\s*<pipe name="%s" role="[a-z]+"[^>]*>.*?</pipe>' % p, "", prim, flags=re.S)

    def fix(m):
        keep = [x for x in m.group(1).split(",") if x and x not in DROP_PIPES]
        return 'supportPipes="%s"' % ",".join(keep)
    prim = re.sub(r'supportPipes="([^"]*)"', fix, prim)
    prim = re.sub(r'\n\s*<device [^>]*supportPipes=""[^>]*/>', "", prim)
    s = s[:a] + prim + s[b:]
    v.write(PATH, s.encode("utf-8"))
    ctx.info("  audio_policy_config.xml: primary adapter reduced to PCM playback/capture")
