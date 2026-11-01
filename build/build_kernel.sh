#!/bin/bash
# Kernel: crDroid android_kernel_xiaomi_sm6250 (branch 16.0, miatoll: curtana/excalibur/gram/joyeuse)
# + src/kernel/patches. crDroid's tree is used instead of LineageOS's because it already carries the
# EROFS backport (identical to the one in LineageOS sm6150 used by hmos2sweet); HarmonyOS images are EROFS.
# Output: prebuilt/kernel/{Image.gz,cust-atoll-ab.dtb,dtbo.img}
# Needs: Linux (case-sensitive FS), clang/lld (LLVM 15+), aarch64-linux-gnu- and arm-linux-gnueabi- binutils, python3.
set -e
. "$(dirname "$0")/env.sh"
BASE=8b4afb3dd59ea7f94bef73d61bc23a2b713b1df7   # "miatoll_defconfig: Enable CONFIG_WIREGUARD" on crDroid 16.0
if [ ! -d "$CURTANA_KERNEL/.git" ]; then
  git clone -b 16.0 https://github.com/crdroidandroid/android_kernel_xiaomi_sm6250 "$CURTANA_KERNEL"
fi
cd "$CURTANA_KERNEL"
git cat-file -e "$BASE" 2>/dev/null || git fetch origin "$BASE"
git checkout -B hmos2curtana "$BASE"
git -c user.name=hmos2curtana -c user.email=hmos2curtana@localhost am "$REPO"/src/kernel/patches/*.patch
M="O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 CC=clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi-"
make $M vendor/xiaomi/miatoll_defconfig vendor/ohos.config
make $M -j"$(nproc)" Image.gz dtbs
B=out/arch/arm64/boot
D=$B/dts/qcom
cp $B/Image.gz $D/cust-atoll-ab.dtb "$REPO/prebuilt/kernel/"
# all four miatoll overlays, like the LineageOS/crDroid dtbo.img: the bootloader picks the one
# matching the board id (curtana, excalibur, gram, joyeuse)
python3 "$REPO/build/mkdtboimg.py" "$REPO/prebuilt/kernel/dtbo.img" 4096 \
  $D/curtana-atoll-ab-idp-overlay.dtbo $D/excalibur-atoll-ab-idp-overlay.dtbo \
  $D/gram-atoll-ab-idp-overlay.dtbo $D/joyeuse-atoll-ab-idp-overlay.dtbo
echo "kernel -> prebuilt/kernel"
