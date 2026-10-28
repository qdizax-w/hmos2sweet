/*
 * sweet RIL core: entry point, reporting, worker thread, modem requests.
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "ril.h"

#include <cstdlib>
#include <unistd.h>

namespace sweetril {

Ril &Ril::Get()
{
    static Ril r;
    return r;
}

int SlotOf(const ReqDataInfo *req)
{
    return req ? static_cast<int>(req->slotId) : 0;
}

void Ril::Respond(int domain, const ReqDataInfo *req, int32_t err, const void *data, size_t len)
{
    if (report_ == nullptr || req == nullptr) {
        return;
    }
    struct ReportInfo ri = {};
    ri.requestInfo = const_cast<ReqDataInfo *>(req);
    ri.notifyId = 0;
    ri.type = HRIL_RESPONSE;
    ri.error = static_cast<HRilErrNumber>(err);
    int32_t slot = SlotOf(req);
    const uint8_t *d = static_cast<const uint8_t *>(data);
    switch (domain) {
        case D_CALL: report_->OnCallReport(slot, ri, d, len); break;
        case D_DATA: report_->OnDataReport(slot, ri, d, len); break;
        case D_MODEM: report_->OnModemReport(slot, ri, d, len); break;
        case D_NETWORK: report_->OnNetworkReport(slot, ri, d, len); break;
        case D_SIM: report_->OnSimReport(slot, ri, d, len); break;
        case D_SMS: report_->OnSmsReport(slot, ri, d, len); break;
        default: break;
    }
}

void Ril::Notify(int domain, int32_t slot, int32_t notifyId, const void *data, size_t len)
{
    if (report_ == nullptr) {
        return;
    }
    struct ReportInfo ri = {};
    ri.requestInfo = nullptr;
    ri.notifyId = notifyId;
    ri.type = HRIL_NOTIFICATION;
    ri.error = HRIL_ERR_SUCCESS;
    const uint8_t *d = static_cast<const uint8_t *>(data);
    switch (domain) {
        case D_CALL: report_->OnCallReport(slot, ri, d, len); break;
        case D_DATA: report_->OnDataReport(slot, ri, d, len); break;
        case D_MODEM: report_->OnModemReport(slot, ri, d, len); break;
        case D_NETWORK: report_->OnNetworkReport(slot, ri, d, len); break;
        case D_SIM: report_->OnSimReport(slot, ri, d, len); break;
        case D_SMS: report_->OnSmsReport(slot, ri, d, len); break;
        default: break;
    }
}

void Ril::Post(std::function<void()> fn)
{
    {
        std::lock_guard<std::mutex> lk(qMu_);
        queue_.push_back(std::move(fn));
    }
    qCv_.notify_one();
}

void Ril::Worker()
{
    for (;;) {
        std::function<void()> fn;
        {
            std::unique_lock<std::mutex> lk(qMu_);
            qCv_.wait(lk, [this] { return !queue_.empty(); });
            fn = std::move(queue_.front());
            queue_.erase(queue_.begin());
        }
        fn();
    }
}

void Ril::Init(const struct HRilReport *ops)
{
    report_ = ops;
    InstallSsOps(g_callOps);
    std::thread([this] { Worker(); }).detach();
    auto &w = ServiceWatcher::Get();
    for (QmiClient *c : {&dms, &nas, &uim, &voice, &wms, &wds, &wda}) {
        w.Add(c);
    }
    dms.OnUp(ModemOnUp);
    uim.OnUp(SimOnUp);
    nas.OnUp(NetworkOnUp);
    voice.OnUp(CallOnUp);
    wms.OnUp(SmsOnUp);
    wda.OnUp(DataOnUp);
    ImsInit();
    w.Start();
    Klog("vendor library loaded, waiting for the modem's QMI services");
}

/* ================================================================ MODEM */
namespace {
enum DmsOperatingMode : uint8_t {
    DMS_MODE_ONLINE = 0,
    DMS_MODE_LOW_POWER = 1,
    DMS_MODE_OFFLINE = 3,
};

void NotifyRadioState(int st)
{
    Ril &r = Ril::Get();
    {
        std::lock_guard<std::mutex> lk(r.stateMu);
        r.radioState = st;
    }
    int32_t v = st;
    for (int slot = 0; slot < 2; slot++) {
        r.Notify(D_MODEM, slot, HNOTI_MODEM_RADIO_STATE_UPDATED, &v, sizeof(v));
    }
}

int ReadRadioState()
{
    QmiMsg m;
    if (!Ril::Get().dms.Request(DMS_GET_OPERATING_MODE, m) || !m.Ok()) {
        return HRIL_RADIO_POWER_STATE_UNAVAILABLE;
    }
    uint8_t mode = DMS_MODE_LOW_POWER;
    m.GetU8(0x01, mode);
    return mode == DMS_MODE_ONLINE ? HRIL_RADIO_POWER_STATE_ON : HRIL_RADIO_POWER_STATE_OFF;
}

void ReqSetRadioState(const ReqDataInfo *req, int32_t fun, int32_t rst)
{
    Ril::Get().Post([req, fun, rst] {
        Ril &r = Ril::Get();
        (void)rst;
        uint8_t mode = (fun == 1) ? DMS_MODE_ONLINE : DMS_MODE_LOW_POWER;
        QmiMsg m;
        bool sent = r.dms.Request(DMS_SET_OPERATING_MODE, TlvWriter().U8(0x01, mode), m, 30000);
        int32_t err = QmiErr(sent, m);
        /* QMI_ERR_NO_EFFECT (0x1A): already in that mode */
        if (sent && m.result != 0 && m.error == 0x1A) {
            err = HRIL_ERR_SUCCESS;
        }
        Klog("SetRadioState fun=%d -> mode %u, %s (err %u)", fun, mode, err == 0 ? "ok" : "failed", m.error);
        r.Respond(D_MODEM, req, err, nullptr, 0);
        if (err == HRIL_ERR_SUCCESS) {
            NotifyRadioState(fun == 1 ? HRIL_RADIO_POWER_STATE_ON : HRIL_RADIO_POWER_STATE_OFF);
            if (fun == 1) {
                NetworkRefresh(true);
            }
        }
    });
}

void ReqGetRadioState(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        int32_t st = ReadRadioState();
        if (st == HRIL_RADIO_POWER_STATE_UNAVAILABLE) {
            Ril::Get().Respond(D_MODEM, req, HRIL_ERR_GENERIC_FAILURE, nullptr, 0);
            return;
        }
        Ril::Get().Respond(D_MODEM, req, HRIL_ERR_SUCCESS, &st, sizeof(st));
    });
}

void RespondId(const ReqDataInfo *req, uint8_t tlv)
{
    Ril::Get().Post([req, tlv] {
        QmiMsg m;
        bool sent = Ril::Get().dms.Request(DMS_GET_IDS, m);
        std::string s;
        if (!sent || !m.Ok() || !m.GetStr(tlv, s) || s.empty()) {
            Ril::Get().Respond(D_MODEM, req, sent ? HRIL_ERR_GENERIC_FAILURE : HRIL_ERR_CMD_SEND_FAILURE, nullptr, 0);
            return;
        }
        Ril::Get().Respond(D_MODEM, req, HRIL_ERR_SUCCESS, s.c_str(), s.size());
    });
}

void ReqGetImei(const ReqDataInfo *req)
{
    RespondId(req, 0x11);
}

void ReqGetImeiSv(const ReqDataInfo *req)
{
    RespondId(req, 0x13);
}

void ReqGetMeid(const ReqDataInfo *req)
{
    RespondId(req, 0x12);
}

void ReqGetVoiceRadioTechnology(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        HRilVoiceRadioInfo info = {};
        FillVoiceRadioInfo(info);
        Ril::Get().Respond(D_MODEM, req, HRIL_ERR_SUCCESS, &info, sizeof(info));
    });
}

void ReqGetBasebandVersion(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        bool sent = Ril::Get().dms.Request(DMS_GET_REVISION, m);
        std::string s;
        if (!sent || !m.Ok() || !m.GetStr(0x01, s)) {
            Ril::Get().Respond(D_MODEM, req, HRIL_ERR_GENERIC_FAILURE, nullptr, 0);
            return;
        }
        Ril::Get().Respond(D_MODEM, req, HRIL_ERR_SUCCESS, s.c_str(), s.size());
    });
}

void ReqShutDown(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        Ril::Get().dms.Request(DMS_SET_OPERATING_MODE, TlvWriter().U8(0x01, DMS_MODE_LOW_POWER), m, 10000);
        Ril::Get().Respond(D_MODEM, req, HRIL_ERR_SUCCESS, nullptr, 0);
    });
}
} // namespace

void ModemOnUp()
{
    int st = ReadRadioState();
    Klog("modem up, radio %s", st == HRIL_RADIO_POWER_STATE_ON ? "on" : "off");
    NotifyRadioState(st == HRIL_RADIO_POWER_STATE_UNAVAILABLE ? HRIL_RADIO_POWER_STATE_OFF : st);
}

const HRilModemReq g_modemOps = {
    .SetRadioState = ReqSetRadioState,
    .GetRadioState = ReqGetRadioState,
    .GetImei = ReqGetImei,
    .GetImeiSv = ReqGetImeiSv,
    .GetMeid = ReqGetMeid,
    .GetVoiceRadioTechnology = ReqGetVoiceRadioTechnology,
    .GetBasebandVersion = ReqGetBasebandVersion,
    .ShutDown = ReqShutDown,
};

} // namespace sweetril

/* --------------------------------------------------------- entry point */
extern "C" {
static HRilOps g_ops = {
    .version = HRIL_VERSION,
    .callOps = &sweetril::g_callOps,
    .simOps = &sweetril::g_simOps,
    .smsOps = &sweetril::g_smsOps,
    .dataOps = &sweetril::g_dataOps,
    .networkOps = &sweetril::g_networkOps,
    .modemOps = &sweetril::g_modemOps,
};

__attribute__((visibility("default"))) const HRilOps *RilInitOps(const struct HRilReport *reportOps)
{
    sweetril::Ril::Get().Init(reportOps);
    return &g_ops;
}
}
