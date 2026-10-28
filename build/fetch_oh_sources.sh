#!/bin/bash
# OpenHarmony 5.1 sources needed by the userspace components (headers + HUKS core).
set -e
. "$(dirname "$0")/env.sh"
TAG=OpenHarmony-v5.1.0-Release
mkdir -p "$OH_SRC"; cd "$OH_SRC"
for r in drivers_interface drivers_peripheral hdf_core graphic_graphic_surface commonlibrary_c_utils \
         communication_ipc hiviewdfx_hilog hiviewdfx_hitrace third_party_libdrm third_party_bounds_checking_function \
         security_huks third_party_openssl; do
  [ -d "$r" ] || git clone --depth 1 -b "$TAG" "https://gitee.com/openharmony/$r.git" "$r"
done
echo "OpenHarmony sources in $OH_SRC"
