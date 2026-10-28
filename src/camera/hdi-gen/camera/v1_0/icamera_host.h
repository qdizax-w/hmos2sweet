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

#ifndef OHOS_HDI_CAMERA_V1_0_ICAMERAHOST_H
#define OHOS_HDI_CAMERA_V1_0_ICAMERAHOST_H

#include <stdint.h>
#include <string>
#include <vector>
#include <hdf_base.h>
#include <hdi_base.h>
#include "camera/v1_0/icamera_device.h"
#include "camera/v1_0/icamera_device_callback.h"
#include "camera/v1_0/icamera_host_callback.h"

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
    CMD_CAMERA_HOST_GET_VERSION = 0,
    CMD_CAMERA_HOST_SET_CALLBACK = 1,
    CMD_CAMERA_HOST_GET_CAMERA_IDS = 2,
    CMD_CAMERA_HOST_GET_CAMERA_ABILITY = 3,
    CMD_CAMERA_HOST_OPEN_CAMERA = 4,
    CMD_CAMERA_HOST_SET_FLASHLIGHT = 5,
};

class ICameraHost : public HdiBase {
public:
    DECLARE_HDI_DESCRIPTOR(u"ohos.hdi.camera.v1_0.ICameraHost");

    virtual ~ICameraHost() = default;

    static sptr<OHOS::HDI::Camera::V1_0::ICameraHost> Get(bool isStub = false);
    static sptr<OHOS::HDI::Camera::V1_0::ICameraHost> Get(const std::string &serviceName, bool isStub = false);

    virtual int32_t SetCallback(const sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback>& callbackObj) = 0;

    virtual int32_t GetCameraIds(std::vector<std::string>& cameraIds) = 0;

    virtual int32_t GetCameraAbility(const std::string& cameraId, std::vector<uint8_t>& cameraAbility) = 0;

    virtual int32_t OpenCamera(const std::string& cameraId,
         const sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice>& device) = 0;

    virtual int32_t SetFlashlight(const std::string& cameraId, bool isEnable) = 0;

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

#endif // OHOS_HDI_CAMERA_V1_0_ICAMERAHOST_H