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

#include "v1_0/stream_operator_callback_stub.h"
#include <hdf_base.h>
#include <hdf_core_log.h>

#define HDF_LOG_TAG    stream_operator_callback_stub

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


OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStub(const sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> &impl)
    : IPCObjectStub(OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor()), impl_(impl)
{
}

OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::~StreamOperatorCallbackStub()
{
    HDF_LOGI("%{public}s enter", __func__);
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option)
{
    switch (code) {
        case CMD_STREAM_OPERATOR_CALLBACK_GET_VERSION:
            return StreamOperatorCallbackStubGetVersion(data, reply, option);
        case CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_STARTED:
            return StreamOperatorCallbackStubOnCaptureStarted(data, reply, option);
        case CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_ENDED:
            return StreamOperatorCallbackStubOnCaptureEnded(data, reply, option);
        case CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_ERROR:
            return StreamOperatorCallbackStubOnCaptureError(data, reply, option);
        case CMD_STREAM_OPERATOR_CALLBACK_ON_FRAME_SHUTTER:
            return StreamOperatorCallbackStubOnFrameShutter(data, reply, option);
        default: {
            HDF_LOGE("%{public}s: cmd %{public}d is not supported", __func__, code);
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureStarted(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureStarted_(streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureEnded(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureEnded_(streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureError(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureError_(streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnFrameShutter(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnFrameShutter_(streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubGetVersion(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubGetVersion_(streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureStarted_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl)
{
    if (streamOperatorCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t captureId = 0;
    if (!streamOperatorCallbackData.ReadInt32(captureId)) {
        HDF_LOGE("%{public}s: read captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<int32_t> streamIds;
    if (!ReadPodArray(streamOperatorCallbackData, streamIds)) {
        HDF_LOGE("%{public}s: failed to read streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorCallbackRet = impl->OnCaptureStarted(captureId, streamIds);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    return streamOperatorCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureEnded_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl)
{
    if (streamOperatorCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t captureId = 0;
    if (!streamOperatorCallbackData.ReadInt32(captureId)) {
        HDF_LOGE("%{public}s: read captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<OHOS::HDI::Camera::V1_0::CaptureEndedInfo> infos;
    if (!ReadPodArray(streamOperatorCallbackData, infos)) {
        HDF_LOGE("%{public}s: failed to read infos", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorCallbackRet = impl->OnCaptureEnded(captureId, infos);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    return streamOperatorCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnCaptureError_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl)
{
    if (streamOperatorCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t captureId = 0;
    if (!streamOperatorCallbackData.ReadInt32(captureId)) {
        HDF_LOGE("%{public}s: read captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<OHOS::HDI::Camera::V1_0::CaptureErrorInfo> infos;
    if (!ReadPodArray(streamOperatorCallbackData, infos)) {
        HDF_LOGE("%{public}s: failed to read infos", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorCallbackRet = impl->OnCaptureError(captureId, infos);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    return streamOperatorCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubOnFrameShutter_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl)
{
    if (streamOperatorCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t captureId = 0;
    if (!streamOperatorCallbackData.ReadInt32(captureId)) {
        HDF_LOGE("%{public}s: read captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<int32_t> streamIds;
    if (!ReadPodArray(streamOperatorCallbackData, streamIds)) {
        HDF_LOGE("%{public}s: failed to read streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint64_t timestamp = 0;
    if (!streamOperatorCallbackData.ReadUint64(timestamp)) {
        HDF_LOGE("%{public}s: read timestamp failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorCallbackRet = impl->OnFrameShutter(captureId, streamIds, timestamp);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    return streamOperatorCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub::StreamOperatorCallbackStubGetVersion_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl)
{
    if (streamOperatorCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint32_t majorVer = 0;

    uint32_t minorVer = 0;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorCallbackRet = impl->GetVersion(majorVer, minorVer);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    if (!streamOperatorCallbackReply.WriteUint32(majorVer)) {
        HDF_LOGE("%{public}s: write majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorCallbackReply.WriteUint32(minorVer)) {
        HDF_LOGE("%{public}s: write minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorCallbackRet;
}
} // V1_0
} // Camera
} // HDI
} // OHOS
