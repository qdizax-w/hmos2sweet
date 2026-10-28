/*
 * sweet RIL: SMS (QMI WMS), 3GPP point to point messages.
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "ril.h"

#include <cstdlib>
#include <deque>

namespace sweetril {
namespace {

enum : uint8_t {
    FMT_GW_PP = 0x06,
    STORAGE_UIM = 0x00,
    STORAGE_NV = 0x01,
    STORAGE_NONE = 0xFF,
    ACTION_TRANSFER_ONLY = 0x02,
    PROTOCOL_WCDMA = 0x01,
    MODE_GW = 0x01,
};
constexpr uint32_t NO_ACK = 0xFFFFFFFFu;

std::mutex g_ackMu;
std::deque<uint32_t> g_pendingAcks;

void RespondSimple(const ReqDataInfo *req, int32_t err)
{
    Ril::Get().Respond(D_SMS, req, err, nullptr, 0);
}

/* raw 3GPP message = SMSC address (length-prefixed, "00" = default) + TPDU */
std::vector<uint8_t> BuildRaw(const std::string &smscHex, const std::string &pduHex)
{
    std::vector<uint8_t> raw;
    std::vector<uint8_t> smsc = HexDecode(smscHex.c_str());
    if (smsc.empty()) {
        raw.push_back(0x00);
    } else {
        raw.insert(raw.end(), smsc.begin(), smsc.end());
    }
    std::vector<uint8_t> pdu = HexDecode(pduHex.c_str());
    raw.insert(raw.end(), pdu.begin(), pdu.end());
    return raw;
}

void DeliverIncoming(const std::vector<uint8_t> &raw, uint32_t ackTid)
{
    if (raw.empty()) return;
    /* TP-MTI of the first TPDU octet after the SMSC address */
    size_t smscLen = raw[0];
    uint8_t mti = (smscLen + 1 < raw.size()) ? (raw[smscLen + 1] & 0x03) : 0;
    std::string hex = HexEncode(raw);
    {
        std::lock_guard<std::mutex> lk(g_ackMu);
        g_pendingAcks.push_back(ackTid);
    }
    Ril &r = Ril::Get();
    if (mti == 0x02) {
        Klog("SMS status report (%zu bytes)", raw.size());
        r.Notify(D_SMS, 0, HNOTI_SMS_STATUS_REPORT, hex.c_str(), hex.size());
    } else {
        Klog("new SMS (%zu bytes)", raw.size());
        HRilSmsResponse rsp = {};
        rsp.pdu = const_cast<char *>(hex.c_str());
        r.Notify(D_SMS, 0, HNOTI_SMS_NEW_SMS, &rsp, hex.size());
    }
}

void ReadStoredAndDeliver(uint8_t storage, uint32_t index)
{
    std::vector<uint8_t> w(5);
    w[0] = storage;
    for (int i = 0; i < 4; i++) w[1 + i] = static_cast<uint8_t>(index >> (8 * i));
    TlvWriter t;
    t.Raw(0x01, w.data(), w.size());
    t.U8(0x10, MODE_GW);
    QmiMsg m;
    if (!Ril::Get().wms.Request(WMS_RAW_READ, t, m) || !m.Ok() || !m.Has(0x01)) {
        return;
    }
    Reader r = m.R(0x01);
    r.U8(); /* tag */
    uint8_t fmt = r.U8();
    std::vector<uint8_t> raw = r.Bytes(r.U16());
    if (fmt == FMT_GW_PP && r.Ok()) {
        DeliverIncoming(raw, NO_ACK);
        /* the message lives on in the inbox now: free the slot */
        TlvWriter d;
        d.U8(0x01, storage).U32(0x10, index).U8(0x12, MODE_GW);
        QmiMsg dm;
        Ril::Get().wms.Request(WMS_DELETE, d, dm);
    }
}

void OnEventReport(const QmiMsg &m)
{
    if (m.Has(0x11)) {
        Reader r = m.R(0x11);
        uint8_t ackInd = r.U8();
        uint32_t tid = r.U32();
        uint8_t fmt = r.U8();
        std::vector<uint8_t> raw = r.Bytes(r.U16());
        if (!r.Ok() || fmt != FMT_GW_PP) {
            return;
        }
        DeliverIncoming(raw, ackInd == 0 ? tid : NO_ACK);
    } else if (m.Has(0x10)) {
        Reader r = m.R(0x10);
        uint8_t storage = r.U8();
        uint32_t index = r.U32();
        if (r.Ok()) {
            ReadStoredAndDeliver(storage, index);
        }
    }
}

/* ------------------------------------------------------------ requests */
void ReqSendGsmSms(const ReqDataInfo *req, const char *const *data, size_t)
{
    std::string smsc = (data && data[0]) ? data[0] : "";
    std::string pdu = (data && data[1]) ? data[1] : "";
    Ril::Get().Post([req, smsc, pdu] {
        std::vector<uint8_t> raw = BuildRaw(smsc, pdu);
        std::vector<uint8_t> v;
        v.push_back(FMT_GW_PP);
        v.push_back(static_cast<uint8_t>(raw.size()));
        v.push_back(static_cast<uint8_t>(raw.size() >> 8));
        v.insert(v.end(), raw.begin(), raw.end());
        QmiMsg m;
        bool sent = Ril::Get().wms.Request(WMS_RAW_SEND, TlvWriter().Raw(0x01, v.data(), v.size()), m, 60000);
        HRilSmsResponse rsp = {};
        uint16_t ref = 0;
        m.GetU16(0x01, ref);
        rsp.msgRef = ref;
        rsp.pdu = nullptr;
        rsp.errCode = -1;
        if (m.Has(0x12)) {
            Reader r = m.R(0x12);
            rsp.errCode = r.U16();
        }
        int32_t err = QmiErr(sent, m);
        Klog("send SMS: %s (ref %u, err %u)", err == 0 ? "ok" : "failed", ref, m.error);
        Ril::Get().Respond(D_SMS, req, err, &rsp, sizeof(rsp));
    });
}

void ReqSendSmsAck(const ReqDataInfo *req, const int32_t *data, size_t)
{
    bool success = data ? data[0] != 0 : true;
    int32_t cause = data ? data[1] : 0;
    Ril::Get().Post([req, success, cause] {
        uint32_t tid = NO_ACK;
        {
            std::lock_guard<std::mutex> lk(g_ackMu);
            if (!g_pendingAcks.empty()) {
                tid = g_pendingAcks.front();
                g_pendingAcks.pop_front();
            }
        }
        if (tid == NO_ACK) {
            RespondSimple(req, HRIL_ERR_SUCCESS);
            return;
        }
        uint8_t info[6] = {static_cast<uint8_t>(tid), static_cast<uint8_t>(tid >> 8), static_cast<uint8_t>(tid >> 16),
            static_cast<uint8_t>(tid >> 24), PROTOCOL_WCDMA, static_cast<uint8_t>(success ? 1 : 0)};
        TlvWriter w;
        w.Raw(0x01, info, 6);
        if (!success) {
            /* RP cause 22 memory capacity exceeded / 111 unspecified, TP cause */
            uint8_t f[2] = {static_cast<uint8_t>(cause == 0xD3 ? 22 : 111), static_cast<uint8_t>(cause ? cause : 0xFF)};
            w.Raw(0x11, f, 2);
        }
        QmiMsg m;
        bool sent = Ril::Get().wms.Request(WMS_SEND_ACK, w, m);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqWriteSimMessage(const ReqDataInfo *req, const HRilSmsWriteSms *data, size_t)
{
    if (data == nullptr) {
        RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
        return;
    }
    std::string smsc = data->smsc ? data->smsc : "";
    std::string pdu = data->pdu ? data->pdu : "";
    Ril::Get().Post([req, smsc, pdu] {
        std::vector<uint8_t> raw = BuildRaw(smsc, pdu);
        std::vector<uint8_t> v;
        v.push_back(STORAGE_UIM);
        v.push_back(FMT_GW_PP);
        v.push_back(static_cast<uint8_t>(raw.size()));
        v.push_back(static_cast<uint8_t>(raw.size() >> 8));
        v.insert(v.end(), raw.begin(), raw.end());
        QmiMsg m;
        bool sent = Ril::Get().wms.Request(WMS_RAW_WRITE, TlvWriter().Raw(0x01, v.data(), v.size()), m);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqDelSimMessage(const ReqDataInfo *req, const int32_t *data, size_t)
{
    int32_t index = data ? *data : 0;
    Ril::Get().Post([req, index] {
        TlvWriter w;
        w.U8(0x01, STORAGE_UIM).U32(0x10, static_cast<uint32_t>(index > 0 ? index - 1 : 0)).U8(0x12, MODE_GW);
        QmiMsg m;
        bool sent = Ril::Get().wms.Request(WMS_DELETE, w, m);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqSetSmscAddr(const ReqDataInfo *req, const HRilServiceCenterAddress *data, size_t)
{
    if (data == nullptr || data->address == nullptr) {
        RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
        return;
    }
    std::string addr = data->address;
    int32_t tosca = data->tosca;
    Ril::Get().Post([req, addr, tosca] {
        TlvWriter w;
        w.Str(0x01, addr);
        char type[4];
        snprintf(type, sizeof(type), "%03d", tosca > 0 ? tosca : (addr[0] == '+' ? 145 : 129));
        w.Raw(0x10, type, 3);
        QmiMsg m;
        bool sent = Ril::Get().wms.Request(WMS_SET_SMSC_ADDRESS, w, m);
        RespondSimple(req, QmiErr(sent, m));
    });
}

void ReqGetSmscAddr(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        bool sent = Ril::Get().wms.Request(WMS_GET_SMSC_ADDRESS, m);
        if (!sent || !m.Ok() || !m.Has(0x01)) {
            RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
            return;
        }
        Reader r = m.R(0x01);
        std::string type = r.Str(3);
        std::string addr = r.Str8();
        HRilServiceCenterAddress a = {};
        a.tosca = atoi(type.c_str());
        if (a.tosca == 0) a.tosca = (!addr.empty() && addr[0] == '+') ? 145 : 129;
        a.address = const_cast<char *>(addr.c_str());
        Ril::Get().Respond(D_SMS, req, HRIL_ERR_SUCCESS, &a, sizeof(a));
    });
}

void ReqSetCBConfig(const ReqDataInfo *req, const HRilCBConfigInfo *, size_t)
{
    RespondSimple(req, HRIL_ERR_SUCCESS);
}

void ReqNotImplemented(const ReqDataInfo *req)
{
    RespondSimple(req, HRIL_ERR_VENDOR_NOT_IMPLEMENT);
}

void ReqCdmaNotSupported(const ReqDataInfo *req, const char *, size_t)
{
    RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
}

void ReqCdmaCbNotSupported(const ReqDataInfo *req, const HRilCdmaCBConfigInfo *, size_t)
{
    RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
}

void ReqCdmaWriteNotSupported(const ReqDataInfo *req, const HRilSmsWriteSms *, size_t)
{
    RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
}

void ReqCdmaWriteV2NotSupported(const ReqDataInfo *req, const HRilSmsWriteCdmaSms *, size_t)
{
    RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
}

void ReqCdmaDelNotSupported(const ReqDataInfo *req, const int32_t *, size_t)
{
    RespondSimple(req, HRIL_ERR_GENERIC_FAILURE);
}

void ReqSetSmsMemoryStatus(const ReqDataInfo *req, int32_t available)
{
    Ril::Get().Post([req, available] {
        QmiMsg m;
        bool sent = Ril::Get().wms.Request(WMS_SET_MEMORY_STATUS,
            TlvWriter().U8(0x01, available ? 1 : 0), m);
        RespondSimple(req, sent ? HRIL_ERR_SUCCESS : HRIL_ERR_CMD_SEND_FAILURE);
    });
}
} // namespace

void SmsOnUp()
{
    Ril &r = Ril::Get();
    static bool handlerSet = false;
    if (!handlerSet) {
        handlerSet = true;
        r.wms.OnIndication([](const QmiMsg &m) {
            if (m.msgId == WMS_EVENT_REPORT_IND) {
                Ril::Get().Post([m] { OnEventReport(m); });
            }
        });
    }
    /* every 3GPP class comes to us (transfer only); we ack after the
     * telephony stack has stored it */
    std::vector<uint8_t> routes;
    const uint8_t classes[] = {0, 1, 2, 3, 4};
    routes.push_back(sizeof(classes));
    routes.push_back(0);
    for (uint8_t c : classes) {
        routes.push_back(0x00);                  /* point to point */
        routes.push_back(c);                     /* message class */
        routes.push_back(c == 2 ? STORAGE_UIM : STORAGE_NONE);
        routes.push_back(c == 2 ? 0x01 : ACTION_TRANSFER_ONLY); /* class 2: store on SIM and notify */
    }
    TlvWriter w;
    w.Raw(0x01, routes.data(), routes.size());
    w.U8(0x10, 0x01); /* status reports to the client */
    QmiMsg m;
    r.wms.Request(WMS_SET_ROUTES, w, m);
    uint8_t on = 1;
    QmiMsg e;
    r.wms.Request(WMS_SET_EVENT_REPORT, TlvWriter().Raw(0x10, &on, 1), e);
    Klog("SMS: routes %s, event report %s", m.Ok() ? "set" : "FAILED", e.Ok() ? "on" : "FAILED");
}

const HRilSmsReq g_smsOps = {
    .SendGsmSms = ReqSendGsmSms,
    .SendSmsAck = ReqSendSmsAck,
    .SendCdmaSms = ReqCdmaNotSupported,
    .SendCdmaAck = ReqCdmaNotSupported,
    .AddSimMessage = ReqWriteSimMessage,
    .DelSimMessage = ReqDelSimMessage,
    .UpdateSimMessage = ReqWriteSimMessage,
    .SetSmscAddr = ReqSetSmscAddr,
    .GetSmscAddr = ReqGetSmscAddr,
    .SetCBConfig = ReqSetCBConfig,
    .GetCBConfig = ReqNotImplemented,
    .GetCdmaCBConfig = ReqNotImplemented,
    .SetCdmaCBConfig = ReqCdmaCbNotSupported,
    .AddCdmaSimMessage = ReqCdmaWriteNotSupported,
    .AddCdmaSimMessageV2 = ReqCdmaWriteV2NotSupported,
    .DelCdmaSimMessage = ReqCdmaDelNotSupported,
    .UpdateCdmaSimMessage = ReqCdmaWriteNotSupported,
    .SetSmsMemoryStatus = ReqSetSmsMemoryStatus,
};

} // namespace sweetril
