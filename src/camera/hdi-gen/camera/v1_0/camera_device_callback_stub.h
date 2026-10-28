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

#ifndef OHOS_HDI_CAMERA_V1_0_CAMERADEVICECALLBACKSTUB_H
#define OHOS_HDI_CAMERA_V1_0_CAMERADEVICECALLBACKSTUB_H

#include <ipc_object_stub.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <refbase.h>
#include "v1_0/icamera_device_callback.h"

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

using namespace OHOS;
class CameraDeviceCallbackStub : public IPCObjectStub {
public:
    explicit CameraDeviceCallbackStub(const sptr<ICameraDeviceCallback> &impl);
    virtual ~CameraDeviceCallbackStub();

    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

    static int32_t CameraDeviceCallbackStubOnError_(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> impl);

    static int32_t CameraDeviceCallbackStubOnResult_(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> impl);

    static int32_t CameraDeviceCallbackStubGetVersion_(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> impl);

private:
    int32_t CameraDeviceCallbackStubOnError(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption);

    int32_t CameraDeviceCallbackStubOnResult(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption);

    int32_t CameraDeviceCallbackStubGetVersion(MessageParcel& cameraDeviceCallbackData, MessageParcel& cameraDeviceCallbackReply, MessageOption& cameraDeviceCallbackOption);


    static inline ObjectDelegator<OHOS::HDI::Camera::V1_0::CameraDeviceCallbackStub, OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> objDelegator_;
    sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback> impl_;
};
} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_CAMERADEVICECALLBACKSTUB_H