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

#include "v1_0/camera_device_callback_stub.h"
#include <hdf_base.h>
#include <hdf_core_log.h>

#define HDF_LOG_TAG    camera_device_callback_stub

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


OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStub(const sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> &impl)
    : IPCObjectStub(OHOS::HDI::Camera::V1_0::ICameraDeviceCallback::GetDescriptor()), impl_(impl)
{
}

OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::~CameraDeviceCallbackStub()
{
    HDF_LOGI("%{public}s enter", __func__);
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option)
{
    switch (code) {
        case CMD_CAMERA_DEVICE_CALLBACK_GET_VERSION:
            return CameraDeviceCallbackStubGetVersion(data, reply, option);
        case CMD_CAMERA_DEVICE_CALLBACK_ON_ERROR:
            return CameraDeviceCallbackStubOnError(data, reply, option);
        case CMD_CAMERA_DEVICE_CALLBACK_ON_RESULT:
            return CameraDeviceCallbackStubOnResult(data, reply, option);
        default: {
            HDF_LOGE("%{public}s: cmd %{public}d is not supported", __func__, code);
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubOnError(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubOnError_(cameraDeviceCallbackData, cameraDeviceCallbackReply, cameraDeviceCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubOnResult(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubOnResult_(cameraDeviceCallbackData, cameraDeviceCallbackReply, cameraDeviceCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubGetVersion(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubGetVersion_(cameraDeviceCallbackData, cameraDeviceCallbackReply, cameraDeviceCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubOnError_(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> impl)
{
    if (cameraDeviceCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDeviceCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    OHOS::HDI::Camera::V1_0::ErrorType type = static_cast<enum ErrorType>(0);
    {
        uint64_t enumTmp = 0;
        if (!cameraDeviceCallbackData.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write type failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        type = static_cast<enum ErrorType>(enumTmp);
    }

    int32_t errorCode = 0;
    if (!cameraDeviceCallbackData.ReadInt32(errorCode)) {
        HDF_LOGE("%{public}s: read errorCode failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceCallbackRet = impl->OnError(type, errorCode);
    if (cameraDeviceCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceCallbackRet);
        return cameraDeviceCallbackRet;
    }

    return cameraDeviceCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubOnResult_(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> impl)
{
    if (cameraDeviceCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDeviceCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint64_t timestamp = 0;
    if (!cameraDeviceCallbackData.ReadUint64(timestamp)) {
        HDF_LOGE("%{public}s: read timestamp failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<uint8_t> result;
    if (!ReadPodArray(cameraDeviceCallbackData, result)) {
        HDF_LOGE("%{public}s: failed to read result", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceCallbackRet = impl->OnResult(timestamp, result);
    if (cameraDeviceCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceCallbackRet);
        return cameraDeviceCallbackRet;
    }

    return cameraDeviceCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub::CameraDeviceCallbackStubGetVersion_(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> impl)
{
    if (cameraDeviceCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDeviceCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint32_t majorVer = 0;

    uint32_t minorVer = 0;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceCallbackRet = impl->GetVersion(majorVer, minorVer);
    if (cameraDeviceCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceCallbackRet);
        return cameraDeviceCallbackRet;
    }

    if (!cameraDeviceCallbackReply.WriteUint32(majorVer)) {
        HDF_LOGE("%{public}s: write majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceCallbackReply.WriteUint32(minorVer)) {
        HDF_LOGE("%{public}s: write minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraDeviceCallbackRet;
}
} // V1_0
} // Camera
} // HDI
} // OHOS
