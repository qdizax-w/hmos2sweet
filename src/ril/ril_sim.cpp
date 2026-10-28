/*
 * sweet RIL: SIM card (QMI UIM).
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "ril.h"

#include <cstdlib>

namespace sweetril {
namespace {

enum : uint8_t {
    UIM_SESSION_PRIMARY_GW = 0,
    UIM_SESSION_SECONDARY_GW = 2,
    UIM_SESSION_CARD_SLOT_1 = 6,
    UIM_SESSION_CARD_SLOT_2 = 7,
};
enum : uint8_t { PIN_ID_PIN1 = 1, PIN_ID_PIN2 = 2 };

uint8_t ProvSession(int slot)
{
    return slot == 1 ? UIM_SESSION_SECONDARY_GW : UIM_SESSION_PRIMARY_GW;
}

uint8_t CardSession(int slot)
{
    return slot == 1 ? UIM_SESSION_CARD_SLOT_2 : UIM_SESSION_CARD_SLOT_1;
}

void AddSession(TlvWriter &w, uint8_t session)
{
    uint8_t s[2] = {session, 0};
    w.Raw(0x01, s, 2);
}

/* "3F007FFF" -> 00 3F FF 7F */
std::vector<uint8_t> PathBytes(const std::string &hexPath)
{
    std::vector<uint8_t> out;
    std::vector<uint8_t> b = HexDecode(hexPath.c_str());
    for (size_t i = 0; i + 1 < b.size(); i += 2) {
        out.push_back(b[i + 1]);
        out.push_back(b[i]);
    }
    return out;
}

void AddFile(TlvWriter &w, uint16_t fileId, const std::string &hexPath)
{
    std::vector<uint8_t> p = PathBytes(hexPath);
    std::vector<uint8_t> v;
    v.push_back(static_cast<uint8_t>(fileId));
    v.push_back(static_cast<uint8_t>(fileId >> 8));
    v.push_back(static_cast<uint8_t>(p.size()));
    v.insert(v.end(), p.begin(), p.end());
    w.Raw(0x02, v.data(), v.size());
}

/* MF-level files (EF_ICCID, EF_PL, EF_DIR, ...) live outside the USIM ADF */
uint8_t SessionFor(int slot, const std::string &path)
{
    std::string p = path;
    for (auto &c : p) c = static_cast<char>(toupper(c));
    if (p == "3F00" || p.empty()) {
        return CardSession(slot);
    }
    return ProvSession(slot);
}

bool ReadTransparent(int slot, uint8_t session, uint16_t fileId, const std::string &path, uint16_t offset,
    uint16_t len, std::vector<uint8_t> &data, uint8_t &sw1, uint8_t &sw2)
{
    TlvWriter w;
    AddSession(w, session);
    AddFile(w, fileId, path);
    uint8_t ri[4] = {static_cast<uint8_t>(offset), static_cast<uint8_t>(offset >> 8), static_cast<uint8_t>(len),
        static_cast<uint8_t>(len >> 8)};
    w.Raw(0x03, ri, 4);
    QmiMsg m;
    (void)slot;
    if (!Ril::Get().uim.Request(UIM_READ_TRANSPARENT, w, m)) {
        return false;
    }
    sw1 = 0x90;
    sw2 = 0x00;
    if (m.Has(0x10)) {
        Reader r = m.R(0x10);
        sw1 = r.U8();
        sw2 = r.U8();
    }
    if (m.Has(0x11)) {
        Reader r = m.R(0x11);
        uint16_t n = r.U16();
        data = r.Bytes(n);
    }
    if (!m.Ok() && !m.Has(0x10)) {
        sw1 = 0x6F;
        sw2 = 0x00;
    }
    return true;
}

bool ReadRecord(uint8_t session, uint16_t fileId, const std::string &path, uint16_t rec, uint16_t len,
    std::vector<uint8_t> &data, uint8_t &sw1, uint8_t &sw2)
{
    TlvWriter w;
    AddSession(w, session);
    AddFile(w, fileId, path);
    uint8_t ri[4] = {static_cast<uint8_t>(rec), static_cast<uint8_t>(rec >> 8), static_cast<uint8_t>(len),
        static_cast<uint8_t>(len >> 8)};
    w.Raw(0x03, ri, 4);
    QmiMsg m;
    if (!Ril::Get().uim.Request(UIM_READ_RECORD, w, m)) {
        return false;
    }
    sw1 = 0x90;
    sw2 = 0x00;
    if (m.Has(0x10)) {
        Reader r = m.R(0x10);
        sw1 = r.U8();
        sw2 = r.U8();
    }
    if (m.Has(0x11)) {
        Reader r = m.R(0x11);
        uint16_t n = r.U16();
        data = r.Bytes(n);
    }
    if (!m.Ok() && !m.Has(0x10)) {
        sw1 = 0x6A;
        sw2 = 0x83;
    }
    return true;
}

/* GET RESPONSE in the 2G (TS 51.011) layout the telephony stack parses */
bool GetResponse(uint8_t session, uint16_t fileId, const std::string &path, std::vector<uint8_t> &rsp, uint8_t &sw1,
    uint8_t &sw2)
{
    TlvWriter w;
    AddSession(w, session);
    AddFile(w, fileId, path);
    QmiMsg m;
    if (!Ril::Get().uim.Request(UIM_GET_FILE_ATTRIBUTES, w, m)) {
        return false;
    }
    sw1 = 0x90;
    sw2 = 0x00;
    if (m.Has(0x10)) {
        Reader r = m.R(0x10);
        sw1 = r.U8();
        sw2 = r.U8();
    }
    if (!m.Ok() || !m.Has(0x11)) {
        if (!m.Has(0x10)) {
            sw1 = 0x6A;
            sw2 = 0x82;
        }
        rsp.clear();
        return true;
    }
    Reader r = m.R(0x11);
    uint16_t size = r.U16();
    uint16_t fid = r.U16();
    uint8_t type = r.U8();
    uint16_t recSize = r.U16();
    uint16_t recCount = r.U16();
    if (size == 0 && recSize && recCount) {
        size = static_cast<uint16_t>(recSize * recCount);
    }
    uint8_t structure = 0x00;
    uint8_t fileType = 0x04; /* EF */
    switch (type) {
        case 1: structure = 0x03; break; /* cyclic */
        case 2: structure = 0x01; break; /* linear fixed */
        case 3: fileType = 0x02; break;  /* DF */
        case 4: fileType = 0x01; break;  /* MF */
        default: break;
    }
    rsp.assign(15, 0);
    rsp[2] = static_cast<uint8_t>(size >> 8);
    rsp[3] = static_cast<uint8_t>(size);
    rsp[4] = static_cast<uint8_t>(fid >> 8);
    rsp[5] = static_cast<uint8_t>(fid);
    rsp[6] = fileType;
    rsp[11] = 0x01;
    rsp[12] = 0x02;
    rsp[13] = structure;
    rsp[14] = static_cast<uint8_t>(recSize);
    return true;
}

int32_t MapSimState(const CardInfo &c)
{
    if (!c.valid) {
        return HRIL_SIM_NOT_READY;
    }
    if (c.cardState != 1) {
        return HRIL_SIM_NOT_INSERTED;
    }
    switch (c.appState) {
        case 7: return HRIL_SIM_READY;
        case 2: return HRIL_SIM_PIN;
        case 3: return HRIL_SIM_PUK;
        case 5: return HRIL_SIM_PUK;
        case 4:
            return c.persoState == 2 ? HRIL_SIM_READY : HRIL_PH_NET_PIN;
        default: return HRIL_SIM_NOT_READY;
    }
}

int32_t MapSimType(const CardInfo &c)
{
    if (c.appType == 2) return HRIL_SIM_TYPE_USIM;
    if (c.appType == 1) return HRIL_SIM_TYPE_SIM;
    return HRIL_SIM_TYPE_UNKNOWN;
}

/* parses the Card Status TLV (response 0x10 / indication 0x10) */
void ParseCardStatus(const QmiMsg &m, CardInfo out[2])
{
    for (int i = 0; i < 2; i++) {
        CardInfo fresh;
        fresh.valid = true;
        fresh.iccid = out[i].iccid;
        out[i] = fresh;
    }
    if (!m.Has(0x10)) {
        return;
    }
    Reader r = m.R(0x10);
    uint16_t idxGw[2];
    idxGw[0] = r.U16();
    r.U16();
    idxGw[1] = r.U16();
    r.U16();
    uint8_t ncards = r.U8();
    for (uint8_t c = 0; c < ncards && r.Ok(); c++) {
        CardInfo ci;
        ci.valid = true;
        ci.cardState = r.U8();
        uint8_t upinState = r.U8();
        uint8_t upinRetries = r.U8();
        uint8_t upukRetries = r.U8();
        r.U8(); /* error code */
        uint8_t napps = r.U8();
        int chosen = -1;
        struct App {
            uint8_t type, state, perso, pin1, pin1r, puk1r, pin2, pin2r, puk2r;
            bool upin;
            std::vector<uint8_t> aid;
        };
        std::vector<App> apps;
        for (uint8_t a = 0; a < napps && r.Ok(); a++) {
            App ap;
            ap.type = r.U8();
            ap.state = r.U8();
            ap.perso = r.U8();
            r.U8(); /* perso feature */
            r.U8(); /* perso retries */
            r.U8(); /* perso unblock retries */
            uint8_t aidLen = r.U8();
            ap.aid = r.Bytes(aidLen);
            ap.upin = r.U8() != 0;
            ap.pin1 = r.U8();
            ap.pin1r = r.U8();
            ap.puk1r = r.U8();
            ap.pin2 = r.U8();
            ap.pin2r = r.U8();
            ap.puk2r = r.U8();
            apps.push_back(ap);
        }
        if (c < 2) {
            uint16_t idx = idxGw[c];
            if (idx != 0xFFFF && (idx >> 8) == c && (idx & 0xFF) < apps.size()) {
                chosen = idx & 0xFF;
            }
            for (size_t a = 0; chosen < 0 && a < apps.size(); a++) {
                if (apps[a].type == 2) chosen = static_cast<int>(a);
            }
            for (size_t a = 0; chosen < 0 && a < apps.size(); a++) {
                if (apps[a].type == 1) chosen = static_cast<int>(a);
            }
            if (chosen >= 0) {
                const App &ap = apps[chosen];
                ci.appType = ap.type;
                ci.appState = ap.state;
                ci.persoState = ap.perso;
                ci.upinReplacesPin1 = ap.upin;
                ci.pin1State = ap.upin ? upinState : ap.pin1;
                ci.pin1Retries = ap.upin ? upinRetries : ap.pin1r;
                ci.puk1Retries = ap.upin ? upukRetries : ap.puk1r;
                ci.pin2State = ap.pin2;
                ci.pin2Retries = ap.pin2r;
                ci.puk2Retries = ap.puk2r;
                ci.aid = ap.aid;
            }
            ci.iccid = out[c].iccid;
            if (ci.cardState != 1) {
                ci.iccid.clear();
            }
            out[c] = ci;
        }
    }
}

std::string DecodeIccid(const std::vector<uint8_t> &b)
{
    static const char *d = "0123456789ABCDEF";
    std::string s;
    for (uint8_t x : b) {
        uint8_t lo = x & 0x0F, hi = x >> 4;
        if (lo == 0xF) break;
        s.push_back(d[lo]);
        if (hi == 0xF) break;
        s.push_back(d[hi]);
    }
    return s;
}

void ReadIccid(int slot)
{
    std::vector<uint8_t> data;
    uint8_t sw1 = 0, sw2 = 0;
    if (ReadTransparent(slot, CardSession(slot), 0x2FE2, "3F00", 0, 0, data, sw1, sw2) && sw1 == 0x90 &&
        !data.empty()) {
        std::string iccid = DecodeIccid(data);
        Ril &r = Ril::Get();
        std::lock_guard<std::mutex> lk(r.stateMu);
        r.cards[slot].iccid = iccid;
    }
}

void OnCardStatus(const QmiMsg &m, bool notify)
{
    Ril &r = Ril::Get();
    int32_t oldState[2], newState[2];
    bool needIccid[2] = {false, false};
    {
        std::lock_guard<std::mutex> lk(r.stateMu);
        oldState[0] = r.cards[0].valid ? MapSimState(r.cards[0]) : -100;
        oldState[1] = r.cards[1].valid ? MapSimState(r.cards[1]) : -100;
        ParseCardStatus(m, r.cards);
        for (int i = 0; i < 2; i++) {
            newState[i] = MapSimState(r.cards[i]);
            needIccid[i] = r.cards[i].cardState == 1 && r.cards[i].iccid.empty();
        }
    }
    for (int i = 0; i < 2; i++) {
        if (needIccid[i]) {
            ReadIccid(i);
        }
        if (oldState[i] != newState[i]) {
            Klog("SIM%d state %d -> %d", i + 1, oldState[i], newState[i]);
            if (notify) {
                r.Notify(D_SIM, i, HNOTI_SIM_STATUS_CHANGED, nullptr, 0);
            }
        }
    }
}

/* ------------------------------------------------------------ requests */
void ReqGetSimStatus(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        Ril &r = Ril::Get();
        int slot = SlotOf(req) == 1 ? 1 : 0;
        bool valid;
        {
            std::lock_guard<std::mutex> lk(r.stateMu);
            valid = r.cards[slot].valid;
        }
        if (!valid) {
            SimRefreshCardStatus(false);
        }
        HRilCardState st = {};
        std::string iccid;
        {
            std::lock_guard<std::mutex> lk(r.stateMu);
            const CardInfo &c = r.cards[slot];
            st.index = slot;
            st.simType = static_cast<HRilSimType>(MapSimType(c));
            st.simState = static_cast<HRilSimState>(MapSimState(c));
            iccid = c.iccid;
        }
        st.iccid = const_cast<char *>(iccid.c_str());
        r.Respond(D_SIM, req, HRIL_ERR_SUCCESS, &st, sizeof(st));
    });
}

void ReqGetSimIO(const ReqDataInfo *req, const HRilSimIO *data, size_t)
{
    if (data == nullptr) {
        Ril::Get().Respond(D_SIM, req, HRIL_ERR_INVALID_PARAMETER, nullptr, 0);
        return;
    }
    HRilSimIO io = *data;
    std::string path = data->pathid ? data->pathid : "";
    std::string payload = data->data ? data->data : "";
    Ril::Get().Post([req, io, path, payload] {
        int slot = SlotOf(req) == 1 ? 1 : 0;
        uint8_t session = SessionFor(slot, path);
        uint16_t fid = static_cast<uint16_t>(io.fileid);
        std::vector<uint8_t> out;
        uint8_t sw1 = 0x6F, sw2 = 0x00;
        bool ok = false;
        switch (io.command) {
            case 176: /* READ BINARY */
                ok = ReadTransparent(slot, session, fid, path, static_cast<uint16_t>(io.p1 << 8 | io.p2),
                    static_cast<uint16_t>(io.p3), out, sw1, sw2);
                break;
            case 178: /* READ RECORD */
                ok = ReadRecord(session, fid, path, static_cast<uint16_t>(io.p1), static_cast<uint16_t>(io.p3), out,
                    sw1, sw2);
                break;
            case 192: /* GET RESPONSE */
                ok = GetResponse(session, fid, path, out, sw1, sw2);
                break;
            case 214:   /* UPDATE BINARY */
            case 220: { /* UPDATE RECORD */
                std::vector<uint8_t> d = HexDecode(payload.c_str());
                TlvWriter w;
                AddSession(w, session);
                AddFile(w, fid, path);
                std::vector<uint8_t> wi;
                uint16_t first = io.command == 214 ? static_cast<uint16_t>(io.p1 << 8 | io.p2) :
                    static_cast<uint16_t>(io.p1);
                wi.push_back(static_cast<uint8_t>(first));
                wi.push_back(static_cast<uint8_t>(first >> 8));
                wi.push_back(static_cast<uint8_t>(d.size()));
                wi.push_back(static_cast<uint8_t>(d.size() >> 8));
                wi.insert(wi.end(), d.begin(), d.end());
                w.Raw(0x03, wi.data(), wi.size());
                QmiMsg m;
                ok = Ril::Get().uim.Request(io.command == 214 ? UIM_WRITE_TRANSPARENT : UIM_WRITE_RECORD, w, m);
                sw1 = 0x90;
                sw2 = 0x00;
                if (m.Has(0x10)) {
                    Reader r = m.R(0x10);
                    sw1 = r.U8();
                    sw2 = r.U8();
                } else if (!m.Ok()) {
                    sw1 = 0x6F;
                }
                break;
            }
            case 242: /* STATUS */
                ok = true;
                sw1 = 0x90;
                sw2 = 0x00;
                break;
            default:
                break;
        }
        if (!ok) {
            Ril::Get().Respond(D_SIM, req, HRIL_ERR_GENERIC_FAILURE, nullptr, 0);
            return;
        }
        std::string hex = HexEncode(out);
        HRilSimIOResponse rsp = {};
        rsp.sw1 = sw1;
        rsp.sw2 = sw2;
        rsp.response = const_cast<char *>(hex.c_str());
        Ril::Get().Respond(D_SIM, req, HRIL_ERR_SUCCESS, &rsp, sizeof(rsp));
    });
}

void ReqGetSimImsi(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        int slot = SlotOf(req) == 1 ? 1 : 0;
        uint8_t type;
        {
            std::lock_guard<std::mutex> lk(Ril::Get().stateMu);
            type = Ril::Get().cards[slot].appType;
        }
        std::vector<uint8_t> d;
        uint8_t sw1 = 0, sw2 = 0;
        std::string path = type == 1 ? "3F007F20" : "3F007FFF";
        bool ok = ReadTransparent(slot, ProvSession(slot), 0x6F07, path, 0, 0, d, sw1, sw2);
        if (!ok || sw1 != 0x90 || d.size() < 2) {
            Ril::Get().Respond(D_SIM, req, HRIL_ERR_GENERIC_FAILURE, nullptr, 0);
            return;
        }
        std::string imsi;
        size_t len = std::min<size_t>(d[0], d.size() - 1);
        for (size_t i = 1; i <= len; i++) {
            uint8_t lo = d[i] & 0x0F, hi = d[i] >> 4;
            if (i != 1 && lo <= 9) imsi.push_back(static_cast<char>('0' + lo));
            if (hi <= 9) imsi.push_back(static_cast<char>('0' + hi));
        }
        Ril::Get().Respond(D_SIM, req, HRIL_ERR_SUCCESS, imsi.c_str(), imsi.size() + 1);
    });
}

void ReqGetSimLockStatus(const ReqDataInfo *req, const HRilSimClock *data, size_t)
{
    std::string fac = (data && data->fac) ? data->fac : "";
    Ril::Get().Post([req, fac] {
        int slot = SlotOf(req) == 1 ? 1 : 0;
        int32_t status = 0;
        {
            std::lock_guard<std::mutex> lk(Ril::Get().stateMu);
            const CardInfo &c = Ril::Get().cards[slot];
            if (fac == "SC") {
                status = (c.pin1State == 1 || c.pin1State == 2 || c.pin1State == 4) ? 1 : 0;
            } else if (fac == "FD") {
                status = 0;
            }
        }
        Ril::Get().Respond(D_SIM, req, HRIL_ERR_SUCCESS, &status, sizeof(status));
    });
}

/* common tail of the PIN operations: result + retries left */
void RespondPin(const ReqDataInfo *req, bool sent, const QmiMsg &m, bool usePuk)
{
    HRilLockStatus ls = {};
    int32_t err = HRIL_ERR_SUCCESS;
    if (!sent) {
        err = HRIL_ERR_CMD_SEND_FAILURE;
        ls.result = HRIL_UNLOCK_OTHER_ERR;
    } else if (m.result == 0) {
        ls.result = HRIL_UNLOCK_SUCCESS;
    } else {
        /* QMI_ERR_INCORRECT_PIN 0x0C, PIN_BLOCKED 0x23 */
        ls.result = (m.error == 0x0C || m.error == 0x23) ? HRIL_UNLOCK_PASSWORD_ERR : HRIL_UNLOCK_OTHER_ERR;
    }
    ls.remain = -1;
    if (m.Has(0x10)) {
        Reader r = m.R(0x10);
        uint8_t verify = r.U8();
        uint8_t unblock = r.U8();
        ls.remain = usePuk ? unblock : verify;
    }
    Ril::Get().Respond(D_SIM, req, err, &ls, sizeof(ls));
    SimRefreshCardStatus(true);
}

void PinOp(const ReqDataInfo *req, uint16_t msg, uint8_t pinId, const std::string &a, const std::string &b,
    bool twoStrings, bool usePuk, int enabled = -1)
{
    Ril::Get().Post([=] {
        int slot = SlotOf(req) == 1 ? 1 : 0;
        TlvWriter w;
        AddSession(w, ProvSession(slot));
        std::vector<uint8_t> info;
        info.push_back(pinId);
        if (enabled >= 0) {
            info.push_back(static_cast<uint8_t>(enabled));
        }
        info.push_back(static_cast<uint8_t>(a.size()));
        info.insert(info.end(), a.begin(), a.end());
        if (twoStrings) {
            info.push_back(static_cast<uint8_t>(b.size()));
            info.insert(info.end(), b.begin(), b.end());
        }
        w.Raw(0x02, info.data(), info.size());
        QmiMsg m;
        bool sent = Ril::Get().uim.Request(msg, w, m, 30000);
        RespondPin(req, sent, m, usePuk);
    });
}

void ReqSetSimLock(const ReqDataInfo *req, const HRilSimClock *data, size_t)
{
    if (data == nullptr) {
        Ril::Get().Respond(D_SIM, req, HRIL_ERR_INVALID_PARAMETER, nullptr, 0);
        return;
    }
    std::string fac = data->fac ? data->fac : "";
    std::string pw = data->passwd ? data->passwd : "";
    uint8_t id = fac == "FD" ? PIN_ID_PIN2 : PIN_ID_PIN1;
    PinOp(req, UIM_SET_PIN_PROTECTION, id, pw, "", false, false, data->mode ? 1 : 0);
}

void ReqChangeSimPassword(const ReqDataInfo *req, const HRilSimPassword *data, size_t)
{
    if (data == nullptr) {
        Ril::Get().Respond(D_SIM, req, HRIL_ERR_INVALID_PARAMETER, nullptr, 0);
        return;
    }
    std::string fac = data->fac ? data->fac : "";
    uint8_t id = (fac == "P2") ? PIN_ID_PIN2 : PIN_ID_PIN1;
    PinOp(req, UIM_CHANGE_PIN, id, data->oldPassword ? data->oldPassword : "",
        data->newPassword ? data->newPassword : "", true, false);
}

void ReqUnlockPin(const ReqDataInfo *req, const char *pin)
{
    PinOp(req, UIM_VERIFY_PIN, PIN_ID_PIN1, pin ? pin : "", "", false, false);
}

void ReqUnlockPuk(const ReqDataInfo *req, const char *puk, const char *pin)
{
    PinOp(req, UIM_UNBLOCK_PIN, PIN_ID_PIN1, puk ? puk : "", pin ? pin : "", true, true);
}

void ReqUnlockPin2(const ReqDataInfo *req, const char *pin2)
{
    PinOp(req, UIM_VERIFY_PIN, PIN_ID_PIN2, pin2 ? pin2 : "", "", false, false);
}

void ReqUnlockPuk2(const ReqDataInfo *req, const char *puk2, const char *pin2)
{
    PinOp(req, UIM_UNBLOCK_PIN, PIN_ID_PIN2, puk2 ? puk2 : "", pin2 ? pin2 : "", true, true);
}

void ReqSimpleOk(const ReqDataInfo *req)
{
    Ril::Get().Respond(D_SIM, req, HRIL_ERR_SUCCESS, nullptr, 0);
}

void ReqSetActiveSim(const ReqDataInfo *req, int32_t, int32_t)
{
    ReqSimpleOk(req);
}

void ReqStkString(const ReqDataInfo *req, const char *)
{
    ReqSimpleOk(req);
}

void ReqStkCallSetup(const ReqDataInfo *req, int32_t)
{
    ReqSimpleOk(req);
}

void ReqGetRadioProtocol(const ReqDataInfo *req)
{
    HRilRadioProtocol p = {};
    p.sessionId = 0;
    p.phase = HRIL_RADIO_PROTOCOL_PHASE_INITIAL;
    p.technology = HRIL_RADIO_PROTOCOL_TECH_GSM | HRIL_RADIO_PROTOCOL_TECH_WCDMA | HRIL_RADIO_PROTOCOL_TECH_HSPA |
        HRIL_RADIO_PROTOCOL_TECH_HSPAP | HRIL_RADIO_PROTOCOL_TECH_TD_SCDMA | HRIL_RADIO_PROTOCOL_TECH_LTE |
        HRIL_RADIO_PROTOCOL_TECH_LTE_CA;
    p.modemId = SlotOf(req);
    p.status = HRIL_RADIO_PROTOCOL_STATUS_NONE;
    Ril::Get().Respond(D_SIM, req, HRIL_ERR_SUCCESS, &p, sizeof(p));
}

void ReqSetRadioProtocol(const ReqDataInfo *req, const HRilRadioProtocol *data)
{
    HRilRadioProtocol p = {};
    if (data) {
        p = *data;
    }
    p.status = HRIL_RADIO_PROTOCOL_STATUS_SUCCESS;
    Ril::Get().Respond(D_SIM, req, HRIL_ERR_SUCCESS, &p, sizeof(p));
}

void ReqOpenLogicalChannel(const ReqDataInfo *req, const char *appID, int32_t p2)
{
    std::string aid = appID ? appID : "";
    Ril::Get().Post([req, aid, p2] {
        int slot = SlotOf(req) == 1 ? 1 : 0;
        TlvWriter w;
        w.U8(0x01, static_cast<uint8_t>(slot + 1));
        std::vector<uint8_t> a = HexDecode(aid.c_str());
        std::vector<uint8_t> v;
        v.push_back(static_cast<uint8_t>(a.size()));
        v.insert(v.end(), a.begin(), a.end());
        w.Raw(0x10, v.data(), v.size());
        (void)p2;
        QmiMsg m;
        bool sent = Ril::Get().uim.Request(0x0042, w, m);
        HRilOpenLogicalChannelResponse rsp = {};
        uint8_t ch = 0;
        std::string sel;
        if (sent && m.Ok() && m.GetU8(0x10, ch)) {
            rsp.channelId = ch;
            rsp.sw1 = 0x90;
            rsp.sw2 = 0x00;
            if (m.Has(0x11)) {
                Reader r = m.R(0x11);
                rsp.sw1 = r.U8();
                rsp.sw2 = r.U8();
            }
            if (m.Has(0x12)) {
                Reader r = m.R(0x12);
                sel = HexEncode(r.Bytes(r.U8()));
            }
            rsp.response = const_cast<char *>(sel.c_str());
            Ril::Get().Respond(D_SIM, req, HRIL_ERR_SUCCESS, &rsp, sizeof(rsp));
        } else {
            Ril::Get().Respond(D_SIM, req, HRIL_ERR_GENERIC_FAILURE, nullptr, 0);
        }
    });
}

void ReqCloseLogicalChannel(const ReqDataInfo *req, int32_t channelId)
{
    Ril::Get().Post([req, channelId] {
        int slot = SlotOf(req) == 1 ? 1 : 0;
        TlvWriter w;
        w.U8(0x01, static_cast<uint8_t>(slot + 1));
        w.U8(0x11, static_cast<uint8_t>(channelId));
        QmiMsg m;
        bool sent = Ril::Get().uim.Request(0x003F, w, m);
        Ril::Get().Respond(D_SIM, req, QmiErr(sent, m), nullptr, 0);
    });
}

void TransmitApdu(const ReqDataInfo *req, const HRilApduSimIO *data, bool basic)
{
    if (data == nullptr) {
        Ril::Get().Respond(D_SIM, req, HRIL_ERR_INVALID_PARAMETER, nullptr, 0);
        return;
    }
    HRilApduSimIO io = *data;
    std::string payload = data->data ? data->data : "";
    Ril::Get().Post([req, io, payload, basic] {
        int slot = SlotOf(req) == 1 ? 1 : 0;
        std::vector<uint8_t> apdu;
        apdu.push_back(static_cast<uint8_t>(io.type));
        apdu.push_back(static_cast<uint8_t>(io.instruction));
        apdu.push_back(static_cast<uint8_t>(io.p1));
        apdu.push_back(static_cast<uint8_t>(io.p2));
        std::vector<uint8_t> d = HexDecode(payload.c_str());
        if (io.p3 >= 0) {
            apdu.push_back(static_cast<uint8_t>(io.p3));
        }
        apdu.insert(apdu.end(), d.begin(), d.end());
        TlvWriter w;
        w.U8(0x01, static_cast<uint8_t>(slot + 1));
        std::vector<uint8_t> v;
        v.push_back(static_cast<uint8_t>(apdu.size()));
        v.push_back(static_cast<uint8_t>(apdu.size() >> 8));
        v.insert(v.end(), apdu.begin(), apdu.end());
        w.Raw(0x02, v.data(), v.size());
        if (!basic) {
            w.U8(0x10, static_cast<uint8_t>(io.channelId));
        }
        QmiMsg m;
        bool sent = Ril::Get().uim.Request(0x003B, w, m);
        if (!sent || !m.Ok() || !m.Has(0x10)) {
            Ril::Get().Respond(D_SIM, req, HRIL_ERR_GENERIC_FAILURE, nullptr, 0);
            return;
        }
        Reader r = m.R(0x10);
        std::vector<uint8_t> resp = r.Bytes(r.U16());
        HRilSimIOResponse rsp = {};
        std::string hex;
        if (resp.size() >= 2) {
            rsp.sw1 = resp[resp.size() - 2];
            rsp.sw2 = resp[resp.size() - 1];
            hex = HexEncode(resp.data(), resp.size() - 2);
        }
        rsp.response = const_cast<char *>(hex.c_str());
        Ril::Get().Respond(D_SIM, req, HRIL_ERR_SUCCESS, &rsp, sizeof(rsp));
    });
}

void ReqTransmitApduLogical(const ReqDataInfo *req, HRilApduSimIO *data, size_t)
{
    TransmitApdu(req, data, false);
}

void ReqTransmitApduBasic(const ReqDataInfo *req, HRilApduSimIO *data, size_t)
{
    TransmitApdu(req, data, true);
}

void ReqUnlockSimLock(const ReqDataInfo *req, int32_t, const char *)
{
    HRilLockStatus ls = {HRIL_UNLOCK_OTHER_ERR, 0};
    Ril::Get().Respond(D_SIM, req, HRIL_ERR_GENERIC_FAILURE, &ls, sizeof(ls));
}

void ReqSendSimMatchedOperatorInfo(const ReqDataInfo *req, HRilNcfgOperatorInfo *, size_t)
{
    ReqSimpleOk(req);
}

void ReqSetSimPowerState(const ReqDataInfo *req, int32_t state)
{
    Ril::Get().Post([req, state] {
        int slot = SlotOf(req) == 1 ? 1 : 0;
        QmiMsg m;
        bool sent = Ril::Get().uim.Request(state == 0 ? 0x0030 : 0x0031,
            TlvWriter().U8(0x01, static_cast<uint8_t>(slot + 1)), m);
        Ril::Get().Respond(D_SIM, req, QmiErr(sent, m), nullptr, 0);
    });
}
} // namespace

void SimRefreshCardStatus(bool notify)
{
    QmiMsg m;
    if (Ril::Get().uim.Request(UIM_GET_CARD_STATUS, m) && m.Ok()) {
        OnCardStatus(m, notify);
    }
}

void SimOnUp()
{
    Ril &r = Ril::Get();
    static bool handlerSet = false;
    if (!handlerSet) {
        handlerSet = true;
        r.uim.OnIndication([](const QmiMsg &m) {
            if (m.msgId == UIM_CARD_STATUS_IND) {
                /* runs on the UIM reader thread: do the follow-up reads elsewhere */
                Ril::Get().Post([m] { OnCardStatus(m, true); });
            }
        });
    }
    QmiMsg m;
    r.uim.Request(UIM_REGISTER_EVENTS, TlvWriter().U32(0x01, 0x1), m);
    SimRefreshCardStatus(true);
}

const HRilSimReq g_simOps = {
    .GetSimStatus = ReqGetSimStatus,
    .GetSimIO = ReqGetSimIO,
    .GetSimImsi = ReqGetSimImsi,
    .GetSimLockStatus = ReqGetSimLockStatus,
    .SetSimLock = ReqSetSimLock,
    .ChangeSimPassword = ReqChangeSimPassword,
    .UnlockPin = ReqUnlockPin,
    .UnlockPuk = ReqUnlockPuk,
    .GetSimPinInputTimes = nullptr,
    .UnlockPin2 = ReqUnlockPin2,
    .UnlockPuk2 = ReqUnlockPuk2,
    .GetSimPin2InputTimes = nullptr,
    .SetActiveSim = ReqSetActiveSim,
    .SimStkSendTerminalResponse = ReqStkString,
    .SimStkSendEnvelope = ReqStkString,
    .SimStkSendEnvelopeResponseContainStatus = ReqStkString,
    .SimStkSendCallSetupRequestResult = ReqStkCallSetup,
    .SimStkIsReady = ReqSimpleOk,
    .GetRadioProtocol = ReqGetRadioProtocol,
    .SetRadioProtocol = ReqSetRadioProtocol,
    .SimOpenLogicalChannel = ReqOpenLogicalChannel,
    .SimCloseLogicalChannel = ReqCloseLogicalChannel,
    .SimTransmitApduLogicalChannel = ReqTransmitApduLogical,
    .SimTransmitApduBasicChannel = ReqTransmitApduBasic,
    .SimAuthentication = nullptr,
    .UnlockSimLock = ReqUnlockSimLock,
    .SendSimMatchedOperatorInfo = ReqSendSimMatchedOperatorInfo,
    .SetSimPowerState = ReqSetSimPowerState,
};

} // namespace sweetril
