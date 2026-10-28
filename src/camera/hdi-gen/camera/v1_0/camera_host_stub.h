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

#ifndef OHOS_HDI_CAMERA_V1_0_CAMERAHOSTSTUB_H
#define OHOS_HDI_CAMERA_V1_0_CAMERAHOSTSTUB_H

#include <ipc_object_stub.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <refbase.h>
#include "v1_0/icamera_host.h"

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

using namespace OHOS;
class CameraHostStub : public IPCObjectStub {
public:
    explicit CameraHostStub(const sptr<ICameraHost> &impl);
    virtual ~CameraHostStub();

    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

    static int32_t CameraHostStubSetCallback_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl);

    static int32_t CameraHostStubGetCameraIds_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl);

    static int32_t CameraHostStubGetCameraAbility_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl);

    static int32_t CameraHostStubOpenCamera_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl);

    static int32_t CameraHostStubSetFlashlight_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl);

    static int32_t CameraHostStubGetVersion_(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption, sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl);

private:
    int32_t CameraHostStubSetCallback(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption);

    int32_t CameraHostStubGetCameraIds(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption);

    int32_t CameraHostStubGetCameraAbility(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption);

    int32_t CameraHostStubOpenCamera(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption);

    int32_t CameraHostStubSetFlashlight(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption);

    int32_t CameraHostStubGetVersion(MessageParcel& cameraHostData, MessageParcel& cameraHostReply, MessageOption& cameraHostOption);


    static inline ObjectDelegator<OHOS::HDI::Camera::V1_0::CameraHostStub, OHOS::HDI::Camera::V1_0::ICameraHost> objDelegator_;
    sptr<OHOS::HDI::Camera::V1_0::ICameraHost> impl_;
};
} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_CAMERAHOSTSTUB_H