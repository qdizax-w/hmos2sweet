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

#include "v1_0/camera_device_callback_proxy.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <hdi_support.h>
#include <message_option.h>
#include <message_parcel.h>
#include <string_ex.h>

#define HDF_LOG_TAG    camera_device_callback_proxy

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
int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::OnError(OHOS::HDI::Camera::V1_0::ErrorType type,
     int32_t errorCode)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::OnError_(type, errorCode, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::OnResult(uint64_t timestamp,
     const std::vector<uint8_t>& result)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::OnResult_(timestamp, result, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::GetVersion(uint32_t& majorVer, uint32_t& minorVer)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::GetVersion_(majorVer, minorVer, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::OnError_(OHOS::HDI::Camera::V1_0::ErrorType type,
     int32_t errorCode, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceCallbackData;
    MessageParcel cameraDeviceCallbackReply;
    MessageOption cameraDeviceCallbackOption(MessageOption::TF_SYNC);

    if (!cameraDeviceCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDeviceCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceCallbackData.WriteUint64(static_cast<uint64_t>(type))) {
        HDF_LOGE("%{public}s: write type failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceCallbackData.WriteInt32(errorCode)) {
        HDF_LOGE("%{public}s: write errorCode failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceCallbackRet = remote->SendRequest(CMD_CAMERA_DEVICE_CALLBACK_ON_ERROR, cameraDeviceCallbackData, cameraDeviceCallbackReply, cameraDeviceCallbackOption);
    if (cameraDeviceCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceCallbackRet);
        return cameraDeviceCallbackRet;
    }

    return cameraDeviceCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::OnResult_(uint64_t timestamp,
     const std::vector<uint8_t>& result, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceCallbackData;
    MessageParcel cameraDeviceCallbackReply;
    MessageOption cameraDeviceCallbackOption(MessageOption::TF_SYNC);

    if (!cameraDeviceCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDeviceCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceCallbackData.WriteUint64(timestamp)) {
        HDF_LOGE("%{public}s: write timestamp failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(cameraDeviceCallbackData, result)) {
        HDF_LOGE("%{public}s: failed to write result", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceCallbackRet = remote->SendRequest(CMD_CAMERA_DEVICE_CALLBACK_ON_RESULT, cameraDeviceCallbackData, cameraDeviceCallbackReply, cameraDeviceCallbackOption);
    if (cameraDeviceCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceCallbackRet);
        return cameraDeviceCallbackRet;
    }

    return cameraDeviceCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy::GetVersion_(uint32_t& majorVer, uint32_t& minorVer,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceCallbackData;
    MessageParcel cameraDeviceCallbackReply;
    MessageOption cameraDeviceCallbackOption(MessageOption::TF_SYNC);

    if (!cameraDeviceCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDeviceCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceCallbackRet = remote->SendRequest(CMD_CAMERA_DEVICE_CALLBACK_GET_VERSION, cameraDeviceCallbackData, cameraDeviceCallbackReply, cameraDeviceCallbackOption);
    if (cameraDeviceCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceCallbackRet);
        return cameraDeviceCallbackRet;
    }

    if (!cameraDeviceCallbackReply.ReadUint32(majorVer)) {
        HDF_LOGE("%{public}s: read majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceCallbackReply.ReadUint32(minorVer)) {
        HDF_LOGE("%{public}s: read minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraDeviceCallbackRet;
}

} // V1_0
} // Camera
} // HDI
} // OHOS
