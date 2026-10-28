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

#ifndef OHOS_HDI_CAMERA_V1_0_OFFLINESTREAMOPERATORPROXY_H
#define OHOS_HDI_CAMERA_V1_0_OFFLINESTREAMOPERATORPROXY_H

#include "v1_0/ioffline_stream_operator.h"
#include <iproxy_broker.h>

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

class OfflineStreamOperatorProxy : public IProxyBroker<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator> {
public:
    explicit OfflineStreamOperatorProxy(const sptr<IRemoteObject>& remote) : IProxyBroker<OHOS::HDI::Camera::V1_0::IOfflineStreamOperator>(remote) {
    }
    virtual ~OfflineStreamOperatorProxy() = default;

    inline bool IsProxy() override
    {
        return true;
    }

    int32_t CancelCapture(int32_t captureId) override;

    int32_t ReleaseStreams(const std::vector<int32_t>& streamIds) override;

    int32_t Release() override;

    int32_t GetVersion(uint32_t& majorVer, uint32_t& minorVer) override;

    static int32_t CancelCapture_(int32_t captureId, const sptr<IRemoteObject> remote);

    static int32_t ReleaseStreams_(const std::vector<int32_t>& streamIds, const sptr<IRemoteObject> remote);

    static int32_t Release_(const sptr<IRemoteObject> remote);

    static int32_t GetVersion_(uint32_t& majorVer, uint32_t& minorVer, const sptr<IRemoteObject> remote);

private:
    static inline BrokerDelegator<OHOS::HDI::Camera::V1_0::OfflineStreamOperatorProxy> delegator_;
};

} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_OFFLINESTREAMOPERATORPROXY_H