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

#ifndef OHOS_HDI_CAMERA_V1_0_STREAMOPERATORSTUB_H
#define OHOS_HDI_CAMERA_V1_0_STREAMOPERATORSTUB_H

#include <ipc_object_stub.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <refbase.h>
#include "v1_0/istream_operator.h"

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

using namespace OHOS;
class StreamOperatorStub : public IPCObjectStub {
public:
    explicit StreamOperatorStub(const sptr<IStreamOperator> &impl);
    virtual ~StreamOperatorStub();

    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

    static int32_t StreamOperatorStubIsStreamsSupported_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubCreateStreams_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubReleaseStreams_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubCommitStreams_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubGetStreamAttributes_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubAttachBufferQueue_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubDetachBufferQueue_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubCapture_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubCancelCapture_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubChangeToOfflineStream_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

    static int32_t StreamOperatorStubGetVersion_(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl);

private:
    int32_t StreamOperatorStubIsStreamsSupported(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubCreateStreams(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubReleaseStreams(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubCommitStreams(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubGetStreamAttributes(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubAttachBufferQueue(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubDetachBufferQueue(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubCapture(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubCancelCapture(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubChangeToOfflineStream(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);

    int32_t StreamOperatorStubGetVersion(MessageParcel& streamOperatorData, MessageParcel& streamOperatorReply, MessageOption& streamOperatorOption);


    static inline ObjectDelegator<OHOS::HDI::Camera::V1_0::StreamOperatorStub, OHOS::HDI::Camera::V1_0::IStreamOperator> objDelegator_;
    sptr<OHOS::HDI::Camera::V1_0::IStreamOperator> impl_;
};
} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_STREAMOPERATORSTUB_H