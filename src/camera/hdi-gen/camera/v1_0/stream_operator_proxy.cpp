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

#include "v1_0/stream_operator_proxy.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <hdi_support.h>
#include <iproxy_broker.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <string_ex.h>
#include "camera/v1_0/offline_stream_operator_proxy.h"

#define HDF_LOG_TAG    stream_operator_proxy

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
int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::IsStreamsSupported(OHOS::HDI::Camera::V1_0::OperationMode mode,
     const std::vector<uint8_t>& modeSetting, const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& infos, OHOS::HDI::Camera::V1_0::StreamSupportType& type)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::IsStreamsSupported_(mode, modeSetting, infos, type, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CreateStreams(const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& streamInfos)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CreateStreams_(streamInfos, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::ReleaseStreams(const std::vector<int32_t>& streamIds)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::ReleaseStreams_(streamIds, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CommitStreams(OHOS::HDI::Camera::V1_0::OperationMode mode,
     const std::vector<uint8_t>& modeSetting)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CommitStreams_(mode, modeSetting, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::GetStreamAttributes(std::vector<OHOS::HDI::Camera::V1_0::StreamAttribute>& attributes)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::GetStreamAttributes_(attributes, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::AttachBufferQueue(int32_t streamId,
     const sptr<OHOS::HDI::Camera::V1_0::BufferProducerSequenceable>& bufferProducer)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::AttachBufferQueue_(streamId, bufferProducer, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::DetachBufferQueue(int32_t streamId)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::DetachBufferQueue_(streamId, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::Capture(int32_t captureId,
     const OHOS::HDI::Camera::V1_0::CaptureInfo& info, bool isStreaming)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::Capture_(captureId, info, isStreaming, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CancelCapture(int32_t captureId)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CancelCapture_(captureId, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::ChangeToOfflineStream(const std::vector<int32_t>& streamIds,
     const sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator>& offlineOperator)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::ChangeToOfflineStream_(streamIds, callbackObj, offlineOperator, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::GetVersion(uint32_t& majorVer, uint32_t& minorVer)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorProxy::GetVersion_(majorVer, minorVer, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::IsStreamsSupported_(OHOS::HDI::Camera::V1_0::OperationMode mode,
     const std::vector<uint8_t>& modeSetting, const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& infos, OHOS::HDI::Camera::V1_0::StreamSupportType& type, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteUint64(static_cast<uint64_t>(mode))) {
        HDF_LOGE("%{public}s: write mode failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(streamOperatorData, modeSetting)) {
        HDF_LOGE("%{public}s: failed to write modeSetting", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteUint32(infos.size())) {
        HDF_LOGE("%{public}s: write infos size failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    for (const auto& it0 : infos) {
        if (!OHOS::HDI::Camera::V1_0::StreamInfoBlockMarshalling(streamOperatorData, it0)) {
            HDF_LOGE("%{public}s: write it0 failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_IS_STREAMS_SUPPORTED, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    {
        uint64_t enumTmp = 0;
        if (!streamOperatorReply.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write type failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        type = static_cast<enum StreamSupportType>(enumTmp);
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CreateStreams_(const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& streamInfos, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteUint32(streamInfos.size())) {
        HDF_LOGE("%{public}s: write streamInfos size failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    for (const auto& it0 : streamInfos) {
        if (!OHOS::HDI::Camera::V1_0::StreamInfoBlockMarshalling(streamOperatorData, it0)) {
            HDF_LOGE("%{public}s: write it0 failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_CREATE_STREAMS, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::ReleaseStreams_(const std::vector<int32_t>& streamIds,
     const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(streamOperatorData, streamIds)) {
        HDF_LOGE("%{public}s: failed to write streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_RELEASE_STREAMS, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CommitStreams_(OHOS::HDI::Camera::V1_0::OperationMode mode,
     const std::vector<uint8_t>& modeSetting, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteUint64(static_cast<uint64_t>(mode))) {
        HDF_LOGE("%{public}s: write mode failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(streamOperatorData, modeSetting)) {
        HDF_LOGE("%{public}s: failed to write modeSetting", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_COMMIT_STREAMS, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::GetStreamAttributes_(std::vector<OHOS::HDI::Camera::V1_0::StreamAttribute>& attributes, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteBool(false)) {
        HDF_LOGE("%{public}s:failed to write flag of memory setting!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_GET_STREAM_ATTRIBUTES, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    if (!ReadPodArray(streamOperatorReply, attributes)) {
        HDF_LOGE("%{public}s: failed to read attributes", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::AttachBufferQueue_(int32_t streamId,
     const sptr<OHOS::HDI::Camera::V1_0::BufferProducerSequenceable>& bufferProducer, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteInt32(streamId)) {
        HDF_LOGE("%{public}s: write streamId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteStrongParcelable(bufferProducer)) {
        HDF_LOGE("%{public}s: write bufferProducer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_ATTACH_BUFFER_QUEUE, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::DetachBufferQueue_(int32_t streamId,
     const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteInt32(streamId)) {
        HDF_LOGE("%{public}s: write streamId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_DETACH_BUFFER_QUEUE, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::Capture_(int32_t captureId,
     const OHOS::HDI::Camera::V1_0::CaptureInfo& info, bool isStreaming, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteInt32(captureId)) {
        HDF_LOGE("%{public}s: write captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!OHOS::HDI::Camera::V1_0::CaptureInfoBlockMarshalling(streamOperatorData, info)) {
        HDF_LOGE("%{public}s: write info failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteBool(isStreaming)) {
        HDF_LOGE("%{public}s: write isStreaming failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_CAPTURE, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::CancelCapture_(int32_t captureId,
     const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteInt32(captureId)) {
        HDF_LOGE("%{public}s: write captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_CANCEL_CAPTURE, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::ChangeToOfflineStream_(const std::vector<int32_t>& streamIds,
     const sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator>& offlineOperator, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(streamOperatorData, streamIds)) {
        HDF_LOGE("%{public}s: failed to write streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (callbackObj == nullptr) {
        HDF_LOGE("%{public}s: parameter callbackObj is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorData.WriteRemoteObject(OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(callbackObj, OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor()))) {
        HDF_LOGE("%{public}s: write callbackObj failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_CHANGE_TO_OFFLINE_STREAM, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    sptr<IRemoteObject> offlineOperatorRemote = streamOperatorReply.ReadRemoteObject();
    if (offlineOperatorRemote == nullptr) {
        HDF_LOGE("%{public}s: read an invalid remote object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    offlineOperator = new OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy(offlineOperatorRemote);
    if (offlineOperator == nullptr) {
        HDF_LOGE("%{public}s: failed to create interface object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorProxy::GetVersion_(uint32_t& majorVer, uint32_t& minorVer,
     const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorData;
    MessageParcel streamOperatorReply;
    MessageOption streamOperatorOption(MessageOption::TF_SYNC);

    if (!streamOperatorData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorRet = remote->SendRequest(CMD_STREAM_OPERATOR_GET_VERSION, streamOperatorData, streamOperatorReply, streamOperatorOption);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    if (!streamOperatorReply.ReadUint32(majorVer)) {
        HDF_LOGE("%{public}s: read majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorReply.ReadUint32(minorVer)) {
        HDF_LOGE("%{public}s: read minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorRet;
}

} // V1_0
} // Camera
} // HDI
} // OHOS
