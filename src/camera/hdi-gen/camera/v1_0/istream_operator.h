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

#ifndef OHOS_HDI_CAMERA_V1_0_ISTREAMOPERATOR_H
#define OHOS_HDI_CAMERA_V1_0_ISTREAMOPERATOR_H

#include <stdint.h>
#include <vector>
#include <hdf_base.h>
#include <hdi_base.h>
#include "buffer_producer_sequenceable.h"
#include "camera/v1_0/ioffline_stream_operator.h"
#include "camera/v1_0/istream_operator_callback.h"
#include "camera/v1_0/types.h"

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
namespace HDI {
namespace Camera {
namespace V1_0 {
using namespace OHOS;
using namespace OHOS::HDI;

enum {
    CMD_STREAM_OPERATOR_GET_VERSION = 0,
    CMD_STREAM_OPERATOR_IS_STREAMS_SUPPORTED = 1,
    CMD_STREAM_OPERATOR_CREATE_STREAMS = 2,
    CMD_STREAM_OPERATOR_RELEASE_STREAMS = 3,
    CMD_STREAM_OPERATOR_COMMIT_STREAMS = 4,
    CMD_STREAM_OPERATOR_GET_STREAM_ATTRIBUTES = 5,
    CMD_STREAM_OPERATOR_ATTACH_BUFFER_QUEUE = 6,
    CMD_STREAM_OPERATOR_DETACH_BUFFER_QUEUE = 7,
    CMD_STREAM_OPERATOR_CAPTURE = 8,
    CMD_STREAM_OPERATOR_CANCEL_CAPTURE = 9,
    CMD_STREAM_OPERATOR_CHANGE_TO_OFFLINE_STREAM = 10,
};

class IStreamOperator : public HdiBase {
public:
    DECLARE_HDI_DESCRIPTOR(u"ohos.hdi.camera.v1_0.IStreamOperator");

    virtual ~IStreamOperator() = default;

    virtual int32_t IsStreamsSupported(OHOS::HDI::Camera::V1_0::OperationMode mode,
         const std::vector<uint8_t>& modeSetting, const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& infos, OHOS::HDI::Camera::V1_0::StreamSupportType& type) = 0;

    virtual int32_t CreateStreams(const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& streamInfos) = 0;

    virtual int32_t ReleaseStreams(const std::vector<int32_t>& streamIds) = 0;

    virtual int32_t CommitStreams(OHOS::HDI::Camera::V1_0::OperationMode mode,
         const std::vector<uint8_t>& modeSetting) = 0;

    virtual int32_t GetStreamAttributes(std::vector<OHOS::HDI::Camera::V1_0::StreamAttribute>& attributes) = 0;

    virtual int32_t AttachBufferQueue(int32_t streamId,
         const sptr<OHOS::HDI::Camera::V1_0::BufferProducerSequenceable>& bufferProducer) = 0;

    virtual int32_t DetachBufferQueue(int32_t streamId) = 0;

    virtual int32_t Capture(int32_t captureId, const OHOS::HDI::Camera::V1_0::CaptureInfo& info, bool isStreaming) = 0;

    virtual int32_t CancelCapture(int32_t captureId) = 0;

    virtual int32_t ChangeToOfflineStream(const std::vector<int32_t>& streamIds,
         const sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator>& offlineOperator) = 0;

    virtual int32_t GetVersion(uint32_t& majorVer, uint32_t& minorVer)
    {
        majorVer = 1;
        minorVer = 0;
        return HDF_SUCCESS;
    }

    virtual bool IsProxy()
    {
        return false;
    }

    virtual const std::u16string GetDesc()
    {
        return metaDescriptor_;
    }
};
} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_ISTREAMOPERATOR_H