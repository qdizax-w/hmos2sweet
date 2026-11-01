#!/bin/bash
# PIN lock screen without TEE: OpenHarmony's REE user_auth and pin_auth HDI
# services (drivers_peripheral, OpenHarmony-5.1.0-Release) built for the Pura 70
# userspace. They replace Huawei's TEE-backed services and drivers.
# Needs: ohos-clang++ (sysroot), the generated HDI headers of user_auth v3_0 (from the
# OpenHarmony-v5.0.3-Release IDL: HarmonyOS 5.x has no VerifyAuthToken, a different vtable) and
# pin_auth v2_1 (hdi-gen, see docs), openssl headers, and the Huawei libraries
# to link against in $IAMLIBS (libhdf_host, libpin_auth_stub_2.1, libuser_auth_stub_3.0).
set -e
. "$(dirname "$0")/env.sh"
S=$REPO/src/iam; P=$REPO/patches/46-pin-lock; B=$WORK/iam; mkdir -p "$B" "$P/files/lib64"
O=${OH_SRC}; GEN=${IAM_GEN:-$O/gen/iam}; OSSL=${OPENSSL_INC:-$O/third_party_openssl/include}
IAMLIBS=${IAMLIBS:?set IAMLIBS to a directory with libhdf_host/libpin_auth_stub/libuser_auth_stub}
INC="-I$S/compat -I$O/hdf_core/interfaces/inner_api/hdi -I$O/hdf_core/interfaces/inner_api/hdi/base \
 -I$O/hdf_core/interfaces/inner_api/ipc -I$O/hdf_core/interfaces/inner_api/core -I$O/hdf_core/interfaces/inner_api/utils \
 -I$O/hdf_core/interfaces/inner_api/osal/shared -I$O/hdf_core/interfaces/inner_api/osal/uhdf \
 -I$O/hdf_core/interfaces/inner_api/host/shared -I$O/hdf_core/interfaces/inner_api/host/uhdf \
 -I$O/communication_ipc/interfaces/innerkits/ipc_core/include -I$O/commonlibrary_c_utils/base/include \
 -I$O/hiviewdfx_hilog/interfaces/native/innerkits/include -I$O/third_party_bounds_checking_function/include -I$OSSL"
CFL="-O2 -fPIC -Wno-everything -D__OHOS__"
LIBS="-L$IAMLIBS -lhilog -lutils.z -lipc_single.z -lhdi.z -lhdf_utils.z -lsec_shared.z -lcrypto_openssl.z -lbegetutil.z -lc++ -lc"

build() { # name srcdir gen_pkg includes... -- sources
    local name=$1 dir=$2 pkg=$3; shift 3
    local objdir=$B/$name; mkdir -p "$objdir"; local objs=()
    for f in "$@"; do
        local o=$objdir/$(echo "$f" | tr / _).o
        case "$f" in
            *.c) ohos-clang $CFL -std=gnu11 $INC -I$GEN/$pkg -I$GEN $IDIRS -c "$dir/$f" -o "$o" ;;
            *) ohos-clang++ $CFL -std=c++17 -fno-rtti $INC -I$GEN/$pkg -I$GEN $IDIRS -c "$dir/$f" -o "$o" ;;
        esac
        objs+=("$o")
    done
    OBJS=("${objs[@]}")
}

# ---- pin_auth
PA=$S/pin_auth/hdi_service
IDIRS="-I$PA/service/inc -I$PA/adaptor/inc -I$PA/common/inc -I$PA/database/inc -I$PA/main/inc -I$S/pin_auth/common/logs -I$S/pin_auth/common/utils"
build pin_auth_service $PA pin_auth adaptor/src/adaptor_algorithm.c adaptor/src/adaptor_file.c adaptor/src/adaptor_memory.c \
  adaptor/src/adaptor_time.c adaptor/src/buffer.c adaptor/src/file_operator.c common/src/attribute.c common/src/c_array.c \
  database/src/pin_db.c database/src/pin_db_ops.c database/src/pin_db_ops_base.c database/src/pin_db_ops_v0.c \
  database/src/pin_db_ops_v1.c main/src/all_in_one_func.c main/src/collector_func.c main/src/executor_func_common.c \
  main/src/pin_auth.cpp main/src/verifier_func.c service/src/all_in_one_impl.cpp service/src/collector_impl.cpp \
  service/src/executor_impl_common.cpp service/src/pin_auth_interface_service.cpp service/src/verifier_impl.cpp
ohos-clang++ -shared -o "$B/libpin_auth_interface_service_2.1.z.so" "${OBJS[@]}" \
  -Wl,-soname,libpin_auth_interface_service_2.1.z.so $LIBS -lpin_auth_stub_2.1.z
build pin_auth_driver $PA pin_auth service/src/pin_auth_interface_driver.cpp
ohos-clang++ -shared -o "$B/libpin_auth_driver.z.so" "${OBJS[@]}" -Wl,-soname,libpin_auth_driver.z.so \
  -L"$B" -lpin_auth_interface_service_2.1.z $LIBS -lpin_auth_stub_2.1.z -lhdf_host.z -lhdf_ipc_adapter.z

# ---- user_auth
UA=$S/user_auth/hdi_service
IDIRS="-I$UA/adaptor/inc -I$UA/coauth/inc -I$UA/common/inc -I$UA/database/inc -I$UA/interface -I$UA/idm/inc \
 -I$UA/key_mgr/inc -I$UA/user_auth/inc -I$UA/utils/inc -I$UA/service -I$S/user_auth/common/logs -I$S/user_auth/common/utils"
build user_auth_service $UA user_auth adaptor/src/adaptor_algorithm.c adaptor/src/adaptor_file.c adaptor/src/adaptor_memory.c \
  adaptor/src/adaptor_time.c adaptor/src/buffer.c adaptor/src/file_operator.c coauth/src/coauth.c coauth/src/coauth_funcs.c \
  coauth/src/executor_message.c coauth/src/pool.c common/src/user_sign_centre.c database/src/file_manager_utils.c \
  database/src/global_config_file_manager.c database/src/idm_common.c database/src/idm_database.c \
  database/src/idm_file_manager.c hal_sdk/useriam_common.cpp idm/src/enroll_specification_check.c idm/src/idm_session.c \
  idm/src/user_idm_funcs.c key_mgr/src/ed25519_key.c key_mgr/src/hmac_key.c key_mgr/src/token_key.c \
  key_mgr/src/udid_manager.c service/user_auth_interface_service.cpp user_auth/src/auth_level.c \
  user_auth/src/auth_token_signer.c user_auth/src/context_manager.c user_auth/src/identify_funcs.c \
  user_auth/src/user_auth_funcs.c utils/src/attribute.c utils/src/c_array.c utils/src/linked_list.c
ohos-clang++ -shared -o "$B/libuser_auth_interface_service_3.0.z.so" "${OBJS[@]}" \
  -Wl,-soname,libuser_auth_interface_service_3.0.z.so $LIBS -luser_auth_stub_3.0.z
build user_auth_driver $UA user_auth service/user_auth_interface_driver.cpp
ohos-clang++ -shared -o "$B/libuser_auth_driver.z.so" "${OBJS[@]}" -Wl,-soname,libuser_auth_driver.z.so \
  -L"$B" -luser_auth_interface_service_3.0.z $LIBS -luser_auth_stub_3.0.z -lhdf_host.z -lhdf_ipc_adapter.z

for l in libpin_auth_interface_service_2.1.z.so libpin_auth_driver.z.so libuser_auth_interface_service_3.0.z.so \
    libuser_auth_driver.z.so; do
  llvm-strip "$B/$l"; cp "$B/$l" "$P/files/lib64/"
  "$REPO/build/check_syms.sh" "$P/files/lib64/$l" "$P/files/lib64" "$IAMLIBS" | tail -1
done
