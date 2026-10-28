/*
 * sweet RIL: supplementary services and USSD (QMI VOICE).
 *
 * Call forwarding, call waiting, call barring, CLIP/CLIR and USSD are all
 * handled by the modem: the requests below only translate the 27.007 style
 * values of the HarmonyOS RIL interface into QMI VOICE messages.
 *
 * Message ids/TLVs: Set Supplementary Service 0x33, Get Call Waiting 0x34,
 * Get Call Barring 0x35, Get CLIP 0x36, Get CLIR 0x37, Get Call Forwarding
 * 0x38, Set Call Barring Password 0x39, USSD 0x3A-0x3E, Originate USSD No
 * Wait 0x43.
 *
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "ril.h"

#include <cstring>

namespace sweetril {
namespace {

enum : uint16_t {
    VOICE_SET_SUPS = 0x0033,
    VOICE_GET_CALL_WAITING = 0x0034,
    VOICE_GET_CALL_BARRING = 0x0035,
    VOICE_GET_CLIP = 0x0036,
    VOICE_GET_CLIR = 0x0037,
    VOICE_GET_CALL_FORWARDING = 0x0038,
    VOICE_SET_BARRING_PASSWORD = 0x0039,
    VOICE_ORIG_USSD = 0x003A,
    VOICE_ANSWER_USSD = 0x003B,
    VOICE_CANCEL_USSD = 0x003C,
    VOICE_RELEASE_USSD_IND = 0x003D,
    VOICE_USSD_IND = 0x003E,
    VOICE_ORIG_USSD_NO_WAIT = 0x0043,
};

enum : uint8_t {
    SUPS_ACTIVATE = 0x01,
    SUPS_DEACTIVATE = 0x02,
    SUPS_REGISTER = 0x03,
    SUPS_ERASE = 0x04,
};

enum : uint8_t {
    REASON_FWD_UNCONDITIONAL = 0x01, /* +1 = 27.007 reason */
    REASON_BAR_ALL_OUTGOING = 0x07,
    REASON_BAR_OUTGOING_INT = 0x08,
    REASON_BAR_OUTGOING_INT_EXT_HOME = 0x09,
    REASON_BAR_ALL_INCOMING = 0x0A,
    REASON_BAR_INCOMING_ROAMING = 0x0B,
    REASON_BAR_ALL = 0x0C,
    REASON_BAR_ALL_OUTGOING_SERVICES = 0x0D,
    REASON_BAR_ALL_INCOMING_SERVICES = 0x0E,
    REASON_CALL_WAITING = 0x0F,
};

enum : uint8_t {
    USS_DCS_ASCII = 0x01,
    USS_DCS_8BIT = 0x02,
    USS_DCS_UCS2 = 0x03,
};

constexpr uint8_t SERVICE_CLASS_VOICE = 0x01;

void RespondSimple(const ReqDataInfo *req, int32_t err)
{
    Ril::Get().Respond(D_CALL, req, err, nullptr, 0);
}

/* 27.007 facility -> QMI barring reason (0 = unknown) */
uint8_t BarringReason(const char *fac)
{
    static const struct {
        const char *fac;
        uint8_t reason;
    } map[] = {
        {"AO", REASON_BAR_ALL_OUTGOING},
        {"OI", REASON_BAR_OUTGOING_INT},
        {"OX", REASON_BAR_OUTGOING_INT_EXT_HOME},
        {"AI", REASON_BAR_ALL_INCOMING},
        {"IR", REASON_BAR_INCOMING_ROAMING},
        {"AB", REASON_BAR_ALL},
        {"AG", REASON_BAR_ALL_OUTGOING_SERVICES},
        {"AC", REASON_BAR_ALL_INCOMING_SERVICES},
    };
    if (fac == nullptr) {
        return 0;
    }
    for (auto &e : map) {
        if (strcasecmp(fac, e.fac) == 0) {
            return e.reason;
        }
    }
    return 0;
}

/* the 4-digit barring password as the modem wants it */
bool Password(const char *pw, uint8_t out[4])
{
    if (pw == nullptr || strlen(pw) != 4) {
        return false;
    }
    memcpy(out, pw, 4);
    return true;
}

uint8_t ServiceClass(int32_t classx)
{
    /* 27.007 class bitmask; 0 or 255 = "all", the modem wants voice for calls */
    if (classx <= 0 || classx >= 255) {
        return SERVICE_CLASS_VOICE;
    }
    return static_cast<uint8_t>(classx);
}

int32_t SupsErr(bool sent, const QmiMsg &m, const char *what)
{
    int32_t err = QmiErr(sent, m);
    Klog("ss: %s %s (err %u)", what, err == HRIL_ERR_SUCCESS ? "ok" : "failed", m.error);
    return err;
}

/* ------------------------------------------------------- call waiting */
void ReqGetCallWaiting(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_GET_CALL_WAITING, TlvWriter().U8(0x10, SERVICE_CLASS_VOICE), m,
            30000);
        int32_t err = SupsErr(sent, m, "get call waiting");
        if (err != HRIL_ERR_SUCCESS) {
            RespondSimple(req, err);
            return;
        }
        uint8_t cls = 0;
        m.GetU8(0x10, cls);
        HRilCallWaitResult r = {};
        r.status = cls != 0 ? 1 : 0;
        r.classCw = cls;
        Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &r, sizeof(r));
    });
}

void ReqSetCallWaiting(const ReqDataInfo *req, int32_t active)
{
    Ril::Get().Post([req, active] {
        uint8_t info[2] = {static_cast<uint8_t>(active ? SUPS_ACTIVATE : SUPS_DEACTIVATE), REASON_CALL_WAITING};
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_SET_SUPS,
            TlvWriter().Raw(0x01, info, 2).U8(0x10, SERVICE_CLASS_VOICE), m, 30000);
        RespondSimple(req, SupsErr(sent, m, "set call waiting"));
    });
}

/* ----------------------------------------------------- call forwarding */
void ReqSetCallTransferInfo(const ReqDataInfo *req, HRilCFInfo info)
{
    std::string number = info.number ? info.number : "";
    int32_t reason = info.reason, mode = info.mode, classx = info.classx, time = info.time;
    Ril::Get().Post([req, number, reason, mode, classx, time] {
        if (reason < 0 || reason > 5) {
            RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
            return;
        }
        uint8_t action;
        switch (mode) {
            case 0: action = SUPS_DEACTIVATE; break;
            case 1: action = SUPS_ACTIVATE; break;
            case 3: action = SUPS_REGISTER; break;
            case 4: action = SUPS_ERASE; break;
            default:
                RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
                return;
        }
        /* enabling forwarding with a number is a registration */
        if (action == SUPS_ACTIVATE && !number.empty()) {
            action = SUPS_REGISTER;
        }
        uint8_t sups[2] = {action, static_cast<uint8_t>(REASON_FWD_UNCONDITIONAL + reason)};
        TlvWriter w;
        w.Raw(0x01, sups, 2).U8(0x10, ServiceClass(classx));
        if (!number.empty() && (action == SUPS_REGISTER || action == SUPS_ACTIVATE)) {
            std::vector<uint8_t> num = {static_cast<uint8_t>(number.size())};
            num.insert(num.end(), number.begin(), number.end());
            w.Raw(0x12, num.data(), num.size());
        }
        if (reason == 2 && time > 0) {
            w.U8(0x13, static_cast<uint8_t>(time > 30 ? 30 : time));
        }
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_SET_SUPS, w, m, 30000);
        RespondSimple(req, SupsErr(sent, m, "set call forwarding"));
    });
}

void ReqGetCallTransferInfo(const ReqDataInfo *req, int32_t reason)
{
    Ril::Get().Post([req, reason] {
        if (reason < 0 || reason > 5) {
            RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
            return;
        }
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_GET_CALL_FORWARDING,
            TlvWriter().U8(0x01, static_cast<uint8_t>(REASON_FWD_UNCONDITIONAL + reason)).U8(0x10,
                SERVICE_CLASS_VOICE), m, 30000);
        int32_t err = SupsErr(sent, m, "get call forwarding");
        if (err != HRIL_ERR_SUCCESS) {
            RespondSimple(req, err);
            return;
        }
        struct Entry {
            uint8_t status, cls, timer;
            std::string number;
        };
        std::vector<Entry> entries;
        if (m.Has(0x10)) {
            Reader r = m.R(0x10);
            uint8_t n = r.U8();
            for (uint8_t i = 0; i < n && r.Ok(); i++) {
                Entry e;
                e.status = r.U8();
                e.cls = r.U8();
                e.number = r.Str8();
                e.timer = r.U8();
                if (r.Ok()) entries.push_back(e);
            }
        }
        std::vector<HRilCFQueryInfo> out;
        if (entries.empty()) {
            HRilCFQueryInfo q = {};
            q.reason = reason;
            q.status = 0;
            q.classx = SERVICE_CLASS_VOICE;
            q.number = const_cast<char *>("");
            q.type = 129;
            q.time = 0;
            out.push_back(q);
        }
        for (auto &e : entries) {
            HRilCFQueryInfo q = {};
            q.reason = reason;
            q.status = e.status ? 1 : 0;
            q.classx = e.cls ? e.cls : SERVICE_CLASS_VOICE;
            q.number = const_cast<char *>(e.number.c_str());
            q.type = (!e.number.empty() && e.number[0] == '+') ? 145 : 129;
            q.time = e.timer;
            out.push_back(q);
        }
        Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, out.data(), out.size() * sizeof(HRilCFQueryInfo));
    });
}

/* -------------------------------------------------------- call barring */
void ReqGetCallRestriction(const ReqDataInfo *req, const char *fac)
{
    uint8_t reason = BarringReason(fac);
    Ril::Get().Post([req, reason] {
        if (reason == 0) {
            RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
            return;
        }
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_GET_CALL_BARRING,
            TlvWriter().U8(0x01, reason).U8(0x10, SERVICE_CLASS_VOICE), m, 30000);
        int32_t err = SupsErr(sent, m, "get call barring");
        if (err != HRIL_ERR_SUCCESS) {
            RespondSimple(req, err);
            return;
        }
        uint8_t cls = 0;
        m.GetU8(0x10, cls);
        HRilCallRestrictionResult r = {};
        r.status = cls != 0 ? 1 : 0;
        r.classCw = cls;
        Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &r, sizeof(r));
    });
}

void ReqSetCallRestriction(const ReqDataInfo *req, CallRestrictionInfo info)
{
    uint8_t reason = BarringReason(info.fac);
    uint8_t pw[4] = {};
    bool havePw = Password(info.password, pw);
    int32_t mode = info.mode;
    Ril::Get().Post([req, reason, havePw, mode, pw0 = pw[0], pw1 = pw[1], pw2 = pw[2], pw3 = pw[3]] {
        if (reason == 0) {
            RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
            return;
        }
        uint8_t sups[2] = {static_cast<uint8_t>(mode ? SUPS_ACTIVATE : SUPS_DEACTIVATE), reason};
        TlvWriter w;
        w.Raw(0x01, sups, 2).U8(0x10, SERVICE_CLASS_VOICE);
        if (havePw) {
            uint8_t p[4] = {pw0, pw1, pw2, pw3};
            w.Raw(0x11, p, 4);
        }
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_SET_SUPS, w, m, 30000);
        RespondSimple(req, SupsErr(sent, m, "set call barring"));
    });
}

void ReqSetBarringPassword(const ReqDataInfo *req, HRilSetBarringInfo info)
{
    uint8_t reason = BarringReason(info.fac);
    uint8_t oldPw[4], newPw[4];
    bool ok = Password(info.oldPassword, oldPw) && Password(info.newPassword, newPw);
    if (reason == 0) {
        reason = REASON_BAR_ALL; /* "AB": the password is shared by all barring services */
    }
    std::vector<uint8_t> body = {reason};
    if (ok) {
        body.insert(body.end(), oldPw, oldPw + 4);
        body.insert(body.end(), newPw, newPw + 4);
        body.insert(body.end(), newPw, newPw + 4); /* "new password again" */
    }
    Ril::Get().Post([req, ok, body] {
        if (!ok) {
            RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
            return;
        }
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_SET_BARRING_PASSWORD, TlvWriter().Raw(0x01, body.data(),
            body.size()), m, 30000);
        RespondSimple(req, SupsErr(sent, m, "set barring password"));
    });
}

/* ----------------------------------------------------------- CLIP/CLIR */
int32_t g_clipAction = 1;

void ReqGetClip(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_GET_CLIP, m, 30000);
        int32_t err = SupsErr(sent, m, "get clip");
        if (err != HRIL_ERR_SUCCESS) {
            RespondSimple(req, err);
            return;
        }
        HRilGetClipResult r = {};
        r.action = g_clipAction;
        r.clipStat = 2; /* unknown */
        if (m.Has(0x10)) {
            Reader rd = m.R(0x10);
            rd.U8(); /* active */
            uint8_t prov = rd.U8();
            if (rd.Ok()) r.clipStat = prov ? 1 : 0;
        }
        Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &r, sizeof(r));
    });
}

void ReqSetClip(const ReqDataInfo *req, int32_t action)
{
    g_clipAction = action; /* only the local presentation of the +CLIP result */
    RespondSimple(req, HRIL_ERR_SUCCESS);
}

void ReqGetClir(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_GET_CLIR, m, 30000);
        int32_t err = SupsErr(sent, m, "get clir");
        if (err != HRIL_ERR_SUCCESS) {
            RespondSimple(req, err);
            return;
        }
        HRilGetCallClirResult r = {};
        {
            std::lock_guard<std::mutex> lk(Ril::Get().stateMu);
            r.action = Ril::Get().clirMode;
        }
        r.clirStat = 2; /* unknown */
        if (m.Has(0x10)) {
            Reader rd = m.R(0x10);
            rd.U8(); /* active */
            uint8_t prov = rd.U8();
            if (rd.Ok()) {
                /* QMI: 0 not provisioned, 1 permanent, 2 temp. restricted, 3 temp. allowed */
                static const int32_t map[] = {0, 1, 3, 4};
                r.clirStat = prov < 4 ? map[prov] : 2;
            }
        }
        Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &r, sizeof(r));
    });
}

void ReqSetClir(const ReqDataInfo *req, int32_t action)
{
    /* +CLIR n: 0 network default, 1 invocation (hide number), 2 suppression;
     * applied to every following Dial Call */
    {
        std::lock_guard<std::mutex> lk(Ril::Get().stateMu);
        Ril::Get().clirMode = action;
    }
    RespondSimple(req, HRIL_ERR_SUCCESS);
}

/* ---------------------------------------------------------------- USSD */
bool g_ussdActive = false;
int32_t g_cusd = 1;

std::string Utf8FromUtf16(const std::vector<uint16_t> &u)
{
    std::string s;
    for (size_t i = 0; i < u.size(); i++) {
        uint32_t c = u[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < u.size()) {
            c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00);
            i++;
        }
        if (c < 0x80) {
            s += static_cast<char>(c);
        } else if (c < 0x800) {
            s += static_cast<char>(0xC0 | (c >> 6));
            s += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            s += static_cast<char>(0xE0 | (c >> 12));
            s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            s += static_cast<char>(0xF0 | (c >> 18));
            s += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return s;
}

/* text of a USS Data TLV ({dcs, u8 len, data}) or USS Data UTF16 TLV */
std::string UssText(const QmiMsg &m, uint8_t ussTlv, uint8_t utf16Tlv)
{
    if (m.Has(utf16Tlv)) {
        Reader r = m.R(utf16Tlv);
        uint8_t n = r.U8();
        std::vector<uint16_t> u;
        for (uint8_t i = 0; i < n && r.Ok(); i++) {
            u.push_back(r.U16());
        }
        if (r.Ok() && !u.empty()) {
            return Utf8FromUtf16(u);
        }
    }
    if (m.Has(ussTlv)) {
        Reader r = m.R(ussTlv);
        uint8_t dcs = r.U8();
        uint8_t n = r.U8();
        std::vector<uint8_t> d = r.Bytes(n);
        if (dcs == USS_DCS_UCS2) {
            std::vector<uint16_t> u;
            for (size_t i = 0; i + 1 < d.size(); i += 2) {
                u.push_back(static_cast<uint16_t>(d[i] << 8 | d[i + 1]));
            }
            return Utf8FromUtf16(u);
        }
        return std::string(d.begin(), d.end());
    }
    return "";
}

void NotifyUssd(int32_t type, const std::string &text)
{
    HRilUssdNoticeInfo n = {};
    n.m = type;
    n.str = const_cast<char *>(text.c_str());
    Klog("ussd: notice type %d, %zu chars", type, text.size());
    Ril::Get().Notify(D_CALL, 0, HNOTI_CALL_USSD_REPORT, &n, sizeof(n));
}

std::vector<uint8_t> UssData(const std::string &s)
{
    bool ascii = true;
    for (unsigned char c : s) {
        if (c >= 0x80) ascii = false;
    }
    std::vector<uint8_t> d;
    if (ascii) {
        d.push_back(USS_DCS_ASCII);
        d.push_back(static_cast<uint8_t>(s.size()));
        d.insert(d.end(), s.begin(), s.end());
        return d;
    }
    /* UCS2 big endian from UTF-8 (BMP only) */
    std::vector<uint8_t> u;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        uint32_t cp;
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            i += 3;
        } else {
            cp = '?';
            i += 1;
        }
        u.push_back(static_cast<uint8_t>(cp >> 8));
        u.push_back(static_cast<uint8_t>(cp));
    }
    d.push_back(USS_DCS_UCS2);
    d.push_back(static_cast<uint8_t>(u.size()));
    d.insert(d.end(), u.begin(), u.end());
    return d;
}

void ReqSetUssd(const ReqDataInfo *req, const char *str)
{
    std::string s = str ? str : "";
    Ril::Get().Post([req, s] {
        if (s.empty() || s.size() > 160) {
            RespondSimple(req, HRIL_ERR_INVALID_PARAMETER);
            return;
        }
        std::vector<uint8_t> d = UssData(s);
        QmiMsg m;
        bool sent;
        if (g_ussdActive) {
            /* reply inside a network-initiated / ongoing session */
            sent = Ril::Get().voice.Request(VOICE_ANSWER_USSD, TlvWriter().Raw(0x01, d.data(), d.size()), m, 30000);
        } else {
            /* the result arrives as an Originate USSD No Wait indication */
            sent = Ril::Get().voice.Request(VOICE_ORIG_USSD_NO_WAIT, TlvWriter().Raw(0x01, d.data(), d.size()), m,
                30000);
            if (sent && m.Ok()) {
                g_ussdActive = true;
            }
        }
        RespondSimple(req, SupsErr(sent, m, "ussd"));
    });
}

void ReqGetUssd(const ReqDataInfo *req)
{
    Ril::Get().Respond(D_CALL, req, HRIL_ERR_SUCCESS, &g_cusd, sizeof(g_cusd));
}

void ReqCloseUnFinishedUssd(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        bool sent = Ril::Get().voice.Request(VOICE_CANCEL_USSD, m, 15000);
        g_ussdActive = false;
        /* nothing to cancel is fine too */
        RespondSimple(req, sent ? HRIL_ERR_SUCCESS : HRIL_ERR_CMD_SEND_FAILURE);
    });
}

void OnUssdIndication(const QmiMsg &m)
{
    switch (m.msgId) {
        case VOICE_USSD_IND: {
            uint8_t action = 0;
            m.GetU8(0x01, action);
            /* 2 = the network expects an answer */
            bool more = action == 0x02;
            g_ussdActive = more;
            NotifyUssd(more ? 1 : 0, UssText(m, 0x10, 0x11));
            break;
        }
        case VOICE_ORIG_USSD_NO_WAIT: {
            uint16_t err = 0;
            if (m.GetU16(0x10, err) && err != 0) {
                g_ussdActive = false;
                NotifyUssd(4, "");
                break;
            }
            g_ussdActive = false;
            NotifyUssd(0, UssText(m, 0x12, 0x14));
            break;
        }
        case VOICE_RELEASE_USSD_IND:
            g_ussdActive = false;
            NotifyUssd(2, "");
            break;
        default:
            break;
    }
}
} // namespace

void SsOnUp()
{
    static bool handlerSet = false;
    if (!handlerSet) {
        handlerSet = true;
        Ril::Get().voice.OnIndication([](const QmiMsg &m) {
            if (m.msgId == VOICE_USSD_IND || m.msgId == VOICE_ORIG_USSD_NO_WAIT || m.msgId == VOICE_RELEASE_USSD_IND) {
                Ril::Get().Post([m] { OnUssdIndication(m); });
            }
        });
    }
}

void InstallSsOps(HRilCallReq &ops)
{
    ops.GetClip = ReqGetClip;
    ops.SetClip = ReqSetClip;
    ops.GetClir = ReqGetClir;
    ops.SetClir = ReqSetClir;
    ops.GetCallWaiting = ReqGetCallWaiting;
    ops.SetCallWaiting = ReqSetCallWaiting;
    ops.SetCallTransferInfo = ReqSetCallTransferInfo;
    ops.GetCallTransferInfo = ReqGetCallTransferInfo;
    ops.GetCallRestriction = ReqGetCallRestriction;
    ops.SetCallRestriction = ReqSetCallRestriction;
    ops.SetBarringPassword = ReqSetBarringPassword;
    ops.SetUssd = ReqSetUssd;
    ops.GetUssd = ReqGetUssd;
    ops.CloseUnFinishedUssd = ReqCloseUnFinishedUssd;
}

} // namespace sweetril
