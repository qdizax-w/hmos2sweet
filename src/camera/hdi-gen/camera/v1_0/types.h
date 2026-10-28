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

#ifndef OHOS_HDI_CAMERA_V1_0_TYPES_H
#define OHOS_HDI_CAMERA_V1_0_TYPES_H

#include <cstdbool>
#include <cstdint>
#include <vector>
#include "buffer_producer_sequenceable.h"

#ifndef HDI_BUFF_MAX_SIZE
#define HDI_BUFF_MAX_SIZE (1024 * 200)
#endif

#ifndef HDI_CHECK_VALUE_RETURN
#define HDI_CHECK_VALUE_RETURN(lv, compare, rv, ret) do { \
    if ((lv) compare (rv)) { \
        return ret; \
    } \
} while (false)
#endif

#ifndef HDI_CHECK_VALUE_RET_GOTO
#define HDI_CHECK_VALUE_RET_GOTO(lv, compare, rv, ret, value, table) do { \
    if ((lv) compare (rv)) { \
        ret = value; \
        goto table; \
    } \
} while (false)
#endif

namespace OHOS {
class MessageParcel;
}

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

using namespace OHOS;

enum CamRetCode : int32_t {
    NO_ERROR = 0,
    CAMERA_BUSY = -1,
    INSUFFICIENT_RESOURCES = -2,
    INVALID_ARGUMENT = -3,
    METHOD_NOT_SUPPORTED = -4,
    CAMERA_CLOSED = -5,
    DEVICE_ERROR = -6,
    NO_PERMISSION = -7,
};

enum ResultCallbackMode : int32_t {
    PER_FRAME = 0,
    ON_CHANGED = 1,
};

enum OperationMode : int32_t {
    NORMAL = 0,
};

enum StreamIntent : int32_t {
    PREVIEW = 0,
    VIDEO = 1,
    STILL_CAPTURE = 2,
    POST_VIEW = 3,
    ANALYZE = 4,
    CUSTOM = 5,
};

enum EncodeType : int32_t {
    ENCODE_TYPE_NULL = 0,
    ENCODE_TYPE_H264 = 1,
    ENCODE_TYPE_H265 = 2,
    ENCODE_TYPE_JPEG = 3,
};

enum StreamSupportType : int32_t {
    DYNAMIC_SUPPORTED = 0,
    RE_CONFIGURED_REQUIRED = 1,
    NOT_SUPPORTED = 2,
};

enum CameraStatus : int32_t {
    UN_AVAILABLE = 0,
    AVAILABLE = 1,
};

enum FlashlightStatus : int32_t {
    FLASHLIGHT_OFF = 0,
    FLASHLIGHT_ON = 1,
    FLASHLIGHT_UNAVAILABLE = 2,
};

enum CameraEvent : int32_t {
    CAMERA_EVENT_DEVICE_ADD = 0,
    CAMERA_EVENT_DEVICE_RMV = 1,
};

enum ErrorType : int32_t {
    FATAL_ERROR = 0,
    REQUEST_TIMEOUT = 1,
    DRIVER_ERROR = 2,
    DEVICE_PREEMPT = 3,
    DEVICE_DISCONNECT = 4,
    DCAMERA_ERROR_BEGIN = 1024,
    DCAMERA_ERROR_DEVICE_IN_USE,
    DCAMERA_ERROR_NO_PERMISSION,
};

enum StreamError : int32_t {
    UNKNOWN_ERROR = 0,
    BUFFER_LOST = 1,
};

struct StreamInfo {
    int32_t streamId_;
    int32_t width_;
    int32_t height_;
    int32_t format_;
    int32_t dataspace_;
    OHOS::HDI::Camera::V1_0::StreamIntent intent_;
    bool tunneledMode_;
    sptr<OHOS::HDI::Camera::V1_0::BufferProducerSequenceable> bufferQueue_;
    int32_t minFrameDuration_;
    OHOS::HDI::Camera::V1_0::EncodeType encodeType_;
};

struct StreamAttribute {
    int32_t streamId_;
    int32_t width_;
    int32_t height_;
    int32_t overrideFormat_;
    int32_t overrideDataspace_;
    int32_t producerUsage_;
    int32_t producerBufferCount_;
    int32_t maxBatchCaptureCount_;
    int32_t maxCaptureCount_;
} __attribute__ ((aligned(8)));

struct CaptureInfo {
    std::vector<int32_t> streamIds_;
    std::vector<uint8_t> captureSetting_;
    bool enableShutterCallback_;
};

struct CaptureEndedInfo {
    int32_t streamId_;
    int32_t frameCount_;
} __attribute__ ((aligned(8)));

struct CaptureErrorInfo {
    int32_t streamId_;
    OHOS::HDI::Camera::V1_0::StreamError error_;
} __attribute__ ((aligned(8)));

bool StreamInfoBlockMarshalling(OHOS::MessageParcel &data, const OHOS::HDI::Camera::V1_0::StreamInfo& dataBlock);

bool StreamInfoBlockUnmarshalling(OHOS::MessageParcel &data, OHOS::HDI::Camera::V1_0::StreamInfo& dataBlock);

bool StreamAttributeBlockMarshalling(OHOS::MessageParcel &data, const OHOS::HDI::Camera::V1_0::StreamAttribute& dataBlock);

bool StreamAttributeBlockUnmarshalling(OHOS::MessageParcel &data, OHOS::HDI::Camera::V1_0::StreamAttribute& dataBlock);

bool CaptureInfoBlockMarshalling(OHOS::MessageParcel &data, const OHOS::HDI::Camera::V1_0::CaptureInfo& dataBlock);

bool CaptureInfoBlockUnmarshalling(OHOS::MessageParcel &data, OHOS::HDI::Camera::V1_0::CaptureInfo& dataBlock);

bool CaptureEndedInfoBlockMarshalling(OHOS::MessageParcel &data, const OHOS::HDI::Camera::V1_0::CaptureEndedInfo& dataBlock);

bool CaptureEndedInfoBlockUnmarshalling(OHOS::MessageParcel &data, OHOS::HDI::Camera::V1_0::CaptureEndedInfo& dataBlock);

bool CaptureErrorInfoBlockMarshalling(OHOS::MessageParcel &data, const OHOS::HDI::Camera::V1_0::CaptureErrorInfo& dataBlock);

bool CaptureErrorInfoBlockUnmarshalling(OHOS::MessageParcel &data, OHOS::HDI::Camera::V1_0::CaptureErrorInfo& dataBlock);

} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_TYPES_H