"""Raise the software HEVC decoder size limit (EXPERIMENTAL).

Byte patches for known builds of /system/lib64/libhevc_decoder.z.so; other
builds are left alone. The patched copy goes to /vendor/etc/sweet and is
bind-mounted over the original at boot.
"""
import hashlib

PATH = "lib64/libhevc_decoder.z.so"
KNOWN = {
    # HarmonyOS 5.1 (Pura 70 full OTA, 2025-07)
    "33e09e01eea0f1f5d0bb31c8a48ee7c8": [
        (0xc01c, 0x80, 0xff), (0xc01d, 0x07, 0x0f),      # caps: max width 1920 -> 4095
        (0xc0b4, 0x80, 0xff), (0xc0b5, 0x07, 0x0f),      # caps: max height 1920 -> 4095
        (0x15e9a, 0x1e, 0x40), (0x15e9d, 0xfd, 0xf9), (0x15e9e, 0x1d, 0x3f),  # configure check
    ],
}


def apply(ctx):
    data = ctx.image_file("system", "system/" + PATH)
    if data is None:
        ctx.warn("  /system/%s not found" % PATH)
        return
    md5 = hashlib.md5(data).hexdigest()
    if md5 not in KNOWN:
        ctx.warn("  unknown libhevc_decoder build (%s): not patched" % md5)
        return
    b = bytearray(data)
    for off, old, new in KNOWN[md5]:
        if b[off] != old:
            ctx.warn("  unexpected byte at 0x%x: not patched" % off)
            return
        b[off] = new
    ctx.vendor.write("etc/sweet/libhevc_decoder.z.so", bytes(b))
    ctx.info("  libhevc_decoder.z.so patched (%d bytes changed)" % len(KNOWN[md5]))
