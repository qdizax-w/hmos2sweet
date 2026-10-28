#!/bin/bash
# Rebuild every prebuilt binary of the repository from src/.
#   build/build_all.sh <hmos2sweet work folder>     (for the sysroot libraries)
set -e
D=$(dirname "$0")
bash "$D/build_kernel.sh"
bash "$D/build_static.sh"
bash "$D/fetch_oh_sources.sh"
bash "$D/make_sysroot.sh" "${1:?usage: build_all.sh <hmos2sweet work folder>}"
bash "$D/build_ohos.sh"
[ -d "${MESA_SRC:-$D/work/mesa}" ] && bash "$D/build_mesa.sh" || echo "MESA_SRC not set: Mesa prebuilt kept"
[ -d "${AOSP:-$D/work/aosp}" ] && bash "$D/build_camera_android.sh" || echo "AOSP headers not set: camera bridge prebuilt kept"
