#!/bin/bash
# Only the audio VDI (libaudio_primary_impl.z.so, patch 31): the one HarmonyOS userspace binary that
# hmos2curtana changes (curtana's WCD937x/TAS2562 sound card). Everything else in patches/*/files is
# reused from hmos2sweet as is.
# Needs: build/make_sysroot.sh done first (it wants an hmos2curtana work folder with ota/system.img and
# ota/vendor.img); clang/lld 15+, llvm-strip, git. Works on Linux and macOS (with an LLVM release toolchain).
set -e
. "$(dirname "$0")/env.sh"
S=$REPO/src; P=$REPO/patches; A=$WORK/ohos/audio
CXX="ohos-clang++ -std=c++17 -O2 -fPIC -Wno-everything"
CC="ohos-clang -O2 -fPIC -Wno-everything"
mkdir -p "$A/obj"
[ -d "$A/tinyalsa" ] || git clone -q https://github.com/tinyalsa/tinyalsa "$A/tinyalsa"
for f in mixer mixer_hw pcm pcm_hw limits snd_card_plugin; do
  $CC -fvisibility=hidden -I"$A/tinyalsa/include" -c "$A/tinyalsa/src/$f.c" -o "$A/obj/ta_$f.o"
done
$CXX -fvisibility=hidden -I"$S/audio/vdi" -I"$A/tinyalsa/include" -c "$S/audio/sweet_audio_vdi.cpp" -o "$A/obj/vdi.o"
ohos-clang++ -shared -o "$A/libaudio_primary_impl.z.so" "$A"/obj/*.o -Wl,-soname,libaudio_primary_impl.z.so \
  -Wl,--no-undefined -lc++ -lc -lm -ldl
llvm-strip "$A/libaudio_primary_impl.z.so"
cp "$A/libaudio_primary_impl.z.so" "$P/31-audio/files/lib64/"
echo "audio VDI -> patches/31-audio/files/lib64/libaudio_primary_impl.z.so"
