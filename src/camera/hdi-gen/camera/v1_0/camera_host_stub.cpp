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

#include "v1_0/camera_host_stub.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <hdi_support.h>
#include <iproxy_broker.h>
#include <object_collector.h>
#include <securec.h>
#include <string_ex.h>
#include "camera/v1_0/camera_device_callback_proxy.h"
#include "camera/v1_0/camera_host_callback_proxy.h"

#define HDF_LOG_TAG    camera_host_stub

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

sptr<OHOS::HDI::Camera::V1_0::ICameraHost> OHOS::HDI::Camera::V1_0::ICameraHost::Get(bool isStub)
{
    return OHOS::HDI::Camera::V1_0::ICameraHost::Get("camera_host_service", isStub);
}

sptr<OHOS::HDI::Camera::V1_0::ICameraHost> OHOS::HDI::Camera::V1_0::ICameraHost::Get(const std::string& serviceName, bool isStub)
{
    if (!isStub) {
        return nullptr;
    }
    std::string desc = Str16ToStr8(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor());
    void *impl = LoadHdiImpl(desc.c_str(), serviceName == "camera_host_service" ? "service" : serviceName.c_str());
    if (impl == nullptr) {
        HDF_LOGE("failed to load hdi impl %{public}s", desc.c_str());
        return nullptr;
    }
    return reinterpret_cast<OHOS::HDI::Camera::V1_0::ICameraHost *>(impl);
}


OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStub(const sptr<OHOS::HDI::Camera::V1_0::ICameraHost> &impl)
    : IPCObjectStub(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor()), impl_(impl)
{
}

OHOS::HDI::Camera::V1_0::CameraHostStub::~CameraHostStub()
{
    HDF_LOGI("%{public}s enter", __func__);
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option)
{
    switch (code) {
        case CMD_CAMERA_HOST_GET_VERSION:
            return CameraHostStubGetVersion(data, reply, option);
        case CMD_CAMERA_HOST_SET_CALLBACK:
            return CameraHostStubSetCallback(data, reply, option);
        case CMD_CAMERA_HOST_GET_CAMERA_IDS:
            return CameraHostStubGetCameraIds(data, reply, option);
        case CMD_CAMERA_HOST_GET_CAMERA_ABILITY:
            return CameraHostStubGetCameraAbility(data, reply, option);
        case CMD_CAMERA_HOST_OPEN_CAMERA:
            return CameraHostStubOpenCamera(data, reply, option);
        case CMD_CAMERA_HOST_SET_FLASHLIGHT:
            return CameraHostStubSetFlashlight(data, reply, option);
        default: {
            HDF_LOGE("%{public}s: cmd %{public}d is not supported", __func__, code);
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubSetCallback(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubSetCallback_(cameraHostData, cameraHostReply, cameraHostOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetCameraIds(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetCameraIds_(cameraHostData, cameraHostReply, cameraHostOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetCameraAbility(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetCameraAbility_(cameraHostData, cameraHostReply, cameraHostOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubOpenCamera(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubOpenCamera_(cameraHostData, cameraHostReply, cameraHostOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubSetFlashlight(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubSetFlashlight_(cameraHostData, cameraHostReply, cameraHostOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetVersion(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption)
{
    return OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetVersion_(cameraHostData, cameraHostReply, cameraHostOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubSetCallback_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl)
{
    if (cameraHostData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> callbackObj;
    sptr<IRemoteObject> callbackObjRemote = cameraHostData.ReadRemoteObject();
    if (callbackObjRemote == nullptr) {
        HDF_LOGE("%{public}s: read an invalid remote object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    callbackObj = new OHOS::HDI::Camera::V1_0::CameraHostCallbackProxy(callbackObjRemote);
    if (callbackObj == nullptr) {
        HDF_LOGE("%{public}s: failed to create interface object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostRet = impl->SetCallback(callbackObj);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetCameraIds_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl)
{
    if (cameraHostData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    bool cameraHostMemSet = false;
    if (!cameraHostData.ReadBool(cameraHostMemSet)) {
        HDF_LOGE("%{public}s: failed to read cameraHostMemSet", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::vector<std::string> cameraIds;
    if (cameraHostMemSet) {
        uint32_t capacity = 0;
        if (!cameraHostData.ReadUint32(capacity)) {
            HDF_LOGE("%{public}s: failed to read capacity", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        HDI_CHECK_VALUE_RETURN(capacity, >, HDI_BUFF_MAX_SIZE / sizeof(std::string), HDF_ERR_INVALID_PARAM);
        cameraIds.reserve(capacity);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostRet = impl->GetCameraIds(cameraIds);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    if (!cameraHostReply.WriteUint32(cameraIds.size())) {
        HDF_LOGE("%{public}s: write cameraIds size failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    for (const auto& it0 : cameraIds) {
        if (!cameraHostReply.WriteCString(it0.c_str())) {
            HDF_LOGE("%{public}s: write it0 failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetCameraAbility_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl)
{
    if (cameraHostData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    bool cameraHostMemSet = false;
    if (!cameraHostData.ReadBool(cameraHostMemSet)) {
        HDF_LOGE("%{public}s: failed to read cameraHostMemSet", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    const char* cameraIdCp = cameraHostData.ReadCString();
    if (cameraIdCp == nullptr) {
        HDF_LOGE("%{public}s: read cameraId failed", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::string cameraId = cameraIdCp;

    std::vector<uint8_t> cameraAbility;
    if (cameraHostMemSet) {
        uint32_t capacity = 0;
        if (!cameraHostData.ReadUint32(capacity)) {
            HDF_LOGE("%{public}s: failed to read capacity", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        HDI_CHECK_VALUE_RETURN(capacity, >, HDI_BUFF_MAX_SIZE / sizeof(uint8_t), HDF_ERR_INVALID_PARAM);
        cameraAbility.reserve(capacity);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostRet = impl->GetCameraAbility(cameraId, cameraAbility);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    if (!WritePodArray(cameraHostReply, cameraAbility)) {
        HDF_LOGE("%{public}s: failed to write cameraAbility", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubOpenCamera_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl)
{
    if (cameraHostData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    const char* cameraIdCp = cameraHostData.ReadCString();
    if (cameraIdCp == nullptr) {
        HDF_LOGE("%{public}s: read cameraId failed", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::string cameraId = cameraIdCp;

    sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> callbackObj;
    sptr<IRemoteObject> callbackObjRemote = cameraHostData.ReadRemoteObject();
    if (callbackObjRemote == nullptr) {
        HDF_LOGE("%{public}s: read an invalid remote object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    callbackObj = new OHOS::HDI::Camera::V1_0::CameraDeviceCallbackProxy(callbackObjRemote);
    if (callbackObj == nullptr) {
        HDF_LOGE("%{public}s: failed to create interface object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> device;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostRet = impl->OpenCamera(cameraId, callbackObj, device);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    if (device == nullptr) {
        HDF_LOGE("%{public}s: parameter device is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostReply.WriteRemoteObject(OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(device, OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()))) {
        HDF_LOGE("%{public}s: write device failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubSetFlashlight_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl)
{
    if (cameraHostData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    const char* cameraIdCp = cameraHostData.ReadCString();
    if (cameraIdCp == nullptr) {
        HDF_LOGE("%{public}s: read cameraId failed", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::string cameraId = cameraIdCp;

    bool isEnable = false;
    if (!cameraHostData.ReadBool(isEnable)) {
        HDF_LOGE("%{public}s: read isEnable failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostRet = impl->SetFlashlight(cameraId, isEnable);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostStub::CameraHostStubGetVersion_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl)
{
    if (cameraHostData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint32_t majorVer = 0;

    uint32_t minorVer = 0;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraHostRet = impl->GetVersion(majorVer, minorVer);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    if (!cameraHostReply.WriteUint32(majorVer)) {
        HDF_LOGE("%{public}s: write majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostReply.WriteUint32(minorVer)) {
        HDF_LOGE("%{public}s: write minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraHostRet;
}
} // V1_0
} // Camera
} // HDI
} // OHOS
