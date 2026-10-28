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

#ifndef OHOS_HDI_CAMERA_V1_0_OFFLINESTREAMOPERATORSTUB_H
#define OHOS_HDI_CAMERA_V1_0_OFFLINESTREAMOPERATORSTUB_H

#include <ipc_object_stub.h>
#include <message_option.h>
#include <message_parcel.h>
#include <object_collector.h>
#include <refbase.h>
#include "v1_0/ioffline_stream_operator.h"

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

using namespace OHOS;
class OfflineStreamOperatorStub : public IPCObjectStub {
public:
    explicit OfflineStreamOperatorStub(const sptr<IOfflineStreamOperator> &impl);
    virtual ~OfflineStreamOperatorStub();

    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

    static int32_t OfflineStreamOperatorStubCancelCapture_(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl);

    static int32_t OfflineStreamOperatorStubReleaseStreams_(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl);

    static int32_t OfflineStreamOperatorStubRelease_(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl);

    static int32_t OfflineStreamOperatorStubGetVersion_(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl);

private:
    int32_t OfflineStreamOperatorStubCancelCapture(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption);

    int32_t OfflineStreamOperatorStubReleaseStreams(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption);

    int32_t OfflineStreamOperatorStubRelease(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption);

    int32_t OfflineStreamOperatorStubGetVersion(MessageParcel& offlineStreamOperatorData, MessageParcel& offlineStreamOperatorReply, MessageOption& offlineStreamOperatorOption);


    static inline ObjectDelegator<OHOS::HDI::Camera::V1_0::OfflineStreamOperatorStub, OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> objDelegator_;
    sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> impl_;
};
} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_OFFLINESTREAMOPERATORSTUB_H