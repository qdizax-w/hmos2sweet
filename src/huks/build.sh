#!/bin/bash
# Software HUKS engine for HarmonyOS on sweet: replaces Huawei's TEE-backed
# /vendor/lib64/chipsetsdk/libhuks_ca_hdi.z.so with OpenHarmony's huks core
# (OpenSSL crypto engine, software root key).
# Called by build/build_ohos.sh with HUKS_SRC, OPENSSL_INC, BCF, HILOG_INC and OUT set.
set -e
H=${HUKS_SRC:?}
B=${OUT:?}
HERE=$(cd "$(dirname "$0")" && pwd)
INC="-I$H/interfaces/inner_api/huks_standard/main/include
 -I$H/frameworks/huks_standard/main/common/include
 -I$H/frameworks/huks_standard/main/crypto_engine/crypto_common/include
 -I$H/frameworks/huks_standard/main/crypto_engine/openssl/include
 -I$H/frameworks/huks_standard/main/crypto_engine/rkc/include
 -I$H/frameworks/huks_standard/main/os_dependency/sysinfo/include
 -I$H/frameworks/huks_standard/main/core/include
 -I$H/services/huks_standard/huks_engine/main/core/include
 -I$H/services/huks_standard/huks_engine/main/core_dependency/include
 -I$H/services/huks_standard/huks_engine/main/device_cert_manager/include
 -I$H/services/huks_standard/huks_service/main/core/include
 -I$H/utils/crypto_adapter -I$H/utils/file_operator -I$H/utils/list -I$H/utils/mutex
 -I$BCF -I$HILOG_INC -I$OPENSSL_INC"
DEF="-D_HUKS_LOG_ENABLE_ -DL2_STANDARD -DHKS_ENABLE_CLEAN_FILE -DSUPPORT_STORAGE_BACKUP -D_HARDWARE_ROOT_KEY_
 -DHKS_ENABLE_UPGRADE_KEY -DHKS_KEY_VERSION=3 -DHKS_CONFIG_KEY_STORE_PATH=\"/data/service/el1/public/huks_service\"
 -DHUKS_UID_TRUST_LIST={0} -DOPENSSL_SUPPRESS_DEPRECATED -DOPENSSL_NO_DEPRECATED_3_0=0"
SRC="$H/services/huks_standard/huks_engine/main/core/src/*.c
 $H/services/huks_standard/huks_engine/main/device_cert_manager/src/*.c
 $H/frameworks/huks_standard/main/common/src/*.c
 $H/frameworks/huks_standard/main/crypto_engine/crypto_common/src/*.c
 $H/frameworks/huks_standard/main/crypto_engine/openssl/src/*.c
 $H/frameworks/huks_standard/main/os_dependency/posix/*.c
 $H/utils/crypto_adapter/hks_client_service_adapter.c $H/utils/crypto_adapter/hks_client_service_adapter_common.c
 $H/utils/file_operator/hks_file_operator.c $H/utils/list/hks_double_list.c $H/utils/mutex/hks_mutex.c
 $HERE/hks_sweet_extra.c"
mkdir -p $B/obj
fails=0
for f in $SRC; do
  case $f in *_lite.c) continue;; esac
  o=$B/obj/$(basename ${f%.*}).o
  ohos-clang -std=gnu11 -O2 -Wno-everything $DEF $INC -c $f -o $o 2>>$B/err.log || { echo "FAIL $f"; fails=$((fails+1)); }
done
for f in $H/services/huks_standard/huks_engine/main/core_dependency/src/*.cpp; do
  o=$B/obj/$(basename ${f%.*}).o
  ohos-clang++ -std=c++17 -O2 -Wno-everything $DEF $INC -c $f -o $o 2>>$B/err.log || { echo "FAIL $f"; fails=$((fails+1)); }
done
echo "compile failures: $fails"
[ "$fails" = 0 ] || { echo "see $B/err.log"; exit 1; }
ohos-clang++ -shared -o "$B/libhuks_ca_hdi.z.so" "$B"/obj/*.o -Wl,-soname,libhuks_ca_hdi.z.so \
  -Wl,--version-script="$HERE/huks.map" -lcrypto_openssl.z -lsec_shared.z -lhilog -lutils.z -lc++ -lc
llvm-strip "$B/libhuks_ca_hdi.z.so"
