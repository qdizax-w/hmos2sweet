#!/bin/bash
# cambridge (bionic) and sweetcam-launch (static) for patch 47.
# Needs: clang/lld, aarch64-linux-gnu-gcc, the AOSP headers (bionic, hardware/libhardware, system/core,
# system/media) in $AOSP, and the bionic runtime + libcamera_metadata of a sweet Android ROM in $ANDROID_LIBS
# (lib64 folder with libc.so, libdl.so, libm.so, libcamera_metadata.so) and crtbegin/crtend objects in $ANDROID_CRT.
set -e
. "$(dirname "$0")/env.sh"
: "${AOSP:=$WORK/aosp}" "${ANDROID_LIBS:=$WORK/android/lib64}" "${ANDROID_CRT:=$WORK/android/crt}"
S=$REPO/src/camera; P=$REPO/patches/47-camera; B=$WORK/camera_android; mkdir -p "$B"
BI=$AOSP/platform_bionic/libc
INC="-nostdinc -isystem $(clang -print-resource-dir)/include -isystem $BI/include -isystem $BI/kernel/uapi \
 -isystem $BI/kernel/uapi/asm-arm64 -isystem $BI/kernel/android/uapi -I$AOSP/platform_hardware_libhardware/include \
 -I$AOSP/platform_system_media/camera/include -I$AOSP/platform_system_core/libcutils/include \
 -I$AOSP/platform_system_core/libsystem/include"
clang --target=aarch64-linux-android33 -fPIC -O2 -fno-stack-protector $INC -c "$S/bridge/cambridge.c" -o "$B/cambridge.o"
ld.lld -m aarch64linux -pie --dynamic-linker=/system/bin/linker64 -z noexecstack -z relro -z now --hash-style=gnu \
  -o "$B/cambridge" "$ANDROID_CRT/crtbegin_dynamic.o" "$B/cambridge.o" -L"$ANDROID_LIBS" -lcamera_metadata -lc -ldl -lm \
  "$ANDROID_CRT/crtend_android.o"
llvm-strip -o "$P/files/android/system/bin/cambridge" "$B/cambridge"
aarch64-linux-gnu-gcc -static -O2 -w -o "$P/files/bin/sweetcam-launch" "$S/launcher/sweetcam-launch.c"
aarch64-linux-gnu-strip "$P/files/bin/sweetcam-launch"
echo "camera bridge rebuilt"
