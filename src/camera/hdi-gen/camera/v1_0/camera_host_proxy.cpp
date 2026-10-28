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

#include "v1_0/camera_host_proxy.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <hdi_support.h>
#include <iproxy_broker.h>
#include <iservmgr_hdi.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <string_ex.h>
#include "camera/v1_0/camera_device_proxy.h"

#define HDF_LOG_TAG    camera_host_proxy

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

template<typename ElementType>
static bool ReadPodArray(MessageParcel &parcel, std::vector<ElementType> &data);

sptr<OHOS::HDI::Camera::V1_0::ICameraHost> OHOS::HDI::Camera::V1_0::ICameraHost::Get(bool isStub)
{
    return ICameraHost::Get("camera_host_service", isStub);
}

sptr<OHOS::HDI::Camera::V1_0::ICameraHost> OHOS::HDI::Camera::V1_0::ICameraHost::Get(const std::string& serviceName, bool isStub)
{
    if (isStub) {
        std::string desc = Str16ToStr8(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor());
        void *impl = LoadHdiImpl(desc.c_str(), serviceName == "camera_host_service" ? "service" : serviceName.c_str());
        if (impl == nullptr) {
            HDF_LOGE("failed to load hdi impl %{public}s", desc.data());
            return nullptr;
        }
        return reinterpret_cast<OHOS::HDI::Camera::V1_0::ICameraHost *>(impl);
    }

    using namespace OHOS::HDI::ServiceManager::V1_0;
    auto servMgr = IServiceManager::Get();
    if (servMgr == nullptr) {
        HDF_LOGE("%{public}s:get IServiceManager failed!", __func__);
        return nullptr;
    }

    sptr<IRemoteObject> remote = servMgr->GetService(serviceName.c_str());
    if (remote == nullptr) {
        HDF_LOGE("%{public}s:get remote object failed!", __func__);
        return nullptr;
    }

    sptr<OHOS::HDI::Camera::V1_0::CameraHostProxy> proxy = new OHOS::HDI::Camera::V1_0::CameraHostProxy(remote);
    if (proxy == nullptr) {
        HDF_LOGE("%{public}s:iface_cast failed!", __func__);
        return nullptr;
    }

    proxy->servMgr_ = OHOS::HDI::hdi_objcast<IServiceManager>(servMgr);
    proxy->deathRecipient_ = new OHOS::HDI::Camera::V1_0::CameraHostProxy::IServiceManagerDeathRecipient(proxy);
    proxy->servMgr_->AddDeathRecipient(proxy->deathRecipient_);
    proxy->isReconnected_ = false;
    proxy->serviceName_ = serviceName;
    uint32_t serMajorVer = 0;
    uint32_t serMinorVer = 0;
    int32_t cameraHostRet = proxy->GetVersion(serMajorVer, serMinorVer);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:get version failed!", __func__);
        return nullptr;
    }

    if (serMajorVer != 1) {
        HDF_LOGE("%{public}s:check version failed! version of service:%u.%u, version of client:1.0", __func__, serMajorVer, serMinorVer);
        return nullptr;
    }

    return proxy;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::Reconnect(
    sptr<OHOS::HDI::Camera::V1_0::CameraHostProxy> proxy) 
{
    if (proxy == nullptr) {
        HDF_LOGW("Reconnect failed : input proxy is null");
        return HDF_FAILURE;
    }
    using namespace OHOS::HDI::ServiceManager::V1_0;
    proxy->isReconnected_ = false;
    auto iServMgr = IServiceManager::Get();
    if (iServMgr == nullptr) {
        HDF_LOGW("Reconnect failed : iServMgr is null");
        return HDF_FAILURE;
    };
    proxy->reconnectRemote_ = iServMgr->GetService(proxy->serviceName_.c_str());
    if (proxy->reconnectRemote_ == nullptr) {
        HDF_LOGW("Reconnect failed : reconnectRemote_ is null");
        return HDF_FAILURE;
    }
    proxy->servMgr_ = OHOS::HDI::hdi_objcast<IServiceManager>(iServMgr);
    if (proxy->servMgr_ == nullptr) {
        HDF_LOGE("%{public}s:get IServiceManager failed!", __func__);
        return HDF_FAILURE;
    }
    proxy->servMgr_->AddDeathRecipient(
        new OHOS::HDI::Camera::V1_0::CameraHostProxy::IServiceManagerDeathRecipient(proxy));
    proxy->isReconnected_ = true;
    return HDF_SUCCESS;
}


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
int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::SetCallback(const sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback>& callbackObj)
{
    return OHOS::HDI::Camera::V1_0::CameraHostProxy::SetCallback_(callbackObj, GetCurrentRemote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::GetCameraIds(std::vector<std::string>& cameraIds)
{
    return OHOS::HDI::Camera::V1_0::CameraHostProxy::GetCameraIds_(cameraIds, GetCurrentRemote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::GetCameraAbility(const std::string& cameraId,
     std::vector<uint8_t>& cameraAbility)
{
    return OHOS::HDI::Camera::V1_0::CameraHostProxy::GetCameraAbility_(cameraId, cameraAbility, GetCurrentRemote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::OpenCamera(const std::string& cameraId,
     const sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice>& device)
{
    return OHOS::HDI::Camera::V1_0::CameraHostProxy::OpenCamera_(cameraId, callbackObj, device, GetCurrentRemote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::SetFlashlight(const std::string& cameraId, bool isEnable)
{
    return OHOS::HDI::Camera::V1_0::CameraHostProxy::SetFlashlight_(cameraId, isEnable, GetCurrentRemote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::GetVersion(uint32_t& majorVer, uint32_t& minorVer)
{
    return OHOS::HDI::Camera::V1_0::CameraHostProxy::GetVersion_(majorVer, minorVer, GetCurrentRemote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::SetCallback_(const sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback>& callbackObj, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostData;
    MessageParcel cameraHostReply;
    MessageOption cameraHostOption(MessageOption::TF_SYNC);

    if (!cameraHostData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (callbackObj == nullptr) {
        HDF_LOGE("%{public}s: parameter callbackObj is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostData.WriteRemoteObject(OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(callbackObj, OHOS::HDI::Camera::V1_0::ICameraHostCallback::GetDescriptor()))) {
        HDF_LOGE("%{public}s: write callbackObj failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostRet = remote->SendRequest(CMD_CAMERA_HOST_SET_CALLBACK, cameraHostData, cameraHostReply, cameraHostOption);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::GetCameraIds_(std::vector<std::string>& cameraIds,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostData;
    MessageParcel cameraHostReply;
    MessageOption cameraHostOption(MessageOption::TF_SYNC);

    if (!cameraHostData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostData.WriteBool(false)) {
        HDF_LOGE("%{public}s:failed to write flag of memory setting!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostRet = remote->SendRequest(CMD_CAMERA_HOST_GET_CAMERA_IDS, cameraHostData, cameraHostReply, cameraHostOption);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    uint32_t cameraIdsSize = 0;
    if (!cameraHostReply.ReadUint32(cameraIdsSize)) {
        HDF_LOGE("%{public}s: failed to read size", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    HDI_CHECK_VALUE_RETURN(cameraIdsSize, >, HDI_BUFF_MAX_SIZE / sizeof(std::string), HDF_ERR_INVALID_PARAM);
    cameraIds.clear();
    cameraIds.reserve(cameraIdsSize);
    for (uint32_t i0 = 0; i0 < cameraIdsSize; ++i0) {
        const char* value0Cp = cameraHostReply.ReadCString();
        if (value0Cp == nullptr) {
            HDF_LOGE("%{public}s: read value0 failed", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        std::string value0 = value0Cp;
        cameraIds.push_back(value0);
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::GetCameraAbility_(const std::string& cameraId,
     std::vector<uint8_t>& cameraAbility, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostData;
    MessageParcel cameraHostReply;
    MessageOption cameraHostOption(MessageOption::TF_SYNC);

    if (!cameraHostData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostData.WriteBool(false)) {
        HDF_LOGE("%{public}s:failed to write flag of memory setting!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostData.WriteCString(cameraId.c_str())) {
        HDF_LOGE("%{public}s: write cameraId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostRet = remote->SendRequest(CMD_CAMERA_HOST_GET_CAMERA_ABILITY, cameraHostData, cameraHostReply, cameraHostOption);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    if (!ReadPodArray(cameraHostReply, cameraAbility)) {
        HDF_LOGE("%{public}s: failed to read cameraAbility", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::OpenCamera_(const std::string& cameraId,
     const sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice>& device, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostData;
    MessageParcel cameraHostReply;
    MessageOption cameraHostOption(MessageOption::TF_SYNC);

    if (!cameraHostData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostData.WriteCString(cameraId.c_str())) {
        HDF_LOGE("%{public}s: write cameraId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (callbackObj == nullptr) {
        HDF_LOGE("%{public}s: parameter callbackObj is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostData.WriteRemoteObject(OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(callbackObj, OHOS::HDI::Camera::V1_0::ICameraDeviceCallback::GetDescriptor()))) {
        HDF_LOGE("%{public}s: write callbackObj failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostRet = remote->SendRequest(CMD_CAMERA_HOST_OPEN_CAMERA, cameraHostData, cameraHostReply, cameraHostOption);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    sptr<IRemoteObject> deviceRemote = cameraHostReply.ReadRemoteObject();
    if (deviceRemote == nullptr) {
        HDF_LOGE("%{public}s: read an invalid remote object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    device = new OHOS::HDI::Camera::V1_0::CameraDeviceProxy(deviceRemote);
    if (device == nullptr) {
        HDF_LOGE("%{public}s: failed to create interface object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::SetFlashlight_(const std::string& cameraId, bool isEnable,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostData;
    MessageParcel cameraHostReply;
    MessageOption cameraHostOption(MessageOption::TF_SYNC);

    if (!cameraHostData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostData.WriteCString(cameraId.c_str())) {
        HDF_LOGE("%{public}s: write cameraId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostData.WriteBool(isEnable)) {
        HDF_LOGE("%{public}s: write isEnable failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostRet = remote->SendRequest(CMD_CAMERA_HOST_SET_FLASHLIGHT, cameraHostData, cameraHostReply, cameraHostOption);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    return cameraHostRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraHostProxy::GetVersion_(uint32_t& majorVer, uint32_t& minorVer,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraHostData;
    MessageParcel cameraHostReply;
    MessageOption cameraHostOption(MessageOption::TF_SYNC);

    if (!cameraHostData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraHost::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraHostRet = remote->SendRequest(CMD_CAMERA_HOST_GET_VERSION, cameraHostData, cameraHostReply, cameraHostOption);
    if (cameraHostRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraHostRet);
        return cameraHostRet;
    }

    if (!cameraHostReply.ReadUint32(majorVer)) {
        HDF_LOGE("%{public}s: read majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraHostReply.ReadUint32(minorVer)) {
        HDF_LOGE("%{public}s: read minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraHostRet;
}

} // V1_0
} // Camera
} // HDI
} // OHOS
