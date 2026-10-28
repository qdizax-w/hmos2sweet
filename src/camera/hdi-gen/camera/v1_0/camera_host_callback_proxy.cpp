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

#include "v1_0/camera_host_callback_proxy.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <hdi_support.h>
#include <message_option.h>
#include <message_parcel.h>
#include <string_ex.h>

#define HDF_LOG_TAG    camera_host_callback_proxy

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {


int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnCameraStatus(const std::string& cameraId,
     OHOS::HDI::Camera::V1_0::CameraStatus status)
{
    return OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnCameraStatus_(cameraId, status, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnFlashlightStatus(const std::string& cameraId,
     OHOS::HDI::Camera::V1_0::FlashlightStatus status)
{
    return OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnFlashlightStatus_(cameraId, status, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnCameraEvent(const std::string& cameraId,
     OHOS::HDI::Camera::V1_0::CameraEvent event)
{
    return OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnCameraEvent_(cameraId, event, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::GetVersion(uint32_t& majorVer, uint32_t& minorVer)
{
    return OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::GetVersion_(majorVer, minorVer, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnCameraStatus_(const std::string& cameraId,
     OHOS::HDI::Camera::V1_0::CameraStatus status, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostCallbackData;
    MessageParcel cameraHostCallbackReply;
    MessageOption cameraHostCallbackOption(MessageOption::TF_SYNC);

    if (!cameraHostCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostCallbackData.WriteCString(cameraId.c_str())) {
        HDF_LOGE("%{public}s: write cameraId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostCallbackData.WriteUint64(static_cast<uint64_t>(status))) {
        HDF_LOGE("%{public}s: write status failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostCallbackRet = remote->SendRequest(CMD_CAMERA_HOST_CALLBACK_ON_CAMERA_STATUS, cameraHostCallbackData, cameraHostCallbackReply, cameraHostCallbackOption);
    if (cameraHostCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostCallbackRet);
        return cameraHostCallbackRet;
    }

    return cameraHostCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnFlashlightStatus_(const std::string& cameraId,
     OHOS::HDI::Camera::V1_0::FlashlightStatus status, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostCallbackData;
    MessageParcel cameraHostCallbackReply;
    MessageOption cameraHostCallbackOption(MessageOption::TF_SYNC);

    if (!cameraHostCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostCallbackData.WriteCString(cameraId.c_str())) {
        HDF_LOGE("%{public}s: write cameraId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostCallbackData.WriteUint64(static_cast<uint64_t>(status))) {
        HDF_LOGE("%{public}s: write status failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostCallbackRet = remote->SendRequest(CMD_CAMERA_HOST_CALLBACK_ON_FLASHLIGHT_STATUS, cameraHostCallbackData, cameraHostCallbackReply, cameraHostCallbackOption);
    if (cameraHostCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostCallbackRet);
        return cameraHostCallbackRet;
    }

    return cameraHostCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::OnCameraEvent_(const std::string& cameraId,
     OHOS::HDI::Camera::V1_0::CameraEvent event, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostCallbackData;
    MessageParcel cameraHostCallbackReply;
    MessageOption cameraHostCallbackOption(MessageOption::TF_SYNC);

    if (!cameraHostCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostCallbackData.WriteCString(cameraId.c_str())) {
        HDF_LOGE("%{public}s: write cameraId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostCallbackData.WriteUint64(static_cast<uint64_t>(event))) {
        HDF_LOGE("%{public}s: write event failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostCallbackRet = remote->SendRequest(CMD_CAMERA_HOST_CALLBACK_ON_CAMERA_EVENT, cameraHostCallbackData, cameraHostCallbackReply, cameraHostCallbackOption);
    if (cameraHostCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostCallbackRet);
        return cameraHostCallbackRet;
    }

    return cameraHostCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy::GetVersion_(uint32_t& majorVer, uint32_t& minorVer,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostCallbackData;
    MessageParcel cameraHostCallbackReply;
    MessageOption cameraHostCallbackOption(MessageOption::TF_SYNC);

    if (!cameraHostCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostCallbackRet = remote->SendRequest(CMD_CAMERA_HOST_CALLBACK_GET_VERSION, cameraHostCallbackData, cameraHostCallbackReply, cameraHostCallbackOption);
    if (cameraHostCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostCallbackRet);
        return cameraHostCallbackRet;
    }

    if (!cameraHostCallbackReply.ReadUint32(majorVer)) {
        HDF_LOGE("%{public}s: read majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostCallbackReply.ReadUint32(minorVer)) {
        HDF_LOGE("%{public}s: read minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraHostCallbackRet;
}

} // V1_0
} // Camera
} // HDI
} // OHOS
