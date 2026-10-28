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

#include "v1_0/stream_operator_callback_proxy.h"
#include <hdf_base.h>
#include <hdf_core_log.h>
#include <hdi_support.h>
#include <message_option.h>
#include <message_parcel.h>
#include <string_ex.h>

#define HDF_LOG_TAG    stream_operator_callback_proxy

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
int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureStarted(int32_t captureId,
     const std::vector<int32_t>& streamIds)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureStarted_(captureId, streamIds, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureEnded(int32_t captureId,
     const std::vector<OHOS::HDI::Camera::V1_0::CaptureEndedInfo>& infos)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureEnded_(captureId, infos, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureError(int32_t captureId,
     const std::vector<OHOS::HDI::Camera::V1_0::CaptureErrorInfo>& infos)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureError_(captureId, infos, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnFrameShutter(int32_t captureId,
     const std::vector<int32_t>& streamIds, uint64_t timestamp)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnFrameShutter_(captureId, streamIds, timestamp, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::GetVersion(uint32_t& majorVer, uint32_t& minorVer)
{
    return OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::GetVersion_(majorVer, minorVer, Remote());
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureStarted_(int32_t captureId,
     const std::vector<int32_t>& streamIds, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorCallbackData;
    MessageParcel streamOperatorCallbackReply;
    MessageOption streamOperatorCallbackOption(MessageOption::TF_SYNC);

    if (!streamOperatorCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorCallbackData.WriteInt32(captureId)) {
        HDF_LOGE("%{public}s: write captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(streamOperatorCallbackData, streamIds)) {
        HDF_LOGE("%{public}s: failed to write streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorCallbackRet = remote->SendRequest(CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_STARTED, streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    return streamOperatorCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureEnded_(int32_t captureId,
     const std::vector<OHOS::HDI::Camera::V1_0::CaptureEndedInfo>& infos, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorCallbackData;
    MessageParcel streamOperatorCallbackReply;
    MessageOption streamOperatorCallbackOption(MessageOption::TF_SYNC);

    if (!streamOperatorCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorCallbackData.WriteInt32(captureId)) {
        HDF_LOGE("%{public}s: write captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(streamOperatorCallbackData, infos)) {
        HDF_LOGE("%{public}s: failed to write infos", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorCallbackRet = remote->SendRequest(CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_ENDED, streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    return streamOperatorCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnCaptureError_(int32_t captureId,
     const std::vector<OHOS::HDI::Camera::V1_0::CaptureErrorInfo>& infos, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorCallbackData;
    MessageParcel streamOperatorCallbackReply;
    MessageOption streamOperatorCallbackOption(MessageOption::TF_SYNC);

    if (!streamOperatorCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorCallbackData.WriteInt32(captureId)) {
        HDF_LOGE("%{public}s: write captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(streamOperatorCallbackData, infos)) {
        HDF_LOGE("%{public}s: failed to write infos", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorCallbackRet = remote->SendRequest(CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_ERROR, streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    return streamOperatorCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::OnFrameShutter_(int32_t captureId,
     const std::vector<int32_t>& streamIds, uint64_t timestamp, const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorCallbackData;
    MessageParcel streamOperatorCallbackReply;
    MessageOption streamOperatorCallbackOption(MessageOption::TF_SYNC);

    if (!streamOperatorCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorCallbackData.WriteInt32(captureId)) {
        HDF_LOGE("%{public}s: write captureId failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!WritePodArray(streamOperatorCallbackData, streamIds)) {
        HDF_LOGE("%{public}s: failed to write streamIds", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorCallbackData.WriteUint64(timestamp)) {
        HDF_LOGE("%{public}s: write timestamp failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorCallbackRet = remote->SendRequest(CMD_STREAM_OPERATOR_CALLBACK_ON_FRAME_SHUTTER, streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    return streamOperatorCallbackRet;
}

int32_t OHOS::HDI::Camera::V1_0::StreamOperatorCallbackProxy::GetVersion_(uint32_t& majorVer, uint32_t& minorVer,
     const sptr<IRemoteObject> remote)
{
    MessageParcel streamOperatorCallbackData;
    MessageParcel streamOperatorCallbackReply;
    MessageOption streamOperatorCallbackOption(MessageOption::TF_SYNC);

    if (!streamOperatorCallbackData.WriteInterfaceToken(OHOS::HDI::Camera::V1_0::IStreamOperatorCallback::GetDescriptor())) {
        HDF_LOGE("%{public}s: failed to write interface descriptor!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (remote == nullptr) {
        HDF_LOGE("%{public}s: invalid remote object!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    int32_t streamOperatorCallbackRet = remote->SendRequest(CMD_STREAM_OPERATOR_CALLBACK_GET_VERSION, streamOperatorCallbackData, streamOperatorCallbackReply, streamOperatorCallbackOption);
    if (streamOperatorCallbackRet != HDF_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, streamOperatorCallbackRet);
        return streamOperatorCallbackRet;
    }

    if (!streamOperatorCallbackReply.ReadUint32(majorVer)) {
        HDF_LOGE("%{public}s: read majorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!streamOperatorCallbackReply.ReadUint32(minorVer)) {
        HDF_LOGE("%{public}s: read minorVer failed!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    return streamOperatorCallbackRet;
}

} // V1_0
} // Camera
} // HDI
} // OHOS
