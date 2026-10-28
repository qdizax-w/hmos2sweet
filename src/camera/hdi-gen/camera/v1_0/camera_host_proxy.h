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

#ifndef OHOS_HDI_CAMERA_V1_0_CAMERAHOSTPROXY_H
#define OHOS_HDI_CAMERA_V1_0_CAMERAHOSTPROXY_H

#include "v1_0/icamera_host.h"
#include <unistd.h>
#include <iproxy_broker.h>

namespace OHOS {
namespace HDI {
namespace Camera {
namespace V1_0 {

class CameraHostProxy : public IProxyBroker<OHOS::HDI::Camera::V1_0::ICameraHost> {
public:
    class IServiceManagerDeathRecipient : public IRemoteObject::DeathRecipient {
    public:
        IServiceManagerDeathRecipient(wptr<OHOS::HDI::Camera::V1_0::CameraHostProxy> proxy) : proxy_(proxy) {} 
        ~IServiceManagerDeathRecipient() override = default;
        void OnRemoteDied(const wptr<IRemoteObject> &remote) override
        {
            int32_t result = HDF_FAILURE;
            const int sleepInterval = 500000;
            const int waitTimes = 10;
            int currentTime = waitTimes;
            do {
                usleep(sleepInterval);
                auto proxy = proxy_.promote();
                if (proxy != nullptr) {
                    result = OHOS::HDI::Camera::V1_0::CameraHostProxy::Reconnect(proxy);
                }
                --currentTime;
            } while (result != HDF_SUCCESS && currentTime >0);
        }
    private:
        wptr<OHOS::HDI::Camera::V1_0::CameraHostProxy> proxy_;
    };

    explicit CameraHostProxy(const sptr<IRemoteObject>& remote) : IProxyBroker<OHOS::HDI::Camera::V1_0::ICameraHost>(remote) {
        reconnectRemote_ = nullptr;
        servMgr_ = nullptr;
        deathRecipient_ = nullptr;
        isReconnected_ = false;
    }
    virtual ~CameraHostProxy() {
        if (servMgr_ != nullptr && deathRecipient_ != nullptr) {
            servMgr_->RemoveDeathRecipient(deathRecipient_);
        }
    }

    inline bool IsProxy() override
    {
        return true;
    }

    int32_t SetCallback(const sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback>& callbackObj) override;

    int32_t GetCameraIds(std::vector<std::string>& cameraIds) override;

    int32_t GetCameraAbility(const std::string& cameraId, std::vector<uint8_t>& cameraAbility) override;

    int32_t OpenCamera(const std::string& cameraId,
         const sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice>& device) override;

    int32_t SetFlashlight(const std::string& cameraId, bool isEnable) override;

    int32_t GetVersion(uint32_t& majorVer, uint32_t& minorVer) override;

    static int32_t SetCallback_(const sptr<OHOS::HDI::Camera::V1_0::ICameraHostCallback>& callbackObj,
         const sptr<IRemoteObject> remote);

    static int32_t GetCameraIds_(std::vector<std::string>& cameraIds, const sptr<IRemoteObject> remote);

    static int32_t GetCameraAbility_(const std::string& cameraId, std::vector<uint8_t>& cameraAbility,
         const sptr<IRemoteObject> remote);

    static int32_t OpenCamera_(const std::string& cameraId,
         const sptr<OHOS::HDI::Camera::V1_0::ICameraDeviceCallback>& callbackObj, sptr<OHOS::HDI::Camera::V1_0::ICameraDevice>& device, const sptr<IRemoteObject> remote);

    static int32_t SetFlashlight_(const std::string& cameraId, bool isEnable, const sptr<IRemoteObject> remote);

    static int32_t GetVersion_(uint32_t& majorVer, uint32_t& minorVer, const sptr<IRemoteObject> remote);

    static int32_t Reconnect(sptr<OHOS::HDI::Camera::V1_0::CameraHostProxy> proxy);

    sptr<IRemoteObject> GetCurrentRemote() {
        return isReconnected_ ? reconnectRemote_ : Remote();
    }

    bool isReconnected_;
    std::string serviceName_;
    sptr<IRemoteObject> servMgr_;
    sptr<OHOS::HDI::Camera::V1_0::CameraHostProxy::IServiceManagerDeathRecipient> deathRecipient_;
    sptr<IRemoteObject> reconnectRemote_;
private:
    static inline BrokerDelegator<OHOS::HDI::Camera::V1_0::CameraHostProxy> delegator_;
};

} // V1_0
} // Camera
} // HDI
} // OHOS

#endif // OHOS_HDI_CAMERA_V1_0_CAMERAHOSTPROXY_H