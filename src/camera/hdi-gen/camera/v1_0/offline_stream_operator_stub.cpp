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

#include "v1_0/offline_stream_operator_stub.h"
#include <hdf_base.h>
#include <hdf_core_log.h>

#define HDF_LOG_TAG    offline_stream_operator_stub

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {
template<typename ElementType>
static bool ReadPodArray(MessageParcel &parcel, std::vector<ElementType> &data);


template<typename ElementType>
static bool ReadPodArray(MessageParcel &parcel, std::vector<ElementType> &data)
{
    data.clear();
    uint32_t size = 0;
    if (!parcel.ReadUint32(size)) {
        HDF_LOGE("%{public}s: failed to read size", __func__);
        return false;
    }

    if (size == 0) {
        return true;
    }
    const ElementType *dataPtr = reinterpret_cast<const ElementType*>(parcel.ReadUnpadBuffer(sizeof(ElementType) * size));
    if (dataPtr == nullptr) {
        HDF_LOGI("%{public}s: failed to read data", __func__);
        return false;
    }
    data.assign(dataPtr, dataPtr + size);
    return true;
}


OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStub(const sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> &impl)
    : IPCObjectStub(OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor()), impl_(impl)
{
}

OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::~OfflineStreamOperatorStub()
{
    HDF_LOGI("%{public}s enter", __func__);
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option)
{
    switch (code) {
        case CMD_OFFLINE_STREAM_OPERATOR_GET_VERSION:
            return OfflineStreamOperatorStubGetVersion(data, reply, option);
        case CMD_OFFLINE_STREAM_OPERATOR_CANCEL_CAPTURE:
            return OfflineStreamOperatorStubCancelCapture(data, reply, option);
        case CMD_OFFLINE_STREAM_OPERATOR_RELEASE_STREAMS:
            return OfflineStreamOperatorStubReleaseStreams(data, reply, option);
        case CMD_OFFLINE_STREAM_OPERATOR_RELEASE:
            return OfflineStreamOperatorStubRelease(data, reply, option);
        default: {
            HDF_LOGE("%{public}s: cmd %{public}d is not supported", __func__, code);
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubCancelCapture(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubCancelCapture_(offlineStreamOperatorData, offlineStreamOperatorReply, offlineStreamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubReleaseStreams(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubReleaseStreams_(offlineStreamOperatorData, offlineStreamOperatorReply, offlineStreamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubRelease(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubRelease_(offlineStreamOperatorData, offlineStreamOperatorReply, offlineStreamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubGetVersion(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubGetVersion_(offlineStreamOperatorData, offlineStreamOperatorReply, offlineStreamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubCancelCapture_(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl)
{
    if (offlineStreamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t captureId = 0;
    if (!offlineStreamOperatorData.ReadInt32(captureId)) {
        HDF_LOGE("%{public}s: read captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t offlineStreamOperatorRet = impl->CancelCapture(captureId);
    if (offlineStreamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, offlineStreamOperatorRet);
        return offlineStreamOperatorRet;
    }

    return offlineStreamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubReleaseStreams_(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl)
{
    if (offlineStreamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<int32_t> streamIds;
    if (!ReadPodArray(offlineStreamOperatorData, streamIds)) {
        HDF_LOGE("%{public}s: failed to read streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t offlineStreamOperatorRet = impl->ReleaseStreams(streamIds);
    if (offlineStreamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, offlineStreamOperatorRet);
        return offlineStreamOperatorRet;
    }

    return offlineStreamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubRelease_(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl)
{
    if (offlineStreamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t offlineStreamOperatorRet = impl->Release();
    if (offlineStreamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, offlineStreamOperatorRet);
        return offlineStreamOperatorRet;
    }

    return offlineStreamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub::OfflineStreamOperatorStubGetVersion_(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl)
{
    if (offlineStreamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint32_t majorVer = 0;

    uint32_t minorVer = 0;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t offlineStreamOperatorRet = impl->GetVersion(majorVer, minorVer);
    if (offlineStreamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, offlineStreamOperatorRet);
        return offlineStreamOperatorRet;
    }

    if (!offlineStreamOperatorReply.WriteUint32(majorVer)) {
        HDF_LOGE("%{public}s: write majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!offlineStreamOperatorReply.WriteUint32(minorVer)) {
        HDF_LOGE("%{public}s: write minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return offlineStreamOperatorRet;
}
} // V1_0
} // Camera
} // HDI
} // OHOS
