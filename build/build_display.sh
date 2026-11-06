#!/bin/bash
# Display buffer + composer VDI (patch 10). Called by build_ohos.sh; CI runs it alone.
# Needs: make_sysroot.sh and fetch_oh_sources.sh done first; clang/lld 15+, llvm-strip.
set -e
. "$(dirname "$0")/env.sh"
S=$REPO/src; P=$REPO/patches; B=$WORK/ohos; mkdir -p "$B"
O=$OH_SRC
CXX="ohos-clang++ -std=c++17 -O2 -fPIC -Wno-everything"
BCF="$O/third_party_bounds_checking_function/include"

HAL_INC="-I$S/display/common -I$S/display/hdi-gen -I$S/display/hdi-gen/display -I$S/display/hdi-gen/display/buffer \
 -I$S/display/hdi-gen/display/composer -I$S/display/hdi-gen/display/graphic/common \
 -I$O/drivers_interface/display/composer/hdifd_parcelable -I$O/drivers_interface/display/composer \
 -I$O/drivers_interface/display -I$O/hiviewdfx_hitrace/interfaces/native/innerkits/include/hitrace_meter \
 -I$O/drivers_interface/display/composer/common/include -I$O/drivers_peripheral/display/buffer/hdi_service/include \
 -I$O/drivers_peripheral/display/composer/hdi_service/include -I$O/drivers_peripheral/display/utils/include \
 -I$O/graphic_graphic_surface/interfaces/inner_api/utils -I$O/hdf_core/interfaces/inner_api/utils \
 -I$O/hdf_core/interfaces/inner_api/osal/shared -I$O/hdf_core/interfaces/inner_api/osal/uhdf \
 -I$O/hdf_core/interfaces/inner_api/hdi -I$O/hdf_core/interfaces/inner_api/hdi/base -I$O/hdf_core/interfaces/inner_api/ipc \
 -I$O/hdf_core/interfaces/inner_api/core -I$O/communication_ipc/interfaces/innerkits/ipc_core/include \
 -I$O/commonlibrary_c_utils/base/include -I$O/hiviewdfx_hilog/interfaces/native/innerkits/include \
 -I$O/third_party_libdrm -I$O/third_party_libdrm/include/drm -I$BCF"
D=$B/display; mkdir -p "$D/obj"
$CXX $HAL_INC -shared -o "$D/libdisplay_buffer_vdi_impl.z.so" "$S/display/buffer/sweet_buffer_vdi.cpp" \
  -Wl,-soname,libdisplay_buffer_vdi_impl.z.so -ldrm -lhilog -lc++ -lc
for f in "$S"/display/composer/src/*.cpp; do
  $CXX -I"$S/display/composer/include" $HAL_INC -c "$f" -o "$D/obj/$(basename "${f%.cpp}").o"
done
ohos-clang++ -shared -o "$D/libdisplay_composer_vdi_impl.z.so" "$D"/obj/*.o -Wl,-soname,libdisplay_composer_vdi_impl.z.so \
  -L"$D" -ldisplay_buffer_vdi_impl.z -ldrm -lhilog -lhdf_utils.z -lsec_shared.z -lhitrace_meter -lutils.z -lc++ -lc
llvm-strip "$D"/*.so
cp "$D"/libdisplay_buffer_vdi_impl.z.so "$D"/libdisplay_composer_vdi_impl.z.so "$P/10-display/files/lib64/"
echo "display -> patches/10-display/files/lib64"
