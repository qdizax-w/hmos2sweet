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

#ifndef OHOS_HDI_CAMERA_V1_0_STREAMOPERATORCALLBACKSTUB_H
#define OHOS_HDI_CAMERA_V1_0_STREAMOPERATORCALLBACKSTUB_H

#include <ipc_object_stub.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <refbase.h>
#include "v1_0/istream_operator_callback.h"

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

using namespace OHOS;
class StreamOperatorCallbackStub : public IPCObjectStub {
public:
    explicit StreamOperatorCallbackStub(const sptr<IStreamOperatorCallback> &impl);
    virtual ~StreamOperatorCallbackStub();

    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

    static int32_t StreamOperatorCallbackStubOnCaptureStarted_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl);

    static int32_t StreamOperatorCallbackStubOnCaptureEnded_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl);

    static int32_t StreamOperatorCallbackStubOnCaptureError_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl);

    static int32_t StreamOperatorCallbackStubOnFrameShutter_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl);

    static int32_t StreamOperatorCallbackStubGetVersion_(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl);

private:
    int32_t StreamOperatorCallbackStubOnCaptureStarted(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption);

    int32_t StreamOperatorCallbackStubOnCaptureEnded(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption);

    int32_t StreamOperatorCallbackStubOnCaptureError(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption);

    int32_t StreamOperatorCallbackStubOnFrameShutter(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption);

    int32_t StreamOperatorCallbackStubGetVersion(MessageParcel& streamOperatorCallbackData, MessageParcel& streamOperatorCallbackReply, MessageOption& streamOperatorCallbackOption);


    static inline ObjectDelegator<OHOS::HDI::Camera::V1_0::StreamOperatorCallbackStub, OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> objDelegator_;
    sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback> impl_;
};
} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_STREAMOPERATORCALLBACKSTUB_H