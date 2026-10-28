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

#include "v1_0/camera_host_callback_stub.h"
#include <hdf_base.h>
#include <hdf_core_log.h>

#define HDF_LOG_TAG    camera_host_callback_stub

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {



OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStub(const sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> &impl)
    : IPCObjectStub(OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor()), impl_(impl)
{
}

OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::~CameraHostCallbackStub()
{
    HDF_LOGI("%{public}s enter", __func__);
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option)
{
    switch (code) {
        case CMD_CAMERA_HOST_CALLBACK_GET_VERSION:
            return CameraHostCallbackStubGetVersion(data, reply, option);
        case CMD_CAMERA_HOST_CALLBACK_ON_CAMERA_STATUS:
            return CameraHostCallbackStubOnCameraStatus(data, reply, option);
        case CMD_CAMERA_HOST_CALLBACK_ON_FLASHLIGHT_STATUS:
            return CameraHostCallbackStubOnFlashlightStatus(data, reply, option);
        case CMD_CAMERA_HOST_CALLBACK_ON_CAMERA_EVENT:
            return CameraHostCallbackStubOnCameraEvent(data, reply, option);
        default: {
            HDF_LOGE("%{public}s: cmd %{public}d is not supported", __func__, code);
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnCameraStatus(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnCameraStatus_(cameraHostCallbackData, cameraHostCallbackReply, cameraHostCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnFlashlightStatus(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnFlashlightStatus_(cameraHostCallbackData, cameraHostCallbackReply, cameraHostCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnCameraEvent(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnCameraEvent_(cameraHostCallbackData, cameraHostCallbackReply, cameraHostCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubGetVersion(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubGetVersion_(cameraHostCallbackData, cameraHostCallbackReply, cameraHostCallbackOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnCameraStatus_(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl)
{
    if (cameraHostCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    const char* cameraIdCp = cameraHostCallbackData.ReadCString();
    if (cameraIdCp == nullptr) {
        HDF_LOGE("%{public}s: read cameraId failed", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::string cameraId = cameraIdCp;

    OHOS::HDI::Camera::V1_0::CameraStatus status = static_cast<enum CameraStatus>(0);
    {
        uint64_t enumTmp = 0;
        if (!cameraHostCallbackData.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write status failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        status = static_cast<enum CameraStatus>(enumTmp);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostCallbackRet = impl->OnCameraStatus(cameraId, status);
    if (cameraHostCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostCallbackRet);
        return cameraHostCallbackRet;
    }

    return cameraHostCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnFlashlightStatus_(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl)
{
    if (cameraHostCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    const char* cameraIdCp = cameraHostCallbackData.ReadCString();
    if (cameraIdCp == nullptr) {
        HDF_LOGE("%{public}s: read cameraId failed", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::string cameraId = cameraIdCp;

    OHOS::HDI::Camera::V1_0::FlashlightStatus status = static_cast<enum FlashlightStatus>(0);
    {
        uint64_t enumTmp = 0;
        if (!cameraHostCallbackData.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write status failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        status = static_cast<enum FlashlightStatus>(enumTmp);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostCallbackRet = impl->OnFlashlightStatus(cameraId, status);
    if (cameraHostCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostCallbackRet);
        return cameraHostCallbackRet;
    }

    return cameraHostCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubOnCameraEvent_(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl)
{
    if (cameraHostCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    const char* cameraIdCp = cameraHostCallbackData.ReadCString();
    if (cameraIdCp == nullptr) {
        HDF_LOGE("%{public}s: read cameraId failed", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::string cameraId = cameraIdCp;

    OHOS::HDI::Camera::V1_0::CameraEvent event = static_cast<enum CameraEvent>(0);
    {
        uint64_t enumTmp = 0;
        if (!cameraHostCallbackData.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write event failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        event = static_cast<enum CameraEvent>(enumTmp);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostCallbackRet = impl->OnCameraEvent(cameraId, event);
    if (cameraHostCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostCallbackRet);
        return cameraHostCallbackRet;
    }

    return cameraHostCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostCallbackStub::CameraHostCallbackStubGetVersion_(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl)
{
    if (cameraHostCallbackData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint32_t majorVer = 0;

    uint32_t minorVer = 0;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostCallbackRet = impl->GetVersion(majorVer, minorVer);
    if (cameraHostCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostCallbackRet);
        return cameraHostCallbackRet;
    }

    if (!cameraHostCallbackReply.WriteUint32(majorVer)) {
        HDF_LOGE("%{public}s: write majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostCallbackReply.WriteUint32(minorVer)) {
        HDF_LOGE("%{public}s: write minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraHostCallbackRet;
}
} // V1_0
} // Camera
} // HDI
} // OHOS
