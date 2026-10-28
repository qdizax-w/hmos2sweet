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

#ifndef OHOS_HDI_CAMERA_V1_0_STREAMOPERATORPROXY_H
#define OHOS_HDI_CAMERA_V1_0_STREAMOPERATORPROXY_H

#include "v1_0/istream_operator.h"
#include <iproxy_broker.h>

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

class StreamOperatorProxy : public IProxyBroker<OHOS::HDI::Camera::V1_0::IStreamOperator> {
public:
    explicit StreamOperatorProxy(const sptr<IRemoteObject>& remote) : IProxyBroker<OHOS::HDI::Camera::V1_0::IStreamOperator>(remote) {
    }
    virtual ~StreamOperatorProxy() = default;

    inline bool IsProxy() override
    {
        return true;
    }

    int32_t IsStreamsSupported(OHOS::HDI::Camera::V1_0::OperationMode mode, const std::vector<uint8_t>& modeSetting,
         const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& infos, OHOS::HDI::Camera::V1_0::StreamSupportType& type) override;

    int32_t CreateStreams(const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& streamInfos) override;

    int32_t ReleaseStreams(const std::vector<int32_t>& streamIds) override;

    int32_t CommitStreams(OHOS::HDI::Camera::V1_0::OperationMode mode,
         const std::vector<uint8_t>& modeSetting) override;

    int32_t GetStreamAttributes(std::vector<OHOS::HDI::Camera::V1_0::StreamAttribute>& attributes) override;

    int32_t AttachBufferQueue(int32_t streamId,
         const sptr<OHOS::HDI::Camera::V1_0::BufferProducerSequenceable>& bufferProducer) override;

    int32_t DetachBufferQueue(int32_t streamId) override;

    int32_t Capture(int32_t captureId, const OHOS::HDI::Camera::V1_0::CaptureInfo& info, bool isStreaming) override;

    int32_t CancelCapture(int32_t captureId) override;

    int32_t ChangeToOfflineStream(const std::vector<int32_t>& streamIds,
         const sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator>& offlineOperator) override;

    int32_t GetVersion(uint32_t& majorVer, uint32_t& minorVer) override;

    static int32_t IsStreamsSupported_(OHOS::HDI::Camera::V1_0::OperationMode mode,
         const std::vector<uint8_t>& modeSetting, const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& infos, OHOS::HDI::Camera::V1_0::StreamSupportType& type, const sptr<IRemoteObject> remote);

    static int32_t CreateStreams_(const std::vector<OHOS::HDI::Camera::V1_0::StreamInfo>& streamInfos,
         const sptr<IRemoteObject> remote);

    static int32_t ReleaseStreams_(const std::vector<int32_t>& streamIds, const sptr<IRemoteObject> remote);

    static int32_t CommitStreams_(OHOS::HDI::Camera::V1_0::OperationMode mode, const std::vector<uint8_t>& modeSetting,
         const sptr<IRemoteObject> remote);

    static int32_t GetStreamAttributes_(std::vector<OHOS::HDI::Camera::V1_0::StreamAttribute>& attributes,
         const sptr<IRemoteObject> remote);

    static int32_t AttachBufferQueue_(int32_t streamId,
         const sptr<OHOS::HDI::Camera::V1_0::BufferProducerSequenceable>& bufferProducer, const sptr<IRemoteObject> remote);

    static int32_t DetachBufferQueue_(int32_t streamId, const sptr<IRemoteObject> remote);

    static int32_t Capture_(int32_t captureId, const OHOS::HDI::Camera::V1_0::CaptureInfo& info, bool isStreaming,
         const sptr<IRemoteObject> remote);

    static int32_t CancelCapture_(int32_t captureId, const sptr<IRemoteObject> remote);

    static int32_t ChangeToOfflineStream_(const std::vector<int32_t>& streamIds,
         const sptr<OHOS::HDI::Camera::V1_0::IStreamOperatorCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator>& offlineOperator, const sptr<IRemoteObject> remote);

    static int32_t GetVersion_(uint32_t& majorVer, uint32_t& minorVer, const sptr<IRemoteObject> remote);

private:
    static inline BrokerDelegator<OHOS::HDI::Camera::V1_0::StreamOperatorProxy> delegator_;
};

} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_STREAMOPERATORPROXY_H