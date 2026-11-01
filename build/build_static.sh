#!/bin/bash
# Static aarch64 helpers (no HarmonyOS libraries needed): hmos-preinit, sweet-rproc,
# qrtr/rmtfs/tqftpserv/pd-mapper, sweet-oobedone, sqlite3, idle.
# Needs: aarch64-linux-gnu-gcc (glibc static), git, curl.
set -e
. "$(dirname "$0")/env.sh"
CC="aarch64-linux-gnu-gcc -static -O2"
STRIP=aarch64-linux-gnu-strip
P=$REPO/patches
S=$REPO/src
B=$WORK/static; mkdir -p "$B"

$CC -w -I"$S/preinit" -o "$REPO/prebuilt/ramdisk/init" "$S/preinit/preinit.c" && $STRIP "$REPO/prebuilt/ramdisk/init"
mkdir -p "$REPO/prebuilt/tools" && $CC -w -o "$REPO/prebuilt/tools/lpwrite" "$S/preinit/lpwrite.c" && $STRIP "$REPO/prebuilt/tools/lpwrite"
$CC -w -o "$P/30-remoteproc/files/bin/sweet-rproc" "$S/remoteproc/sweet-rproc.c"
$CC -o "$P/22-camera-daemon-idle/files/bin/CameraDaemon" "$S/idle/idle.c"

# linux-msm userspace (BSD-3-Clause) with the sweet patches
cd "$B"
for r in qrtr rmtfs tqftpserv pd-mapper; do [ -d $r ] || git clone -q https://github.com/linux-msm/$r; done
git -C qrtr checkout -q 9dc7a88; git -C rmtfs checkout -q b30a3eb; git -C tqftpserv checkout -q b31de6d; git -C pd-mapper checkout -q 5ecd2fe
for r in rmtfs tqftpserv pd-mapper; do git -C $r checkout -q -- .; git -C $r apply "$S/remoteproc/patches/$r.patch"; done
cp "$S/remoteproc/patches/pd-mapper-lzma_stub.c" pd-mapper/lzma_stub.c
A=$B/libqrtr; mkdir -p "$A"
for f in qrtr qmi logging; do $CC -w -Iqrtr/lib -Iqrtr/src -c qrtr/lib/$f.c -o "$A/$f.o"; done
aarch64-linux-gnu-ar rcs "$A/libqrtr.a" "$A"/*.o
O=$P/30-remoteproc/files/bin
$CC -w -Iqrtr/lib -Iqrtr/src qrtr/src/ns.c qrtr/src/addr.c qrtr/src/hash.c qrtr/src/map.c qrtr/src/util.c qrtr/src/waiter.c qrtr/lib/logging.c -o "$O/qrtr-ns"
$CC -w -Iqrtr/lib -Iqrtr/src qrtr/src/lookup.c qrtr/src/util.c "$A/libqrtr.a" -o "$O/qrtr-lookup"
$CC -w -DANDROID -Iqrtr/lib rmtfs/qmi_rmtfs.c rmtfs/rmtfs.c rmtfs/rproc.c rmtfs/sharedmem.c rmtfs/storage.c rmtfs/util.c "$A/libqrtr.a" -lpthread -o "$O/rmtfs"
$CC -w -Iqrtr/lib tqftpserv/tqftpserv.c tqftpserv/translate.c "$A/libqrtr.a" -o "$O/tqftpserv"
$CC -w -Iqrtr/lib pd-mapper/pd-mapper.c pd-mapper/assoc.c pd-mapper/json.c pd-mapper/servreg_loc.c pd-mapper/lzma_stub.c "$A/libqrtr.a" -o "$O/pd-mapper"

# hexagonrpcd (GPL-3.0) on the downstream adsprpc driver, for the ADSP sensors PD
[ -d hexagonrpc ] || git clone -q https://github.com/linux-msm/hexagonrpc
git -C hexagonrpc checkout -q da7a374
$CC -w -I"$S/sensors/compat" -Ihexagonrpc/include -Ihexagonrpc/hexagonrpcd -Ihexagonrpc/libhexagonrpc \
  hexagonrpc/libhexagonrpc/*.c hexagonrpc/libhexagonrpc/interface/*.c hexagonrpc/hexagonrpcd/*.c \
  hexagonrpc/hexagonrpcd/interface/*.c "$S/sensors/sweet_fastrpc.c" -o "$P/34-sensors-pd/files/bin/hexagonrpcd"

# sqlite (public domain) for sweet-oobedone and the debug shell
[ -f sqlite/sqlite3.c ] || git clone -q --depth 1 https://github.com/azadkuh/sqlite-amalgamation sqlite
F="-DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_THREADSAFE=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -Isqlite"
$CC -w $F -o "$P/50-remove-setup-wizard/files/bin/sweet-oobedone" "$S/oobe/sweet-oobedone.c" sqlite/sqlite3.c
$CC -w $F -o "$P/60-debug-tools/files/bin/sqlite3" sqlite/shell.c sqlite/sqlite3.c

$STRIP "$P"/30-remoteproc/files/bin/* "$P"/34-sensors-pd/files/bin/* "$P"/22-camera-daemon-idle/files/bin/* "$P"/50-remove-setup-wizard/files/bin/* "$P"/60-debug-tools/files/bin/*
echo "static helpers rebuilt"
