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

#include "v1_0/camera_device_stub.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <iproxy_broker.h>
#include <object_collector.h>
#include "camera/v1_0/stream_operator_callback_proxy.h"

#define HDF_LOG_TAG    camera_device_stub

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {
template<typename ElementType>
static bool WritePodArray(MessageParcel &parcel, const std::vector<ElementType> &data);
template<typename ElementType>
static bool ReadPodArray(MessageParcel &parcel, std::vector<ElementType> &data);


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


OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStub(const sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> &impl)
    : IPCObjectStub(OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()), impl_(impl)
{
}

OHOS::HDI::Camera::V1_0::CameraDeviceStub::~CameraDeviceStub()
{
    HDF_LOGI("%{public}s enter", __func__);
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option)
{
    switch (code) {
        case CMD_CAMERA_DEVICE_GET_VERSION:
            return CameraDeviceStubGetVersion(data, reply, option);
        case CMD_CAMERA_DEVICE_GET_STREAM_OPERATOR:
            return CameraDeviceStubGetStreamOperator(data, reply, option);
        case CMD_CAMERA_DEVICE_UPDATE_SETTINGS:
            return CameraDeviceStubUpdateSettings(data, reply, option);
        case CMD_CAMERA_DEVICE_SET_RESULT_MODE:
            return CameraDeviceStubSetResultMode(data, reply, option);
        case CMD_CAMERA_DEVICE_GET_ENABLED_RESULTS:
            return CameraDeviceStubGetEnabledResults(data, reply, option);
        case CMD_CAMERA_DEVICE_ENABLE_RESULT:
            return CameraDeviceStubEnableResult(data, reply, option);
        case CMD_CAMERA_DEVICE_DISABLE_RESULT:
            return CameraDeviceStubDisableResult(data, reply, option);
        case CMD_CAMERA_DEVICE_CLOSE:
            return CameraDeviceStubClose(data, reply, option);
        default: {
            HDF_LOGE("%{public}s: cmd %{public}d is not supported", __func__, code);
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetStreamOperator(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetStreamOperator_(cameraDeviceData, cameraDeviceReply, cameraDeviceOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubUpdateSettings(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubUpdateSettings_(cameraDeviceData, cameraDeviceReply, cameraDeviceOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubSetResultMode(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubSetResultMode_(cameraDeviceData, cameraDeviceReply, cameraDeviceOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetEnabledResults(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetEnabledResults_(cameraDeviceData, cameraDeviceReply, cameraDeviceOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubEnableResult(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubEnableResult_(cameraDeviceData, cameraDeviceReply, cameraDeviceOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubDisableResult(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubDisableResult_(cameraDeviceData, cameraDeviceReply, cameraDeviceOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubClose(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubClose_(cameraDeviceData, cameraDeviceReply, cameraDeviceOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetVersion(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption)
{
    return OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetVersion_(cameraDeviceData, cameraDeviceReply, cameraDeviceOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetStreamOperator_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl)
{
    if (cameraDeviceData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> callbackObj;
    sptr<IRemoteObject> callbackObjRemote = cameraDeviceData.ReadRemoteObject();
    if (callbackObjRemote == nullptr) {
        HDF_LOGE("%{public}s: read an invalid remote object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    callbackObj = new OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy(callbackObjRemote);
    if (callbackObj == nullptr) {
        HDF_LOGE("%{public}s: failed to create interface object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> streamOperator;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceRet = impl->GetStreamOperator(callbackObj, streamOperator);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    if (streamOperator == nullptr) {
        HDF_LOGE("%{public}s: parameter streamOperator is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceReply.WriteRemoteObject(OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(streamOperator, OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()))) {
        HDF_LOGE("%{public}s: write streamOperator failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubUpdateSettings_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl)
{
    if (cameraDeviceData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<uint8_t> settings;
    if (!ReadPodArray(cameraDeviceData, settings)) {
        HDF_LOGE("%{public}s: failed to read settings", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceRet = impl->UpdateSettings(settings);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubSetResultMode_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl)
{
    if (cameraDeviceData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    OHOS::HDI::Camera::V1_0::ResultCallbackMode mode = static_cast<enum ResultCallbackMode>(0);
    {
        uint64_t enumTmp = 0;
        if (!cameraDeviceData.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write mode failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        mode = static_cast<enum ResultCallbackMode>(enumTmp);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceRet = impl->SetResultMode(mode);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetEnabledResults_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl)
{
    if (cameraDeviceData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    bool cameraDeviceMemSet = false;
    if (!cameraDeviceData.ReadBool(cameraDeviceMemSet)) {
        HDF_LOGE("%{public}s: failed to read cameraDeviceMemSet", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::vector<int32_t> results;
    if (cameraDeviceMemSet) {
        uint32_t capacity = 0;
        if (!cameraDeviceData.ReadUint32(capacity)) {
            HDF_LOGE("%{public}s: failed to read capacity", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        HDI_CHECK_VALUE_RETURN(capacity, >, HDI_BUFF_MAX_SIZE / sizeof(int32_t), HDF_ERR_INVALID_PARAM);
        results.reserve(capacity);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceRet = impl->GetEnabledResults(results);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    if (!WritePodArray(cameraDeviceReply, results)) {
        HDF_LOGE("%{public}s: failed to write results", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubEnableResult_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl)
{
    if (cameraDeviceData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<int32_t> results;
    if (!ReadPodArray(cameraDeviceData, results)) {
        HDF_LOGE("%{public}s: failed to read results", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceRet = impl->EnableResult(results);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubDisableResult_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl)
{
    if (cameraDeviceData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<int32_t> results;
    if (!ReadPodArray(cameraDeviceData, results)) {
        HDF_LOGE("%{public}s: failed to read results", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceRet = impl->DisableResult(results);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubClose_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl)
{
    if (cameraDeviceData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceRet = impl->Close();
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    return cameraDeviceRet;
}

int32_t OHOS::HDI::Camera::V1_0::CameraDeviceStub::CameraDeviceStubGetVersion_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl)
{
    if (cameraDeviceData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::ICameraDevice::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint32_t majorVer = 0;

    uint32_t minorVer = 0;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t cameraDeviceRet = impl->GetVersion(majorVer, minorVer);
    if (cameraDeviceRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, cameraDeviceRet);
        return cameraDeviceRet;
    }

    if (!cameraDeviceReply.WriteUint32(majorVer)) {
        HDF_LOGE("%{public}s: write majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!cameraDeviceReply.WriteUint32(minorVer)) {
        HDF_LOGE("%{public}s: write minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return cameraDeviceRet;
}
} // V1_0
} // Camera
} // HDI
} // OHOS
