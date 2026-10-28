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

#include "v1_0/stream_operator_stub.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <iproxy_broker.h>
#include <object_collector.h>
#include "camera/v1_0/stream_operator_callback_proxy.h"

#define HDF_LOG_TAG    stream_operator_stub

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


OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStub(const sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> &impl)
    : IPCObjectStub(OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()), impl_(impl)
{
}

OHOS::HDI::Camera::V1_0::StreamOperatorStub::~StreamOperatorStub()
{
    HDF_LOGI("%{public}s enter", __func__);
    ObjectCollector::GetInstance().RemoveObject(impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply, MessageOption& option)
{
    switch (code) {
        case CMD_STREAM_OPERATOR_GET_VERSION:
            return StreamOperatorStubGetVersion(data, reply, option);
        case CMD_STREAM_OPERATOR_IS_STREAMS_SUPPORTED:
            return StreamOperatorStubIsStreamsSupported(data, reply, option);
        case CMD_STREAM_OPERATOR_CREATE_STREAMS:
            return StreamOperatorStubCreateStreams(data, reply, option);
        case CMD_STREAM_OPERATOR_RELEASE_STREAMS:
            return StreamOperatorStubReleaseStreams(data, reply, option);
        case CMD_STREAM_OPERATOR_COMMIT_STREAMS:
            return StreamOperatorStubCommitStreams(data, reply, option);
        case CMD_STREAM_OPERATOR_GET_STREAM_ATTRIBUTES:
            return StreamOperatorStubGetStreamAttributes(data, reply, option);
        case CMD_STREAM_OPERATOR_ATTACH_BUFFER_QUEUE:
            return StreamOperatorStubAttachBufferQueue(data, reply, option);
        case CMD_STREAM_OPERATOR_DETACH_BUFFER_QUEUE:
            return StreamOperatorStubDetachBufferQueue(data, reply, option);
        case CMD_STREAM_OPERATOR_CAPTURE:
            return StreamOperatorStubCapture(data, reply, option);
        case CMD_STREAM_OPERATOR_CANCEL_CAPTURE:
            return StreamOperatorStubCancelCapture(data, reply, option);
        case CMD_STREAM_OPERATOR_CHANGE_TO_OFFLINE_STREAM:
            return StreamOperatorStubChangeToOfflineStream(data, reply, option);
        default: {
            HDF_LOGE("%{public}s: cmd %{public}d is not supported", __func__, code);
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubIsStreamsSupported(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubIsStreamsSupported_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCreateStreams(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCreateStreams_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubReleaseStreams(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubReleaseStreams_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCommitStreams(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCommitStreams_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubGetStreamAttributes(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubGetStreamAttributes_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubAttachBufferQueue(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubAttachBufferQueue_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubDetachBufferQueue(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubDetachBufferQueue_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCapture(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCapture_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCancelCapture(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCancelCapture_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubChangeToOfflineStream(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubChangeToOfflineStream_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubGetVersion(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubGetVersion_(streamOperatorData, streamOperatorReply, streamOperatorOption, impl_);
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubIsStreamsSupported_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    OHOS::HDI::Camera::V1_0::OperationMode mode = static_cast<enum OperationMode>(0);
    {
        uint64_t enumTmp = 0;
        if (!streamOperatorData.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write mode failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        mode = static_cast<enum OperationMode>(enumTmp);
    }

    std::vector<uint8_t> modeSetting;
    if (!ReadPodArray(streamOperatorData, modeSetting)) {
        HDF_LOGE("%{public}s: failed to read modeSetting", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<OHOS::HDI::Camera::V1_0::StreamInfo> infos;
    uint32_t infosSize = 0;
    if (!streamOperatorData.ReadUint32(infosSize)) {
        HDF_LOGE("%{public}s: failed to read size", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    HDI_CHECK_VALUE_RETURN(infosSize, >, HDI_BUFF_MAX_SIZE / sizeof(OHOS::HDI::Camera::V1_0::StreamInfo), HDF_ERR_INVALID_PARAM);
    infos.clear();
    infos.reserve(infosSize);
    for (uint32_t i0 = 0; i0 < infosSize; ++i0) {
        OHOS::HDI::Camera::V1_0::StreamInfo value0;
        if (!StreamInfoBlockUnmarshalling(streamOperatorData, value0)) {
            HDF_LOGE("%{public}s: read value0 failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        infos.push_back(value0);
    }

    OHOS::HDI::Camera::V1_0::StreamSupportType type;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->IsStreamsSupported(mode, modeSetting, infos, type);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    if (!streamOperatorReply.WriteUint64(static_cast<uint64_t>(type))) {
        HDF_LOGE("%{public}s: write type failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCreateStreams_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<OHOS::HDI::Camera::V1_0::StreamInfo> streamInfos;
    uint32_t streamInfosSize = 0;
    if (!streamOperatorData.ReadUint32(streamInfosSize)) {
        HDF_LOGE("%{public}s: failed to read size", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    HDI_CHECK_VALUE_RETURN(streamInfosSize, >, HDI_BUFF_MAX_SIZE / sizeof(OHOS::HDI::Camera::V1_0::StreamInfo), HDF_ERR_INVALID_PARAM);
    streamInfos.clear();
    streamInfos.reserve(streamInfosSize);
    for (uint32_t i0 = 0; i0 < streamInfosSize; ++i0) {
        OHOS::HDI::Camera::V1_0::StreamInfo value0;
        if (!StreamInfoBlockUnmarshalling(streamOperatorData, value0)) {
            HDF_LOGE("%{public}s: read value0 failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        streamInfos.push_back(value0);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->CreateStreams(streamInfos);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubReleaseStreams_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<int32_t> streamIds;
    if (!ReadPodArray(streamOperatorData, streamIds)) {
        HDF_LOGE("%{public}s: failed to read streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->ReleaseStreams(streamIds);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCommitStreams_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    OHOS::HDI::Camera::V1_0::OperationMode mode = static_cast<enum OperationMode>(0);
    {
        uint64_t enumTmp = 0;
        if (!streamOperatorData.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write mode failed!", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        mode = static_cast<enum OperationMode>(enumTmp);
    }

    std::vector<uint8_t> modeSetting;
    if (!ReadPodArray(streamOperatorData, modeSetting)) {
        HDF_LOGE("%{public}s: failed to read modeSetting", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->CommitStreams(mode, modeSetting);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubGetStreamAttributes_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    bool streamOperatorMemSet = false;
    if (!streamOperatorData.ReadBool(streamOperatorMemSet)) {
        HDF_LOGE("%{public}s: failed to read streamOperatorMemSet", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    std::vector<OHOS::HDI::Camera::V1_0::StreamAttribute> attributes;
    if (streamOperatorMemSet) {
        uint32_t capacity = 0;
        if (!streamOperatorData.ReadUint32(capacity)) {
            HDF_LOGE("%{public}s: failed to read capacity", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        HDI_CHECK_VALUE_RETURN(capacity, >, HDI_BUFF_MAX_SIZE / sizeof(OHOS::HDI::Camera::V1_0::StreamAttribute), HDF_ERR_INVALID_PARAM);
        attributes.reserve(capacity);
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->GetStreamAttributes(attributes);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    if (!WritePodArray(streamOperatorReply, attributes)) {
        HDF_LOGE("%{public}s: failed to write attributes", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubAttachBufferQueue_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamId = 0;
    if (!streamOperatorData.ReadInt32(streamId)) {
        HDF_LOGE("%{public}s: read streamId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    sptr<BufferProducerSequenceable> bufferProducer = streamOperatorData.ReadStrongParcelable<BufferProducerSequenceable>();

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->AttachBufferQueue(streamId, bufferProducer);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubDetachBufferQueue_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamId = 0;
    if (!streamOperatorData.ReadInt32(streamId)) {
        HDF_LOGE("%{public}s: read streamId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->DetachBufferQueue(streamId);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCapture_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t captureId = 0;
    if (!streamOperatorData.ReadInt32(captureId)) {
        HDF_LOGE("%{public}s: read captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    OHOS::HDI::Camera::V1_0::CaptureInfo info;
    if (!CaptureInfoBlockUnmarshalling(streamOperatorData, info)) {
        HDF_LOGE("%{public}s: read info failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    bool isStreaming = false;
    if (!streamOperatorData.ReadBool(isStreaming)) {
        HDF_LOGE("%{public}s: read isStreaming failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->Capture(captureId, info, isStreaming);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubCancelCapture_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t captureId = 0;
    if (!streamOperatorData.ReadInt32(captureId)) {
        HDF_LOGE("%{public}s: read captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->CancelCapture(captureId);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubChangeToOfflineStream_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    std::vector<int32_t> streamIds;
    if (!ReadPodArray(streamOperatorData, streamIds)) {
        HDF_LOGE("%{public}s: failed to read streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> callbackObj;
    sptr<IRemoteObject> callbackObjRemote = streamOperatorData.ReadRemoteObject();
    if (callbackObjRemote == nullptr) {
        HDF_LOGE("%{public}s: read an invalid remote object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    callbackObj = new OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy(callbackObjRemote);
    if (callbackObj == nullptr) {
        HDF_LOGE("%{public}s: failed to create interface object", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> offlineOperator;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->ChangeToOfflineStream(streamIds, callbackObj, offlineOperator);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    if (offlineOperator == nullptr) {
        HDF_LOGE("%{public}s: parameter offlineOperator is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorReply.WriteRemoteObject(OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(offlineOperator, OHOS::HDI::Camera::V1_0::IOfflineStreamOperator::GetDescriptor()))) {
        HDF_LOGE("%{public}s: write offlineOperator failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorStub::StreamOperatorStubGetVersion_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl)
{
    if (streamOperatorData.ReadInterfaceToken() != OHOS::HDI::Camera::V1_0::IStreamOperator::GetDescriptor()) {
        HDF_LOGE("%{public}s: interface token check failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint32_t majorVer = 0;

    uint32_t minorVer = 0;

    if (impl == nullptr) {
        HDF_LOGE("%{public}s: impl is nullptr!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t streamOperatorRet = impl->GetVersion(majorVer, minorVer);
    if (streamOperatorRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorRet);
        return streamOperatorRet;
    }

    if (!streamOperatorReply.WriteUint32(majorVer)) {
        HDF_LOGE("%{public}s: write majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorReply.WriteUint32(minorVer)) {
        HDF_LOGE("%{public}s: write minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorRet;
}
} // V1_0
} // Camera
} // HDI
} // OHOS
