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

#ifndef OHOS_HDI_CAMERA_V1_0_CAMERAHOSTCALLBACKSTUB_H
#define OHOS_HDI_CAMERA_V1_0_CAMERAHOSTCALLBACKSTUB_H

#include <ipc_object_stub.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <refbase.h>
#include "v1_0/icamera_host_callback.h"

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

using namespace OHOS;
class CameraHostCallbackStub : public IPCObjectStub {
public:
    explicit CameraHostCallbackStub(const sptr<ICameraHostCallback> &impl);
    virtual ~CameraHostCallbackStub();

    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

    static int32_t CameraHostCallbackStubOnCameraStatus_(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl);

    static int32_t CameraHostCallbackStubOnFlashlightStatus_(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl);

    static int32_t CameraHostCallbackStubOnCameraEvent_(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl);

    static int32_t CameraHostCallbackStubGetVersion_(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl);

private:
    int32_t CameraHostCallbackStubOnCameraStatus(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption);

    int32_t CameraHostCallbackStubOnFlashlightStatus(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption);

    int32_t CameraHostCallbackStubOnCameraEvent(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption);

    int32_t CameraHostCallbackStubGetVersion(MessageParcel& cameraHostCallbackData, MessageParcel& cameraHostCallbackReply, MessageOption& cameraHostCallbackOption);


    static inline ObjectDelegator<OHOS::HDI::Camera::V1_0::CameraHostCallbackStub, OHOS::HDI::Camera::V1_0::ICameraHostCallback> objDelegator_;
    sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback> impl_;
};
} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_CAMERAHOSTCALLBACKSTUB_H