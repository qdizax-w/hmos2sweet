#!/usr/bin/env python3
"""Compare the composer VDI vtable of two builds: HarmonyOS's composer service calls it by slot.

A build against the wrong OpenHarmony headers can still link and then call the wrong method
at runtime, so build_display.sh refuses a new library whose slots differ from the old one.
usage: check_vtable.py old.so new.so
"""
import struct
import sys

SYMBOL = "_ZTVN4OHOS3HDI7DISPLAY22DisplayComposerVdiImplE"
R_RELATIVE = 1027
FIRST_METHOD = 32  # offset-to-top, typeinfo and the two destructors are not slots the service calls


def slots(path):
    d = open(path, "rb").read()
    shoff = struct.unpack_from("<Q", d, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x3a)
    sec = [struct.unpack_from("<IIQQQQIIQQ", d, shoff + i * shentsize) for i in range(shnum)]
    names = d[sec[shstrndx][4]:sec[shstrndx][4] + sec[shstrndx][5]]
    by_name = {names[s[0]:names.index(b"\0", s[0])].decode(): s for s in sec}
    dynsym, dynstr = by_name[".dynsym"], by_name[".dynstr"]
    strs = d[dynstr[4]:dynstr[4] + dynstr[5]]
    syms = []
    for i in range(dynsym[5] // 24):
        name, _, _, _, value, size = struct.unpack_from("<IBBHQQ", d, dynsym[4] + i * 24)
        syms.append((strs[name:strs.index(b"\0", name)].decode(), value, size))
    addr = {}
    for name, value, _ in syms:
        if value:
            addr.setdefault(value, name)
    rel = {}
    r = by_name[".rela.dyn"]
    for i in range(r[5] // 24):
        off, info, addend = struct.unpack_from("<QQq", d, r[4] + i * 24)
        rel[off] = addr.get(addend, hex(addend)) if info & 0xffffffff == R_RELATIVE else syms[info >> 32][0]
    table = next((s for s in syms if s[0] == SYMBOL), None)
    if table is None:
        sys.exit("%s: no %s" % (path, SYMBOL))
    return [rel.get(o) for o in range(table[1] + FIRST_METHOD, table[1] + table[2], 8)]


def main():
    old, new = slots(sys.argv[1]), slots(sys.argv[2])
    if old != new:
        for i in range(max(len(old), len(new))):
            a = old[i] if i < len(old) else None
            b = new[i] if i < len(new) else None
            if a != b:
                print("slot %d: %s -> %s" % (i, a, b))
        sys.exit("composer vtable changed: wrong OpenHarmony headers?")
    print("composer vtable identical: %d slots" % len(new))


if __name__ == "__main__":
    main()
