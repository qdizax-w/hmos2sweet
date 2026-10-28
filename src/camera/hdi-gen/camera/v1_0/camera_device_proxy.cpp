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

#include "v1_0/camera_device_proxy.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <hdi_support.h>
#include <iproxy_broker.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <string_ex.h>
#include "camera/v1_0/stream_operator_proxy.h"

#define HDF_LOG_TAG    camera_device_proxy

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

template<typename ElementType>
static bool ReadPodArray(MessageParcel &parcel, std::vector<ElementType> &data);
template<typename ElementType>
static bool WritePodArray(MessageParcel &parcel, const std::vector<ElementType> &data);


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
int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetStreamOperator(const sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator>& streamOperator)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetStreamOperator_(callbackObj, streamOperator, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::UpdateSettings(const std::vector<uint8_t>& settings)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceProxy::UpdateSettings_(settings, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::SetResultMode(OHOS::HDI::Camera::V1_0::ResultCallbackMode mode)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceProxy::SetResultMode_(mode, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetEnabledResults(std::vector<int32_t>& results)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetEnabledResults_(results, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::EnableResult(const std::vector<int32_t>& results)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceProxy::EnableResult_(results, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::DisableResult(const std::vector<int32_t>& results)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceProxy::DisableResult_(results, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::Close()
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceProxy::Close_(Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetVersion(uint32_t& majorVer, uint32_t& minorVer)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetVersion_(majorVer, minorVer, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetStreamOperator_(const sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator>& streamOperator, const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceData;
    MessageParcel cameraDeviceReply;
    MessageOption cameraDeviceOption(MessageOption::TF_SYNC);

    if (!cameraDeviceData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (callbackObj == nullptr) {
        HDF_LOGE("%{public}s: parameter callbackObj is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceData.WriteRemoteObject(OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(callbackObj, OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor()))) {
        HDF_LOGE("%{public}s: write callbackObj failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceRet = remote->SendRequest(CMD_CAMERA_DEVICE_GET_STREAM_OPERATOR, cameraDeviceData, cameraDeviceReply, cameraDeviceOption);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    sptr<IRemoteObject> streamOperatorRemote = cameraDeviceReply.ReadRemoteObject();
    if (streamOperatorRemote == nullptr) {
        HDF_LOGE("%{public}s: read an invalid remote object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    streamOperator = new OHOS::HDI::Camera::V1_0::StreamOperatorProxy(streamOperatorRemote);
    if (streamOperator == nullptr) {
        HDF_LOGE("%{public}s: failed to create interface object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::UpdateSettings_(const std::vector<uint8_t>& settings,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceData;
    MessageParcel cameraDeviceReply;
    MessageOption cameraDeviceOption(MessageOption::TF_SYNC);

    if (!cameraDeviceData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(cameraDeviceData, settings)) {
        HDF_LOGE("%{public}s: failed to write settings", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceRet = remote->SendRequest(CMD_CAMERA_DEVICE_UPDATE_SETTINGS, cameraDeviceData, cameraDeviceReply, cameraDeviceOption);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::SetResultMode_(OHOS::HDI::Camera::V1_0::ResultCallbackMode mode,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceData;
    MessageParcel cameraDeviceReply;
    MessageOption cameraDeviceOption(MessageOption::TF_SYNC);

    if (!cameraDeviceData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceData.WriteUint64(static_cast<uint64_t>(mode))) {
        HDF_LOGE("%{public}s: write mode failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceRet = remote->SendRequest(CMD_CAMERA_DEVICE_SET_RESULT_MODE, cameraDeviceData, cameraDeviceReply, cameraDeviceOption);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetEnabledResults_(std::vector<int32_t>& results,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceData;
    MessageParcel cameraDeviceReply;
    MessageOption cameraDeviceOption(MessageOption::TF_SYNC);

    if (!cameraDeviceData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceData.WriteBool(false)) {
        HDF_LOGE("%{public}s:failed to write flag of memory setting!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceRet = remote->SendRequest(CMD_CAMERA_DEVICE_GET_ENABLED_RESULTS, cameraDeviceData, cameraDeviceReply, cameraDeviceOption);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    if (!ReadPodArray(cameraDeviceReply, results)) {
        HDF_LOGE("%{public}s: failed to read results", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::EnableResult_(const std::vector<int32_t>& results,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceData;
    MessageParcel cameraDeviceReply;
    MessageOption cameraDeviceOption(MessageOption::TF_SYNC);

    if (!cameraDeviceData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(cameraDeviceData, results)) {
        HDF_LOGE("%{public}s: failed to write results", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceRet = remote->SendRequest(CMD_CAMERA_DEVICE_ENABLE_RESULT, cameraDeviceData, cameraDeviceReply, cameraDeviceOption);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::DisableResult_(const std::vector<int32_t>& results,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceData;
    MessageParcel cameraDeviceReply;
    MessageOption cameraDeviceOption(MessageOption::TF_SYNC);

    if (!cameraDeviceData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(cameraDeviceData, results)) {
        HDF_LOGE("%{public}s: failed to write results", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceRet = remote->SendRequest(CMD_CAMERA_DEVICE_DISABLE_RESULT, cameraDeviceData, cameraDeviceReply, cameraDeviceOption);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::Close_(const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceData;
    MessageParcel cameraDeviceReply;
    MessageOption cameraDeviceOption(MessageOption::TF_SYNC);

    if (!cameraDeviceData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceRet = remote->SendRequest(CMD_CAMERA_DEVICE_CLOSE, cameraDeviceData, cameraDeviceReply, cameraDeviceOption);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceProxy::GetVersion_(uint32_t& majorVer, uint32_t& minorVer,
     const sptr<IRemoteObject> remote)
{
    MessageParcel cameraDeviceData;
    MessageParcel cameraDeviceReply;
    MessageOption cameraDeviceOption(MessageOption::TF_SYNC);

    if (!cameraDeviceData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t cameraDeviceRet = remote->SendRequest(CMD_CAMERA_DEVICE_GET_VERSION, cameraDeviceData, cameraDeviceReply, cameraDeviceOption);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    if (!cameraDeviceReply.ReadUint32(majorVer)) {
        HDF_LOGE("%{public}s: read majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceReply.ReadUint32(minorVer)) {
        HDF_LOGE("%{public}s: read minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraDeviceRet;
}

} // V1_0
} // Camera
} // HDI
} // OHOS
