#!/bin/sh
# Download the erofs-utils build with Android extensions (extract.erofs,
# mkfs.erofs --fs-config-file/--file-contexts) into tools/bin.
# Source: https://github.com/sekaiacg/erofs-utils (Apache-2.0 / GPL-2.0)
set -e
cd "$(dirname "$0")"
TAG=v1.8.10-251217
# hmos2curtana: macOS (Darwin) builds too. The Darwin checksums were recorded from the release on 2026-11-01.
OS=Linux
[ "$(uname -s)" = Darwin ] && OS=Darwin
case "$OS-$(uname -m)" in
  Linux-x86_64|Linux-amd64) ARCH=x86_64; SHA=6d6c6b0e7ad7273cc019644e5d6ad1e263142fad72874e169cc0731854f95bf1 ;;
  Linux-aarch64|Linux-arm64) ARCH=aarch64; SHA=b5f2563e0b1c9713d71b6f95735df916cf18c1a4ef18197b30f4b54f1d3b461b ;;
  Darwin-x86_64) ARCH=x86_64; SHA=af0cc7beade7c4043d44d9e0752e8d6c0aca6ca6eb98b4265ed2d06820a1d9ce ;;
  Darwin-arm64) ARCH=aarch64; SHA=fde25ba470144fff2b7338a5bbf49abc1b9b9aee04c701d1f1dcc1826500a3ca ;;
  *) echo "unsupported system $OS $(uname -m)"; exit 1 ;;
esac
ZIP=erofs-utils-v1.8.10-gee46dd74-251217-${OS}_$ARCH.zip
URL=https://github.com/sekaiacg/erofs-utils/releases/download/$TAG/$ZIP
mkdir -p bin
if command -v curl >/dev/null; then curl -L -o "bin/$ZIP" "$URL"; else wget -O "bin/$ZIP" "$URL"; fi
if command -v sha256sum >/dev/null; then echo "$SHA  bin/$ZIP" | sha256sum -c -; else echo "$SHA  bin/$ZIP" | shasum -a 256 -c -; fi
(cd bin && unzip -o "$ZIP" && rm -f "$ZIP" && chmod +x *.erofs)
# macOS: downloaded binaries are quarantined by Gatekeeper
[ "$OS" = Darwin ] && xattr -d com.apple.quarantine bin/*.erofs 2>/dev/null || true
echo "erofs tools installed in tools/bin:"
ls bin
