/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "v1_0/offline_stream_operator_proxy.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <hdi_support.h>
#include <message_option.h>
#include <message_parcel.h>
#include <string_ex.h>

#define HDF_LOG_TAG    offline_stream_operator_proxy

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

template<typename ElementType>
static bool WritePodArray(MessageParcel &parcel, const std::vector<ElementType> &data);


template<typename ElementType>
static bool WritePodArray(MessageParcel &parcel, const std::vector<ElementType> &data)
{
    if (!parcel.WriteUint32(data.size())) {
        HDF_LOGE("%{public}s: failed to write data size", __func__);
        return false;
    }
    if (data.empty()) {
        return true;
    }
    if (!parcel.WriteUnpadBuffer((const void*)data.data(), sizeof(ElementType) * data.size())) {
        HDF_LOGE("%{public}s: failed to write array", __func__);
        return false;
    }
    return true;
}
int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::CancelCapture(int32_t captureId)
{
    return OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::CancelCapture_(captureId, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::ReleaseStreams(const std::vector<int32_t>& streamIds)
{
    return OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::ReleaseStreams_(streamIds, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::Release()
{
    return OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::Release_(Remote());
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::GetVersion(uint32_t& majorVer, uint32_t& minorVer)
{
    return OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::GetVersion_(majorVer, minorVer, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::CancelCapture_(int32_t captureId,
     const sptr<IRemoteObject> remote)
{
    MessageParcel offlineStreamOperatorData;
    MessageParcel offlineStreamOperatorReply;
    MessageOption offlineStreamOperatorOption(MessageOption::TF_SYNC);

    if (!offlineStreamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!offlineStreamOperatorData.WriteInt32(captureId)) {
        HDF_LOGE("%{public}s: write captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t offlineStreamOperatorRet = remote->SendRequest(CMD_OFFLINE_STREAM_OPERATOR_CANCEL_CAPTURE, offlineStreamOperatorData, offlineStreamOperatorReply, offlineStreamOperatorOption);
    if (offlineStreamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, offlineStreamOperatorRet);
        return offlineStreamOperatorRet;
    }

    return offlineStreamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::ReleaseStreams_(const std::vector<int32_t>& streamIds,
     const sptr<IRemoteObject> remote)
{
    MessageParcel offlineStreamOperatorData;
    MessageParcel offlineStreamOperatorReply;
    MessageOption offlineStreamOperatorOption(MessageOption::TF_SYNC);

    if (!offlineStreamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(offlineStreamOperatorData, streamIds)) {
        HDF_LOGE("%{public}s: failed to write streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t offlineStreamOperatorRet = remote->SendRequest(CMD_OFFLINE_STREAM_OPERATOR_RELEASE_STREAMS, offlineStreamOperatorData, offlineStreamOperatorReply, offlineStreamOperatorOption);
    if (offlineStreamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, offlineStreamOperatorRet);
        return offlineStreamOperatorRet;
    }

    return offlineStreamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::Release_(const sptr<IRemoteObject> remote)
{
    MessageParcel offlineStreamOperatorData;
    MessageParcel offlineStreamOperatorReply;
    MessageOption offlineStreamOperatorOption(MessageOption::TF_SYNC);

    if (!offlineStreamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t offlineStreamOperatorRet = remote->SendRequest(CMD_OFFLINE_STREAM_OPERATOR_RELEASE, offlineStreamOperatorData, offlineStreamOperatorReply, offlineStreamOperatorOption);
    if (offlineStreamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, offlineStreamOperatorRet);
        return offlineStreamOperatorRet;
    }

    return offlineStreamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy::GetVersion_(uint32_t& majorVer, uint32_t& minorVer,
     const sptr<IRemoteObject> remote)
{
    MessageParcel offlineStreamOperatorData;
    MessageParcel offlineStreamOperatorReply;
    MessageOption offlineStreamOperatorOption(MessageOption::TF_SYNC);

    if (!offlineStreamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t offlineStreamOperatorRet = remote->SendRequest(CMD_OFFLINE_STREAM_OPERATOR_GET_VERSION, offlineStreamOperatorData, offlineStreamOperatorReply, offlineStreamOperatorOption);
    if (offlineStreamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, offlineStreamOperatorRet);
        return offlineStreamOperatorRet;
    }

    if (!offlineStreamOperatorReply.ReadUint32(majorVer)) {
        HDF_LOGE("%{public}s: read majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!offlineStreamOperatorReply.ReadUint32(minorVer)) {
        HDF_LOGE("%{public}s: read minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return offlineStreamOperatorRet;
}

} // V1_0
} // Camera
} // HDI
} // OHOS
