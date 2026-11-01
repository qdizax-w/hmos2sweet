#!/bin/bash
# Only hmos-preinit (prebuilt/ramdisk/init): the static helper hmos2curtana changes (debug log window).
# Needs: aarch64-linux-gnu-gcc with static glibc (Debian/Ubuntu: gcc-aarch64-linux-gnu).
set -e
. "$(dirname "$0")/env.sh"
aarch64-linux-gnu-gcc -static -O2 -w -I"$REPO/src/preinit" -o "$REPO/prebuilt/ramdisk/init" "$REPO/src/preinit/preinit.c"
aarch64-linux-gnu-strip "$REPO/prebuilt/ramdisk/init"
echo "hmos-preinit -> prebuilt/ramdisk/init"
