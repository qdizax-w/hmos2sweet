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

#ifndef OHOS_HDI_CAMERA_V1_0_ISTREAMOPERATORCALLBACK_H
#define OHOS_HDI_CAMERA_V1_0_ISTREAMOPERATORCALLBACK_H

#include <stdint.h>
#include <vector>
#include <hdf_base.h>
#include <hdi_base.h>
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
    CMD_STREAM_OPERATOR_CALLBACK_GET_VERSION = 0,
    CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_STARTED = 1,
    CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_ENDED = 2,
    CMD_STREAM_OPERATOR_CALLBACK_ON_CAPTURE_ERROR = 3,
    CMD_STREAM_OPERATOR_CALLBACK_ON_FRAME_SHUTTER = 4,
};

class IStreamOperatorCallback : public HdiBase {
public:
    DECLARE_HDI_DESCRIPTOR(u"ohos.hdi.camera.v1_0.IStreamOperatorCallback");

    virtual ~IStreamOperatorCallback() = default;

    virtual int32_t OnCaptureStarted(int32_t captureId, const std::vector<int32_t>& streamIds) = 0;

    virtual int32_t OnCaptureEnded(int32_t captureId,
         const std::vector<OHOS::HDI::Camera::V1_0::CaptureEndedInfo>& infos) = 0;

    virtual int32_t OnCaptureError(int32_t captureId,
         const std::vector<OHOS::HDI::Camera::V1_0::CaptureErrorInfo>& infos) = 0;

    virtual int32_t OnFrameShutter(int32_t captureId, const std::vector<int32_t>& streamIds, uint64_t timestamp) = 0;

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

#endif // OHOS_HDI_CAMERA_V1_0_ISTREAMOPERATORCALLBACK_H