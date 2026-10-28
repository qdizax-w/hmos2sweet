/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "v1_0/types.h"
#include <hdf_core_log.h>
#include <message_parcel.h>
#include <securec.h>

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

bool StreamInfoBlockMarshalling(OHOS::MessageParcel& data, const OHOS::HDI::Camera::V1_0::StreamInfo& dataBlock)
{
    if (!data.WriteInt32(dataBlock.streamId_)) {
        HDF_LOGE("%{public}s: write dataBlock.streamId_ failed!", __func__);
        return false;
    }

    if (!data.WriteInt32(dataBlock.width_)) {
        HDF_LOGE("%{public}s: write dataBlock.width_ failed!", __func__);
        return false;
    }

    if (!data.WriteInt32(dataBlock.height_)) {
        HDF_LOGE("%{public}s: write dataBlock.height_ failed!", __func__);
        return false;
    }

    if (!data.WriteInt32(dataBlock.format_)) {
        HDF_LOGE("%{public}s: write dataBlock.format_ failed!", __func__);
        return false;
    }

    if (!data.WriteInt32(dataBlock.dataspace_)) {
        HDF_LOGE("%{public}s: write dataBlock.dataspace_ failed!", __func__);
        return false;
    }

    if (!data.WriteUint64(static_cast<uint64_t>(dataBlock.intent_))) {
        HDF_LOGE("%{public}s: write dataBlock.intent_ failed!", __func__);
        return false;
    }

    if (!data.WriteBool(dataBlock.tunneledMode_)) {
        HDF_LOGE("%{public}s: write dataBlock.tunneledMode_ failed!", __func__);
        return false;
    }

    if (!data.WriteStrongParcelable(dataBlock.bufferQueue_)) {
        HDF_LOGE("%{public}s: write dataBlock.bufferQueue_ failed!", __func__);
        return false;
    }

    if (!data.WriteInt32(dataBlock.minFrameDuration_)) {
        HDF_LOGE("%{public}s: write dataBlock.minFrameDuration_ failed!", __func__);
        return false;
    }

    if (!data.WriteUint64(static_cast<uint64_t>(dataBlock.encodeType_))) {
        HDF_LOGE("%{public}s: write dataBlock.encodeType_ failed!", __func__);
        return false;
    }
    return true;
}

bool StreamInfoBlockUnmarshalling(OHOS::MessageParcel& data, OHOS::HDI::Camera::V1_0::StreamInfo& dataBlock)
{
    if (!data.ReadInt32(dataBlock.streamId_)) {
        HDF_LOGE("%{public}s: read dataBlock.streamId_ failed!", __func__);
        return false;
    }

    if (!data.ReadInt32(dataBlock.width_)) {
        HDF_LOGE("%{public}s: read dataBlock.width_ failed!", __func__);
        return false;
    }

    if (!data.ReadInt32(dataBlock.height_)) {
        HDF_LOGE("%{public}s: read dataBlock.height_ failed!", __func__);
        return false;
    }

    if (!data.ReadInt32(dataBlock.format_)) {
        HDF_LOGE("%{public}s: read dataBlock.format_ failed!", __func__);
        return false;
    }

    if (!data.ReadInt32(dataBlock.dataspace_)) {
        HDF_LOGE("%{public}s: read dataBlock.dataspace_ failed!", __func__);
        return false;
    }

    {
        uint64_t enumTmp = 0;
        if (!data.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write dataBlock.intent_ failed!", __func__);
            return false;
        }
        dataBlock.intent_ = static_cast<enum StreamIntent>(enumTmp);
    }

    if (!data.ReadBool(dataBlock.tunneledMode_)) {
        HDF_LOGE("%{public}s: read dataBlock.tunneledMode_ failed!", __func__);
        return false;
    }

    dataBlock.bufferQueue_ = data.ReadStrongParcelable<BufferProducerSequenceable>();

    if (!data.ReadInt32(dataBlock.minFrameDuration_)) {
        HDF_LOGE("%{public}s: read dataBlock.minFrameDuration_ failed!", __func__);
        return false;
    }

    {
        uint64_t enumTmp = 0;
        if (!data.ReadUint64(enumTmp)) {
            HDF_LOGE("%{public}s: write dataBlock.encodeType_ failed!", __func__);
            return false;
        }
        dataBlock.encodeType_ = static_cast<enum EncodeType>(enumTmp);
    }
    return true;
}

bool StreamAttributeBlockMarshalling(OHOS::MessageParcel& data, const OHOS::HDI::Camera::V1_0::StreamAttribute& dataBlock)
{
    if (!data.WriteUnpadBuffer((const void*)&dataBlock, sizeof(OHOS::HDI::Camera::V1_0::StreamAttribute))) {
        return false;
    }
    return true;
}

bool StreamAttributeBlockUnmarshalling(OHOS::MessageParcel& data, OHOS::HDI::Camera::V1_0::StreamAttribute& dataBlock)
{
    const OHOS::HDI::Camera::V1_0::StreamAttribute *dataBlockPtr = reinterpret_cast<const OHOS::HDI::Camera::V1_0::StreamAttribute*>(data.ReadUnpadBuffer(sizeof(OHOS::HDI::Camera::V1_0::StreamAttribute)));
    if (dataBlockPtr == NULL) {
        return false;
    }

    if (memcpy_s(&dataBlock, sizeof(OHOS::HDI::Camera::V1_0::StreamAttribute), dataBlockPtr, sizeof(OHOS::HDI::Camera::V1_0::StreamAttribute)) != EOK) {
        return false;
    }
    return true;
}

bool CaptureInfoBlockMarshalling(OHOS::MessageParcel& data, const OHOS::HDI::Camera::V1_0::CaptureInfo& dataBlock)
{
    if (!WritePodArray(data, dataBlock.streamIds_)) {
        HDF_LOGE("%{public}s: failed to write dataBlock.streamIds_", __func__);
        return false;
    }

    if (!WritePodArray(data, dataBlock.captureSetting_)) {
        HDF_LOGE("%{public}s: failed to write dataBlock.captureSetting_", __func__);
        return false;
    }

    if (!data.WriteBool(dataBlock.enableShutterCallback_)) {
        HDF_LOGE("%{public}s: write dataBlock.enableShutterCallback_ failed!", __func__);
        return false;
    }
    return true;
}

bool CaptureInfoBlockUnmarshalling(OHOS::MessageParcel& data, OHOS::HDI::Camera::V1_0::CaptureInfo& dataBlock)
{
    if (!ReadPodArray(data, dataBlock.streamIds_)) {
        HDF_LOGE("%{public}s: failed to read dataBlock.streamIds_", __func__);
        return false;
    }

    if (!ReadPodArray(data, dataBlock.captureSetting_)) {
        HDF_LOGE("%{public}s: failed to read dataBlock.captureSetting_", __func__);
        return false;
    }

    if (!data.ReadBool(dataBlock.enableShutterCallback_)) {
        HDF_LOGE("%{public}s: read dataBlock.enableShutterCallback_ failed!", __func__);
        return false;
    }
    return true;
}

bool CaptureEndedInfoBlockMarshalling(OHOS::MessageParcel& data, const OHOS::HDI::Camera::V1_0::CaptureEndedInfo& dataBlock)
{
    if (!data.WriteUnpadBuffer((const void*)&dataBlock, sizeof(OHOS::HDI::Camera::V1_0::CaptureEndedInfo))) {
        return false;
    }
    return true;
}

bool CaptureEndedInfoBlockUnmarshalling(OHOS::MessageParcel& data, OHOS::HDI::Camera::V1_0::CaptureEndedInfo& dataBlock)
{
    const OHOS::HDI::Camera::V1_0::CaptureEndedInfo *dataBlockPtr = reinterpret_cast<const OHOS::HDI::Camera::V1_0::CaptureEndedInfo*>(data.ReadUnpadBuffer(sizeof(OHOS::HDI::Camera::V1_0::CaptureEndedInfo)));
    if (dataBlockPtr == NULL) {
        return false;
    }

    if (memcpy_s(&dataBlock, sizeof(OHOS::HDI::Camera::V1_0::CaptureEndedInfo), dataBlockPtr, sizeof(OHOS::HDI::Camera::V1_0::CaptureEndedInfo)) != EOK) {
        return false;
    }
    return true;
}

bool CaptureErrorInfoBlockMarshalling(OHOS::MessageParcel& data, const OHOS::HDI::Camera::V1_0::CaptureErrorInfo& dataBlock)
{
    if (!data.WriteUnpadBuffer((const void*)&dataBlock, sizeof(OHOS::HDI::Camera::V1_0::CaptureErrorInfo))) {
        return false;
    }
    return true;
}

bool CaptureErrorInfoBlockUnmarshalling(OHOS::MessageParcel& data, OHOS::HDI::Camera::V1_0::CaptureErrorInfo& dataBlock)
{
    const OHOS::HDI::Camera::V1_0::CaptureErrorInfo *dataBlockPtr = reinterpret_cast<const OHOS::HDI::Camera::V1_0::CaptureErrorInfo*>(data.ReadUnpadBuffer(sizeof(OHOS::HDI::Camera::V1_0::CaptureErrorInfo)));
    if (dataBlockPtr == NULL) {
        return false;
    }

    if (memcpy_s(&dataBlock, sizeof(OHOS::HDI::Camera::V1_0::CaptureErrorInfo), dataBlockPtr, sizeof(OHOS::HDI::Camera::V1_0::CaptureErrorInfo)) != EOK) {
        return false;
    }
    return true;
}

} // V1_0
} // Camera
} // HDI
} // OHOS
