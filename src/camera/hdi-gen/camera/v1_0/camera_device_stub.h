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

#ifndef OHOS_HDI_CAMERA_V1_0_CAMERADEVICESTUB_H
#define OHOS_HDI_CAMERA_V1_0_CAMERADEVICESTUB_H

#include <ipc_object_stub.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <refbase.h>
#include "v1_0/icamera_device.h"

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

using namespace OHOS;
class CameraDeviceStub : public IPCObjectStub {
public:
    explicit CameraDeviceStub(const sptr<ICameraDevice> &impl);
    virtual ~CameraDeviceStub();

    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

    static int32_t CameraDeviceStubGetStreamOperator_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl);

    static int32_t CameraDeviceStubUpdateSettings_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl);

    static int32_t CameraDeviceStubSetResultMode_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl);

    static int32_t CameraDeviceStubGetEnabledResults_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl);

    static int32_t CameraDeviceStubEnableResult_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl);

    static int32_t CameraDeviceStubDisableResult_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl);

    static int32_t CameraDeviceStubClose_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl);

    static int32_t CameraDeviceStubGetVersion_(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl);

private:
    int32_t CameraDeviceStubGetStreamOperator(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption);

    int32_t CameraDeviceStubUpdateSettings(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption);

    int32_t CameraDeviceStubSetResultMode(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption);

    int32_t CameraDeviceStubGetEnabledResults(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption);

    int32_t CameraDeviceStubEnableResult(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption);

    int32_t CameraDeviceStubDisableResult(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption);

    int32_t CameraDeviceStubClose(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption);

    int32_t CameraDeviceStubGetVersion(MessageParcel& cameraDeviceData, MessageParcel& cameraDeviceReply, MessageOption& cameraDeviceOption);


    static inline ObjectDelegator<OHOS::HDI::Camera::V1_0::CameraDeviceStub, OHOS::HDI::Camera::V1_0::ICameraDevice> objDelegator_;
    sptr<OHOS::HDI::Camera::V1_0::ICameraDevice> impl_;
};
} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_CAMERADEVICESTUB_H