/*
 * Extra entry points exported by Huawei's TEE-backed libhuks_ca_hdi.z.so that
 * the open-source software engine does not implement.  They are not used by
 * libhuks_hdi_service_1.1 (it only imports the standard HuksHdi* set) but are
 * provided so any other caller fails cleanly instead of failing to load.
 */
#include "hks_type.h"

#define HKS_SWEET_UNSUPPORTED(name) \
    int32_t name(void) { return HKS_ERROR_NOT_SUPPORTED; }

HKS_SWEET_UNSUPPORTED(HuksHdiWrapKey)
HKS_SWEET_UNSUPPORTED(HuksHdiUnwrapKey)
HKS_SWEET_UNSUPPORTED(HuksHdiBatchUnwrapKey)
HKS_SWEET_UNSUPPORTED(HuksHdiMigrateKey)
HKS_SWEET_UNSUPPORTED(HuksHdiWrapData)
HKS_SWEET_UNSUPPORTED(HuksHdiUnwrapData)
HKS_SWEET_UNSUPPORTED(HuksHdiUpgradeData)

/* key material lives in the key blob files owned by huks_service; the
 * software engine keeps no per-key state to delete */
int32_t HuksHdiDeleteKey(void) { return HKS_SUCCESS; }
