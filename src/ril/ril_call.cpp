/*
 * sweet RIL: voice calls (QMI VOICE). Circuit switched calls; the call
 * audio path is set up by the audio HAL (voice call mixer paths).
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "ril.h"

#include <unistd.h>

namespace sweetril {
namespace {

enum : uint8_t {
    CS_ORIGINATION = 0x01,
    CS_INCOMING = 0x02,
    CS_CONVERSATION = 0x03,
    CS_CC_IN_PROGRESS = 0x04,
    CS_ALERTING = 0x05,
    CS_HOLD = 0x06,
    CS_WAITING = 0x07,
    CS_DISCONNECTING = 0x08,
    CS_END = 0x09,
    CS_SETUP = 0x0A,
    CS_PRE_ALERTING = 0x0B,
};
enum : uint8_t {
    SUPS_RELEASE_HELD_OR_WAITING = 0x01,
    SUPS_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING = 0x02,
    SUPS_HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD = 0x03,
    SUPS_HOLD_ALL_EXCEPT_SPECIFIED_CALL = 0x04,
    SUPS_MAKE_CONFERENCE_CALL = 0x05,
    SUPS_EXPLICIT_CALL_TRANSFER = 0x06,
    SUPS_END_ALL_CALLS = 0x08,
    SUPS_RELEASE_SPECIFIED_CALL = 0x09,
};

struct Call {
    uint8_t id = 0;
    uint8_t state = 0;
    uint8_t type = 0;
    uint8_t direction = 0;
    uint8_t mode = 0;
    bool multipart = false;
    uint8_t presentation = 0;
    std::string number;
};

/* Call Information TLV (+ Remote Party Number TLV) of Get All Call Info /
 * All Call Status indication */
std::vector<Call> ParseCalls(const QmiMsg &m, uint8_t infoTlv, uint8_t numberTlv)
{
    std::vector<Call> calls;
    if (!m.Has(infoTlv)) {
        return calls;
    }
    Reader r = m.R(infoTlv);
    uint8_t n = r.U8();
    for (uint8_t i = 0; i < n && r.Ok(); i++) {
        Call c;
        c.id = r.U8();
        c.state = r.U8();
        c.type = r.U8();
        c.direction = r.U8();
        c.mode = r.U8();
        c.multipart = r.U8() != 0;
        r.U8(); /* ALS */
        if (r.Ok()) calls.push_back(c);
    }
    if (m.Has(numberTlv)) {
        Reader p = m.R(numberTlv);
        uint8_t k = p.U8();
        for (uint8_t i = 0; i < k && p.Ok(); i++) {
            uint8_t id = p.U8();
            uint8_t pi = p.U8();
            std::string num = p.Str8();
            for (auto &c : calls) {
                if (c.id == id) {
                    c.presentation = pi;
                    c.number = num;
                }
            }
        }
    }
    return calls;
}

std::vector<Call> CurrentCalls(bool &ok)
{
    QmiMsg m;
    ok = Ril::Get().voice.Request(VOICE_GET_ALL_CALL_INFO, m) && m.Ok();
    if (!ok) return {};
    return ParseCalls(m, 0x10, 0x11);
}

bool Live(const Call &c)
{
    return c.state != CS_END && c.state != CS_DISCONNECTING && c.state != 0;
}

void RespondSimple(const ReqDataInfo *req, int32_t err)
{
    Ril::Get().Respond(D_CALL, req, err, nullptr, 0);
}

void ManageCalls(const ReqDataInfo *req, uint8_t type, int callId = -1)
{
    Ril::Get().Post([req, type, callId] {
        TlvWriter w;
        w.U8(0x01, type);
        if (callId >= 0) {
            w.U8(0x10, static_cast<uint8_t>(callId));
        }
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_MANAGE_CALLS, w, m, 30000);
        RespondSimple(req, QmiErr(sent, m));
    });
}

/* ------------------------------------------------------------ requests */
void ReqGetCallList(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        bool ok;
        std::vector<Call> calls = CurrentCalls(ok);
        if (!ok) {
            RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
            return;
        }
        bool anyActive = false;
        for (auto &c : calls) {
            if (c.state == CS_CONVERSATION || c.state == CS_HOLD) anyActive = true;
        }
        std::vector<HRilCallInfo> out;
        for (auto &c : calls) {
            if (!Live(c)) continue;
            HRilCallInfo ci = {};
            ci.index = c.id;
            ci.dir = c.direction == 2 ? 1 : 0;
            switch (c.state) {
                case CS_CONVERSATION: ci.state = HRIL_CALL_ACTIVATE; break;
                case CS_HOLD: ci.state = HRIL_CALL_HOLDING; break;
                case CS_ALERTING:
                case CS_PRE_ALERTING: ci.state = HRIL_CALL_ALERTING; break;
                case CS_INCOMING:
                case CS_SETUP: ci.state = anyActive ? HRIL_CALL_WAITING : HRIL_CALL_INCOMING; break;
                case CS_WAITING: ci.state = HRIL_CALL_WAITING; break;
                default: ci.state = HRIL_CALL_DIALING; break;
            }
            /* an MT call in SETUP is still ringing */
            if (c.direction == 2 && c.state == CS_CC_IN_PROGRESS) ci.state = HRIL_CALL_INCOMING;
            ci.mode = HRIL_CALL_VOICE;
            ci.mpty = c.multipart ? 1 : 0;
            /* QMI call type 0x02 = VoIP: the call runs over IMS (VoLTE) */
            ci.voiceDomain = c.type == 0x02 ? 1 : 0;
            ci.callType = 0;
            ci.number = const_cast<char *>(c.number.c_str());
            ci.type = (!c.number.empty() && c.number[0] == '+') ? 145 : 129;
            ci.alpha = nullptr;
            ci.name = nullptr;
            ci.namePresentation = c.presentation;
            out.push_back(ci);
        }
        Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, out.empty() ? nullptr : out.data(),
            out.size() * sizeof(HRilCallInfo));
    });
}

void ReqDial(const ReqDataInfo *req, const HRilDial *data, size_t)
{
    std::string number = (data && data->address) ? data->address : "";
    int32_t clirArg = data ? data->clir : 0;
    Ril::Get().Post([req, number, clirArg] {
        if (number.empty()) {
            RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
            return;
        }
        TlvWriter w;
        w.Str(0x01, number);
        /* call type 0 = voice, the modem picks the domain (VoLTE when IMS is registered) */
        w.U8(0x10, 0x00);
        int32_t clir = clirArg;
        if (clir == 0) {
            std::lock_guard<std::mutex> lk(Ril::Get().stateMu);
            clir = Ril::Get().clirMode;
        }
        if (clir == 1 || clir == 2) {
            /* +CLIR 1 = invocation (hide) -> QMI 0x02, 2 = suppression -> QMI 0x01 */
            w.U8(0x11, clir == 1 ? 0x02 : 0x01);
        }
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_DIAL_CALL, w, m, 30000);
        Klog("dial: %s (err %u)", sent && m.Ok() ? "ok" : "failed", m.error);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqHangup(const ReqDataInfo *req, const uint32_t *data, size_t)
{
    int idx = data ? static_cast<int>(*data) : -1;
    Ril::Get().Post([req, idx] {
        int id = idx;
        if (id <= 0) {
            bool ok;
            for (auto &c : CurrentCalls(ok)) {
                if (Live(c)) {
                    id = c.id;
                    break;
                }
            }
        }
        if (id <= 0) {
            RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
            return;
        }
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_END_CALL, TlvWriter().U8(0x01, static_cast<uint8_t>(id)), m);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqReject(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        bool ok;
        int id = -1;
        bool waiting = false;
        for (auto &c : CurrentCalls(ok)) {
            if (c.state == CS_INCOMING || c.state == CS_WAITING || c.state == CS_SETUP ||
                (c.direction == 2 && c.state == CS_CC_IN_PROGRESS)) {
                id = c.id;
                waiting = c.state == CS_WAITING;
            }
        }
        QmiMsg m;
        bool sent;
        if (id < 0) {
            RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
            return;
        }
        if (waiting) {
            /* user determined user busy for the waiting call */
            sent = Ril::Get().voice.Request(VOICE_MANAGE_CALLS,
                TlvWriter().U8(0x01, SUPS_RELEASE_HELD_OR_WAITING), m);
        } else {
            sent = Ril::Get().voice.Request(VOICE_END_CALL, TlvWriter().U8(0x01, static_cast<uint8_t>(id)), m);
        }
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqAnswer(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        bool ok;
        int incoming = -1;
        bool otherActive = false;
        for (auto &c : CurrentCalls(ok)) {
            if (c.state == CS_INCOMING || c.state == CS_WAITING || c.state == CS_SETUP ||
                (c.direction == 2 && c.state == CS_CC_IN_PROGRESS)) {
                incoming = c.id;
            } else if (c.state == CS_CONVERSATION) {
                otherActive = true;
            }
        }
        QmiMsg m;
        bool sent;
        if (incoming < 0) {
            RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
            return;
        }
        if (otherActive) {
            sent = Ril::Get().voice.Request(VOICE_MANAGE_CALLS,
                TlvWriter().U8(0x01, SUPS_HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD), m);
        } else {
            sent = Ril::Get().voice.Request(VOICE_ANSWER_CALL,
                TlvWriter().U8(0x01, static_cast<uint8_t>(incoming)), m);
        }
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqHoldCall(const ReqDataInfo *req)
{
    ManageCalls(req, SUPS_HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD);
}

void ReqUnHoldCall(const ReqDataInfo *req)
{
    ManageCalls(req, SUPS_HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD);
}

void ReqSwitchCall(const ReqDataInfo *req)
{
    ManageCalls(req, SUPS_HOLD_ACTIVE_ACCEPT_WAITING_OR_HELD);
}

void ReqCombineConference(const ReqDataInfo *req, int32_t)
{
    ManageCalls(req, SUPS_MAKE_CONFERENCE_CALL);
}

void ReqSeparateConference(const ReqDataInfo *req, int32_t callIndex, int32_t)
{
    ManageCalls(req, SUPS_HOLD_ALL_EXCEPT_SPECIFIED_CALL, callIndex);
}

void ReqCallSupplement(const ReqDataInfo *req, int32_t type)
{
    ManageCalls(req, type == TYPE_HANG_UP_HOLD_WAIT ? SUPS_RELEASE_HELD_OR_WAITING :
        SUPS_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING);
}

void ReqExplicitCallTransfer(const ReqDataInfo *req)
{
    ManageCalls(req, SUPS_EXPLICIT_CALL_TRANSFER);
}

int ActiveCallId()
{
    bool ok;
    for (auto &c : CurrentCalls(ok)) {
        if (c.state == CS_CONVERSATION) return c.id;
    }
    return 0xFF;
}

void ReqStartDtmf(const ReqDataInfo *req, CallDtmfInfo info)
{
    char digit = (info.dtmfKey && info.dtmfKey[0]) ? info.dtmfKey[0] : '0';
    Ril::Get().Post([req, digit] {
        uint8_t d[2] = {static_cast<uint8_t>(ActiveCallId()), static_cast<uint8_t>(digit)};
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_START_CONT_DTMF, TlvWriter().Raw(0x01, d, 2), m);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqStopDtmf(const ReqDataInfo *req, CallDtmfInfo)
{
    Ril::Get().Post([req] {
        uint8_t d = static_cast<uint8_t>(ActiveCallId());
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_STOP_CONT_DTMF, TlvWriter().Raw(0x01, &d, 1), m);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqSendDtmf(const ReqDataInfo *req, CallDtmfInfo info)
{
    std::string digits = info.dtmfKey ? info.dtmfKey : "";
    if (info.stringLength > 0 && static_cast<size_t>(info.stringLength) < digits.size()) {
        digits.resize(info.stringLength);
    }
    Ril::Get().Post([req, digits] {
        std::vector<uint8_t> d;
        d.push_back(static_cast<uint8_t>(ActiveCallId()));
        d.push_back(static_cast<uint8_t>(digits.size()));
        d.insert(d.end(), digits.begin(), digits.end());
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_BURST_DTMF, TlvWriter().Raw(0x01, d.data(), d.size()), m);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqNotSupported(const ReqDataInfo *req)
{
    RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
}

void ReqNotSupportedInt(const ReqDataInfo *req, int32_t)
{
    RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
}

void ReqOk(const ReqDataInfo *req)
{
    RespondSimple(req, HRIL_ERR_SUCCESS);
}

void ReqOkInt(const ReqDataInfo *req, int32_t)
{
    RespondSimple(req, HRIL_ERR_SUCCESS);
}

void ReqSetMute(const ReqDataInfo *req, int32_t mute)
{
    {
        std::lock_guard<std::mutex> lk(Ril::Get().stateMu);
        Ril::Get().muted = mute != 0;
    }
    RespondSimple(req, HRIL_ERR_SUCCESS);
}

void ReqGetMute(const ReqDataInfo *req)
{
    int32_t v;
    {
        std::lock_guard<std::mutex> lk(Ril::Get().stateMu);
        v = Ril::Get().muted ? 1 : 0;
    }
    Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &v, sizeof(v));
}

void ReqGetEmergencyCallList(const ReqDataInfo *req)
{
    static const char *const numbers[] = {"112", "911", "000", "08", "110", "118", "119", "999"};
    const size_t n = sizeof(numbers) / sizeof(numbers[0]);
    std::vector<HRilEmergencyInfo> list(n);
    for (size_t i = 0; i < n; i++) {
        list[i] = {};
        list[i].index = static_cast<int32_t>(i + 1);
        list[i].total = static_cast<int32_t>(n);
        list[i].eccNum = const_cast<char *>(numbers[i]);
        list[i].category = 0;
        list[i].simpresent = 0; /* with and without SIM */
        list[i].mcc = const_cast<char *>("");
        list[i].abnormalService = 0;
    }
    Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, list.data(), list.size() * sizeof(HRilEmergencyInfo));
}

void ReqGetCallFailReason(const ReqDataInfo *req)
{
    int32_t v;
    {
        std::lock_guard<std::mutex> lk(Ril::Get().stateMu);
        v = Ril::Get().lastCallFailCause;
    }
    Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &v, sizeof(v));
}

void ReqSetEmergencyCallList(const ReqDataInfo *req, HRilEmergencyInfo *, const int)
{
    RespondSimple(req, HRIL_ERR_SUCCESS);
}

void ReqGetIntZero(const ReqDataInfo *req)
{
    int32_t v = 0;
    Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &v, sizeof(v));
}

void ReqGetCallPreferenceMode(const ReqDataInfo *req)
{
    int32_t v = HRIL_CALL_MODE_CS_1ST_PS_2ND;
    Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &v, sizeof(v));
}

void OnCallStatus(const QmiMsg &m)
{
    std::vector<Call> calls = ParseCalls(m, 0x01, 0x10);
    for (auto &c : calls) {
        Klog("call %u state %u dir %u", c.id, c.state, c.direction);
    }
    Ril::Get().Notify(D_CALL, 0, HNOTI_CALL_STATE_UPDATED, nullptr, 0);
}
} // namespace

void CallOnUp()
{
    Ril &r = Ril::Get();
    static bool handlerSet = false;
    if (!handlerSet) {
        handlerSet = true;
        r.voice.OnIndication([](const QmiMsg &m) {
            if (m.msgId == VOICE_ALL_CALL_STATUS_IND) {
                Ril::Get().Post([m] { OnCallStatus(m); });
            }
        });
    }
    TlvWriter w;
    SsOnUp();
    w.U8(0x10, 0).U8(0x12, 1).U8(0x13, 1).U8(0x16, 1);
    QmiMsg m;
    r.voice.Request(VOICE_INDICATION_REGISTER, w, m);
}

HRilCallReq g_callOps = {
    .GetCallList = ReqGetCallList,
    .Dial = ReqDial,
    .Hangup = ReqHangup,
    .Reject = ReqReject,
    .Answer = ReqAnswer,
    .GetClip = nullptr, /* ril_ss.cpp */
    .SetClip = nullptr, /* ril_ss.cpp */
    .HoldCall = ReqHoldCall,
    .UnHoldCall = ReqUnHoldCall,
    .SwitchCall = ReqSwitchCall,
    .CombineConference = ReqCombineConference,
    .SeparateConference = ReqSeparateConference,
    .CallSupplement = ReqCallSupplement,
    .GetCallWaiting = nullptr, /* ril_ss.cpp */
    .SetCallWaiting = nullptr, /* ril_ss.cpp */
    .SetCallTransferInfo = nullptr, /* ril_ss.cpp */
    .GetCallTransferInfo = nullptr, /* ril_ss.cpp */
    .GetCallRestriction = nullptr, /* ril_ss.cpp */
    .SetCallRestriction = nullptr, /* ril_ss.cpp */
    .GetClir = nullptr, /* ril_ss.cpp */
    .SetClir = nullptr, /* ril_ss.cpp */
    .StartDtmf = ReqStartDtmf,
    .SendDtmf = ReqSendDtmf,
    .StopDtmf = ReqStopDtmf,
    .GetImsCallList = nullptr,
    .GetCallPreferenceMode = ReqGetCallPreferenceMode,
    .SetCallPreferenceMode = ReqOkInt,
    .GetLteImsSwitchStatus = ReqGetIntZero,
    .SetLteImsSwitchStatus = ReqOkInt,
    .SetUssd = nullptr, /* ril_ss.cpp */
    .GetUssd = nullptr, /* ril_ss.cpp */
    .SetMute = ReqSetMute,
    .GetMute = ReqGetMute,
    .GetEmergencyCallList = ReqGetEmergencyCallList,
    .GetCallFailReason = ReqGetCallFailReason,
    .SetEmergencyCallList = ReqSetEmergencyCallList,
    .SetBarringPassword = nullptr, /* ril_ss.cpp */
    .CloseUnFinishedUssd = nullptr, /* ril_ss.cpp */
    .ExplicitCallTransferConnection = ReqExplicitCallTransfer,
    .SetVonrSwitch = ReqOkInt,
    .SetSuppSrvNotification = ReqOkInt,
    .GetTTYMode = ReqGetIntZero,
    .SetTTYMode = ReqOkInt,
};

} // namespace sweetril
