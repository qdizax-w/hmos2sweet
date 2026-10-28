#!/bin/bash
# Kernel: LineageOS android_kernel_xiaomi_sm6150 (lineage-24.0) + src/kernel/patches.
# Output: prebuilt/kernel/{Image.gz,xiaomi-sdmmagpie.dtb,dtbo.img}
# Needs: clang/lld (LLVM 15+), aarch64-linux-gnu- and arm-linux-gnueabi- binutils, python3.
set -e
. "$(dirname "$0")/env.sh"
BASE=e03e5ba22   # "ARM64: configs: sweet: Enable ln8000 charger" on lineage-24.0
if [ ! -d "$SWEET_KERNEL/.git" ]; then
  git clone -b lineage-24.0 https://github.com/LineageOS/android_kernel_xiaomi_sm6150 "$SWEET_KERNEL"
fi
cd "$SWEET_KERNEL"
git checkout -B hmos2sweet "$BASE"
git am "$REPO"/src/kernel/patches/*.patch
M="O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 CC=clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi-"
make $M vendor/sdmsteppe-perf_defconfig vendor/sweet.config vendor/ohos.config
make $M -j"$(nproc)" Image.gz dtbs
B=out/arch/arm64/boot
cp $B/Image.gz $B/dts/qcom/xiaomi-sdmmagpie.dtb "$REPO/prebuilt/kernel/"
python3 "$REPO/build/mkdtboimg.py" "$REPO/prebuilt/kernel/dtbo.img" 4096 $B/dts/qcom/sweet-sdmmagpie-overlay.dtbo
echo "kernel -> prebuilt/kernel"
