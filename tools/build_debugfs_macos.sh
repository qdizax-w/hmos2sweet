#!/bin/sh
# macOS has no e2fsprogs: build debugfs (used to read the ext4 vendor image of the curtana ROM)
# from the official kernel.org source into tools/bin. Needs the Xcode command line tools.
set -e
cd "$(dirname "$0")"
V=1.47.1
SHA=0d2e0bf80935c3392b73a60dbff82d8a6ef7ea88b806c2eea964b6837d3fd6c2
T=$(mktemp -d)
curl -sL -o "$T/e2fsprogs.tar.gz" "https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v$V/e2fsprogs-$V.tar.gz"
echo "$SHA  $T/e2fsprogs.tar.gz" | shasum -a 256 -c -
tar xzf "$T/e2fsprogs.tar.gz" -C "$T"
mkdir -p "$T/build" bin
(cd "$T/build" && "../e2fsprogs-$V/configure" --disable-nls --disable-fuse2fs --disable-e2initrd-helper \
   --without-libarchive >/dev/null && make -j"$(sysctl -n hw.ncpu)" libs >/dev/null && make -C debugfs debugfs >/dev/null)
cp "$T/build/debugfs/debugfs" bin/debugfs
rm -rf "$T"
echo "debugfs installed in tools/bin"
