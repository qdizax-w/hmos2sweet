/*
 * sweet RIL: network registration, signal, operator, selection (QMI NAS).
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "ril.h"

#include <cstdio>

namespace sweetril {
namespace {

enum : int8_t {
    RIF_CDMA_1X = 0x01,
    RIF_EVDO = 0x02,
    RIF_GSM = 0x04,
    RIF_UMTS = 0x05,
    RIF_LTE = 0x08,
    RIF_TDSCDMA = 0x09,
    RIF_NR = 0x0C,
};
enum : uint16_t {
    RAT_GSM = 1 << 2,
    RAT_UMTS = 1 << 3,
    RAT_LTE = 1 << 4,
    RAT_TDS = 1 << 5,
    RAT_NR = 1 << 6,
};

bool HasIf(const ServingInfo &s, int8_t rif)
{
    for (auto r : s.radioIfs) {
        if (r == rif) return true;
    }
    return false;
}

bool HasCap(const ServingInfo &s, uint8_t cap)
{
    for (auto c : s.dataCaps) {
        if (c == cap) return true;
    }
    return false;
}

bool ParseServing(const QmiMsg &m, ServingInfo &s)
{
    if (!m.Has(0x01)) {
        return false;
    }
    ServingInfo n;
    n.valid = true;
    Reader r = m.R(0x01);
    n.regState = r.U8();
    n.csAttach = r.U8();
    n.psAttach = r.U8();
    r.U8(); /* selected network */
    uint8_t cnt = r.U8();
    for (uint8_t i = 0; i < cnt && r.Ok(); i++) {
        n.radioIfs.push_back(r.S8());
    }
    uint8_t roam = 1;
    if (m.GetU8(0x10, roam)) {
        n.roaming = (roam == 0);
    }
    if (m.Has(0x11)) {
        Reader d = m.R(0x11);
        uint8_t k = d.U8();
        for (uint8_t i = 0; i < k && d.Ok(); i++) {
            n.dataCaps.push_back(d.U8());
        }
    }
    if (m.Has(0x12)) {
        Reader p = m.R(0x12);
        n.mcc = p.U16();
        n.mnc = p.U16();
        n.description = p.Str8();
    }
    m.GetU16(0x1C, n.lac);
    m.GetU32(0x1D, n.cid);
    if (m.GetU16(0x24, n.tac)) {
        n.hasTac = true;
    }
    if (m.Has(0x27)) {
        Reader p = m.R(0x27);
        uint16_t mcc = p.U16();
        uint16_t mnc = p.U16();
        uint8_t pcs = p.U8();
        if (p.Ok() && mcc == n.mcc && mnc == n.mnc) {
            n.mnc3 = pcs != 0;
        }
    }
    s = n;
    return true;
}

int32_t MapRegStatus(uint8_t regState, bool roaming)
{
    switch (regState) {
        case 1: return roaming ? REG_MT_ROAMING : REG_MT_HOME;
        case 2: return NO_REG_MT_SEARCHING;
        case 3: return REG_MT_REJECTED;
        case 4: return REG_MT_UNKNOWN;
        default: return NO_REG_MT_NO_SEARCH;
    }
}

void BuildRegInfo(const ServingInfo &s, bool cs, HRilRegMsgType msgType, HRilRegStatusInfo &ri)
{
    ri = {};
    ri.regType = cs ? CS_REG_TYPE : PS_REG_TYPE;
    ri.regMsgType = msgType;
    ri.notifyMode = REG_NOTIFY_STAT_LAC_CELLID;
    uint8_t attach = cs ? s.csAttach : s.psAttach;
    uint8_t reg = s.regState;
    /* registered, but this domain explicitly detached (e.g. LTE without CSFB) */
    if (reg == 1 && attach == 2) {
        reg = 0;
    }
    ri.regStatus = static_cast<HRilRegStatus>(MapRegStatus(reg, s.roaming));
    bool lte = HasIf(s, RIF_LTE) || HasIf(s, RIF_NR);
    ri.lacCode = (lte && s.hasTac) ? s.tac : s.lac;
    ri.cellId = static_cast<int32_t>(s.cid);
    ri.actType = static_cast<HRilRadioTech>(reg == 1 ? MapRadioTech(s) : RADIO_TECHNOLOGY_UNKNOWN);
    ri.isNrAvailable = 0;
    ri.isEnDcAvailable = 0;
    ri.isDcNrRestricted = 0;
}

void ParseSignal(const QmiMsg &m, SignalInfo &g)
{
    SignalInfo n;
    if (m.Has(0x12)) {
        Reader r = m.R(0x12);
        n.gsmRssi = r.S8();
        n.hasGsm = r.Ok();
    }
    if (m.Has(0x13)) {
        Reader r = m.R(0x13);
        n.wcdmaRssi = r.S8();
        n.wcdmaEcio = r.S16();
        n.hasWcdma = r.Ok();
    }
    if (m.Has(0x19)) {
        Reader r = m.R(0x19);
        n.wcdmaRscp = r.S16();
        n.hasWcdmaRscp = r.Ok();
    }
    if (m.Has(0x14)) {
        Reader r = m.R(0x14);
        n.lteRssi = r.S8();
        n.lteRsrq = r.S8();
        n.lteRsrp = r.S16();
        n.lteSnr = r.S16();
        n.hasLte = r.Ok();
    }
    if (m.Has(0x15)) {
        Reader r = m.R(0x15);
        n.tdRscp = r.S8();
        n.hasTd = r.Ok();
    }
    if (m.Has(0x17)) {
        Reader r = m.R(0x17);
        n.nrRsrp = r.S16();
        n.nrSnr = r.S16();
        n.hasNr = r.Ok();
        if (m.Has(0x18)) {
            Reader q = m.R(0x18);
            n.nrRsrq = q.S16();
        }
    }
    g = n;
}

void BuildRssi(const SignalInfo &g, HRilRssi &r)
{
    r = {};
    if (g.hasGsm) {
        r.gsmRssi.rxlev = g.gsmRssi;
        r.gsmRssi.ber = 0;
    }
    if (g.hasWcdma) {
        r.wcdmaRssi.rxlev = g.wcdmaRssi;
        r.wcdmaRssi.ecio = -(g.wcdmaEcio / 2);
        r.wcdmaRssi.rscp = g.hasWcdmaRscp ? g.wcdmaRscp : g.wcdmaRssi;
        r.wcdmaRssi.ber = 0;
    }
    if (g.hasLte) {
        r.lteRssi.rxlev = g.lteRssi;
        r.lteRssi.rsrq = g.lteRsrq;
        r.lteRssi.rsrp = g.lteRsrp;
        r.lteRssi.snr = g.lteSnr / 10;
    }
    if (g.hasTd) {
        r.tdScdmaRssi.rscp = g.tdRscp;
    }
    if (g.hasNr) {
        r.nrRssi.rsrp = g.nrRsrp;
        r.nrRssi.rsrq = g.nrRsrq;
        r.nrRssi.sinr = g.nrSnr / 10;
    }
}

void OnServing(const QmiMsg &m, bool notify)
{
    Ril &r = Ril::Get();
    ServingInfo s;
    if (!ParseServing(m, s)) {
        return;
    }
    ServingInfo old;
    {
        std::lock_guard<std::mutex> lk(r.stateMu);
        old = r.serving;
        r.serving = s;
    }
    bool changed = !old.valid || old.regState != s.regState || old.csAttach != s.csAttach ||
        old.psAttach != s.psAttach || old.radioIfs != s.radioIfs || old.dataCaps != s.dataCaps ||
        old.lac != s.lac || old.cid != s.cid || old.tac != s.tac || old.roaming != s.roaming ||
        old.mcc != s.mcc || old.mnc != s.mnc;
    if (!changed || !notify) {
        return;
    }
    Klog("serving: reg %u cs %u ps %u rat %d plmn %03u-%02u '%s'", s.regState, s.csAttach, s.psAttach,
        MapRadioTech(s), s.mcc, s.mnc, s.description.c_str());
    HRilRegStatusInfo cs, ps;
    BuildRegInfo(s, true, REG_NOTIFY_TYPE, cs);
    BuildRegInfo(s, false, REG_NOTIFY_TYPE, ps);
    r.Notify(D_NETWORK, 0, HNOTI_NETWORK_CS_REG_STATUS_UPDATED, &cs, sizeof(cs));
    r.Notify(D_NETWORK, 0, HNOTI_NETWORK_PS_REG_STATUS_UPDATED, &ps, sizeof(ps));
    if (MapRadioTech(old) != MapRadioTech(s)) {
        HRilVoiceRadioInfo vi = {};
        FillVoiceRadioInfo(vi);
        r.Notify(D_MODEM, 0, HNOTI_MODEM_VOICE_TECH_UPDATED, &vi, sizeof(vi));
    }
}

void OnSignal(const QmiMsg &m)
{
    Ril &r = Ril::Get();
    SignalInfo g;
    ParseSignal(m, g);
    {
        std::lock_guard<std::mutex> lk(r.stateMu);
        r.signal = g;
    }
    HRilRssi rssi;
    BuildRssi(g, rssi);
    r.Notify(D_NETWORK, 0, HNOTI_NETWORK_SIGNAL_STRENGTH_UPDATED, &rssi, sizeof(rssi));
}

void OnNetworkTime(const QmiMsg &m)
{
    if (!m.Has(0x01)) return;
    Reader r = m.R(0x01);
    uint16_t year = r.U16();
    uint8_t mon = r.U8(), day = r.U8(), hour = r.U8(), min = r.U8(), sec = r.U8();
    if (!r.Ok()) return;
    uint8_t tzU = 0;
    int tz = 0;
    if (m.GetU8(0x10, tzU)) tz = static_cast<int8_t>(tzU);
    uint8_t dst = 0;
    m.GetU8(0x11, dst);
    char buf[64];
    snprintf(buf, sizeof(buf), "%02u/%02u/%02u,%02u:%02u:%02u%c%02d,%u", year % 100, mon, day, hour, min, sec,
        tz < 0 ? '-' : '+', tz < 0 ? -tz : tz, dst);
    Klog("network time %s", buf);
    Ril::Get().Notify(D_NETWORK, 0, HNOTI_NETWORK_TIME_UPDATED, buf, strlen(buf) + 1);
}

/* ------------------------------------------------------------ requests */
void ReqGetSignalStrength(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        Ril &r = Ril::Get();
        HRilRssi rssi = {};
        if (SlotOf(req) == 0) {
            QmiMsg m;
            if (r.nas.Request(NAS_GET_SIGNAL_INFO, m) && m.Ok()) {
                SignalInfo g;
                ParseSignal(m, g);
                std::lock_guard<std::mutex> lk(r.stateMu);
                r.signal = g;
            }
            std::lock_guard<std::mutex> lk(r.stateMu);
            BuildRssi(r.signal, rssi);
        }
        r.Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, &rssi, sizeof(rssi));
    });
}

void RespondReg(const ReqDataInfo *req, bool cs)
{
    Ril::Get().Post([req, cs] {
        Ril &r = Ril::Get();
        ServingInfo s;
        if (SlotOf(req) == 0) {
            QmiMsg m;
            if (r.nas.Request(NAS_GET_SERVING_SYSTEM, m) && m.Ok()) {
                OnServing(m, false);
            }
            std::lock_guard<std::mutex> lk(r.stateMu);
            s = r.serving;
        }
        HRilRegStatusInfo ri;
        BuildRegInfo(s, cs, REG_RESPONSE_TYPE, ri);
        r.Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, &ri, sizeof(ri));
    });
}

void ReqGetCsRegStatus(const ReqDataInfo *req)
{
    RespondReg(req, true);
}

void ReqGetPsRegStatus(const ReqDataInfo *req)
{
    RespondReg(req, false);
}

bool IsNumeric(const std::string &s)
{
    if (s.empty()) return true;
    for (char c : s) {
        if ((c < '0' || c > '9') && c != ' ' && c != '-') return false;
    }
    return true;
}

std::string DecodeName(uint8_t enc, const std::vector<uint8_t> &b)
{
    std::string s;
    if (enc == 0x04) { /* UCS2, little endian from the modem */
        for (size_t i = 0; i + 1 < b.size(); i += 2) {
            uint16_t c = static_cast<uint16_t>(b[i] | b[i + 1] << 8);
            if (c < 0x80) {
                s.push_back(static_cast<char>(c));
            } else if (c < 0x800) {
                s.push_back(static_cast<char>(0xC0 | c >> 6));
                s.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            } else {
                s.push_back(static_cast<char>(0xE0 | c >> 12));
                s.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                s.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }
    } else {
        for (uint8_t c : b) {
            if (c >= 0x20 && c < 0x7F) s.push_back(static_cast<char>(c));
        }
    }
    return s;
}

void ReqGetOperatorInfo(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        Ril &r = Ril::Get();
        ServingInfo s;
        {
            QmiMsg m;
            if (SlotOf(req) == 0 && r.nas.Request(NAS_GET_SERVING_SYSTEM, m) && m.Ok()) {
                OnServing(m, false);
            }
            std::lock_guard<std::mutex> lk(r.stateMu);
            s = r.serving;
        }
        if (SlotOf(req) != 0 || !s.valid || s.regState != 1) {
            r.Respond(D_NETWORK, req, HRIL_ERR_NETWORK_SEARCHING, nullptr, 0);
            return;
        }
        std::string numeric = PlmnNumeric(s);
        std::string longName = s.description, shortName;
        {
            TlvWriter w;
            uint8_t plmn[4] = {static_cast<uint8_t>(s.mcc), static_cast<uint8_t>(s.mcc >> 8),
                static_cast<uint8_t>(s.mnc), static_cast<uint8_t>(s.mnc >> 8)};
            w.Raw(0x01, plmn, 4);
            w.U8(0x11, s.mnc3 ? 1 : 0);
            QmiMsg m;
            if (r.nas.Request(NAS_GET_PLMN_NAME, w, m, 5000) && m.Ok() && m.Has(0x10)) {
                Reader p = m.R(0x10);
                p.U8();
                p.Bytes(p.U8()); /* SPN */
                uint8_t senc = p.U8();
                p.U8();
                p.U8();
                std::vector<uint8_t> sn = p.Bytes(p.U8());
                uint8_t lenc = p.U8();
                p.U8();
                p.U8();
                std::vector<uint8_t> ln = p.Bytes(p.U8());
                if (p.Ok()) {
                    std::string l = DecodeName(lenc, ln), sh = DecodeName(senc, sn);
                    if (!l.empty()) longName = l;
                    if (!sh.empty()) shortName = sh;
                }
            }
        }
        if (IsNumeric(longName)) longName = numeric;
        if (shortName.empty()) shortName = longName;
        const char *resp[3] = {longName.c_str(), shortName.c_str(), numeric.c_str()};
        r.Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, resp, sizeof(resp));
    });
}

void ReqGetNetworkSearchInformation(const ReqDataInfo *req)
{
    /* a scan takes a minute or two: keep it off the request worker */
    std::thread([req] {
        Ril &r = Ril::Get();
        QmiMsg m;
        bool sent = r.nas.Request(NAS_NETWORK_SCAN, TlvWriter().U8(0x10, 0x07 /* GSM|UMTS|LTE */), m, 240000);
        if (!sent || !m.Ok() || !m.Has(0x10)) {
            r.Respond(D_NETWORK, req, HRIL_ERR_NETWORK_SEARCH_TIMEOUT, nullptr, 0);
            return;
        }
        struct Net {
            uint16_t mcc, mnc;
            uint8_t status;
            std::string desc, numeric;
            int rat = RADIO_TECHNOLOGY_UNKNOWN;
        };
        std::vector<Net> nets;
        Reader p = m.R(0x10);
        uint16_t n = p.U16();
        for (uint16_t i = 0; i < n && p.Ok(); i++) {
            Net x;
            x.mcc = p.U16();
            x.mnc = p.U16();
            x.status = p.U8();
            x.desc = p.Str8();
            nets.push_back(x);
        }
        if (m.Has(0x11)) {
            Reader q = m.R(0x11);
            uint16_t k = q.U16();
            for (uint16_t i = 0; i < k && q.Ok(); i++) {
                uint16_t mcc = q.U16(), mnc = q.U16();
                int8_t rif = q.S8();
                if (i < nets.size() && nets[i].mcc == mcc && nets[i].mnc == mnc) {
                    nets[i].rat = rif == RIF_LTE ? RADIO_TECHNOLOGY_LTE :
                        rif == RIF_UMTS ? RADIO_TECHNOLOGY_WCDMA :
                        rif == RIF_GSM ? RADIO_TECHNOLOGY_GSM : RADIO_TECHNOLOGY_UNKNOWN;
                }
            }
        }
        std::vector<bool> three(nets.size(), false);
        if (m.Has(0x12)) {
            Reader q = m.R(0x12);
            uint16_t k = q.U16();
            for (uint16_t i = 0; i < k && q.Ok(); i++) {
                q.U16();
                q.U16();
                uint8_t pcs = q.U8();
                if (i < three.size()) three[i] = pcs != 0;
            }
        }
        std::vector<AvailableOperInfo> infos(nets.size());
        std::vector<AvailableOperInfo *> ptrs;
        for (size_t i = 0; i < nets.size(); i++) {
            char buf[16];
            snprintf(buf, sizeof(buf), three[i] ? "%03u%03u" : "%03u%02u", nets[i].mcc, nets[i].mnc);
            nets[i].numeric = buf;
            if (nets[i].desc.empty()) nets[i].desc = nets[i].numeric;
            uint8_t st = nets[i].status;
            int status = (st & 0x01) ? 2 : (st & 0x10) ? 3 : (st & 0x02) ? 1 : 0;
            infos[i].longName = const_cast<char *>(nets[i].desc.c_str());
            infos[i].shortName = const_cast<char *>(nets[i].desc.c_str());
            infos[i].numeric = const_cast<char *>(nets[i].numeric.c_str());
            infos[i].status = status;
            infos[i].rat = nets[i].rat;
            ptrs.push_back(&infos[i]);
        }
        r.Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, ptrs.data(), ptrs.size() * sizeof(AvailableOperInfo *));
    }).detach();
}

void ReqGetNetworkSelectionMode(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        bool sent = Ril::Get().nas.Request(NAS_GET_SYSTEM_SELECTION_PREFERENCE, m);
        uint8_t mode = 0;
        if (sent && m.Ok()) {
            m.GetU8(0x16, mode);
        }
        int32_t v = mode;
        Ril::Get().Respond(D_NETWORK, req, sent ? HRIL_ERR_SUCCESS : HRIL_ERR_CMD_SEND_FAILURE, &v, sizeof(v));
    });
}

void ReqSetNetworkSelectionMode(const ReqDataInfo *req, const HRilSetNetworkModeInfo *data)
{
    int32_t mode = data ? data->selectMode : 0;
    std::string oper = (data && data->oper) ? data->oper : "";
    Ril::Get().Post([req, mode, oper] {
        TlvWriter w;
        uint16_t mcc = 0, mnc = 0;
        bool three = false;
        if (mode == 1 && oper.size() >= 5) {
            mcc = static_cast<uint16_t>(atoi(oper.substr(0, 3).c_str()));
            mnc = static_cast<uint16_t>(atoi(oper.substr(3).c_str()));
            three = oper.size() >= 6;
        }
        uint8_t sel[5] = {static_cast<uint8_t>(mode == 1 ? 1 : 0), static_cast<uint8_t>(mcc),
            static_cast<uint8_t>(mcc >> 8), static_cast<uint8_t>(mnc), static_cast<uint8_t>(mnc >> 8)};
        w.Raw(0x16, sel, 5);
        w.U8(0x17, 0); /* permanent */
        if (mode == 1) {
            w.U8(0x1A, three ? 1 : 0);
        }
        QmiMsg m;
        bool sent = Ril::Get().nas.Request(NAS_SET_SYSTEM_SELECTION_PREFERENCE, w, m, 30000);
        Ril::Get().Respond(D_NETWORK, req, QmiErr(sent, m), nullptr, 0);
    });
}

uint16_t PrefToMask(int32_t t)
{
    switch (t) {
        case HRIL_NETWORK_GSM: return RAT_GSM;
        case HRIL_NETWORK_WCDMA: return RAT_UMTS;
        case HRIL_NETWORK_LTE:
        case HRIL_NETWORK_NR_LTE: return RAT_LTE;
        case HRIL_NETWORK_LTE_WCDMA:
        case HRIL_NETWORK_NR_LTE_WCDMA: return RAT_LTE | RAT_UMTS;
        case HRIL_NETWORK_WCDMA_GSM: return RAT_UMTS | RAT_GSM;
        case HRIL_NETWORK_TDSCDMA: return RAT_TDS;
        case HRIL_NETWORK_TDSCDMA_GSM: return RAT_TDS | RAT_GSM;
        case HRIL_NETWORK_TDSCDMA_WCDMA_GSM: return RAT_TDS | RAT_UMTS | RAT_GSM;
        case HRIL_NETWORK_LTE_TDSCDMA_WCDMA_GSM:
        case HRIL_NETWORK_NR_LTE_TDSCDMA_WCDMA_GSM: return RAT_LTE | RAT_TDS | RAT_UMTS | RAT_GSM;
        default: return RAT_LTE | RAT_UMTS | RAT_GSM;
    }
}

int32_t MaskToPref(uint16_t m)
{
    m &= (RAT_GSM | RAT_UMTS | RAT_LTE | RAT_TDS);
    if (m == RAT_GSM) return HRIL_NETWORK_GSM;
    if (m == RAT_UMTS) return HRIL_NETWORK_WCDMA;
    if (m == RAT_LTE) return HRIL_NETWORK_LTE;
    if (m == (RAT_LTE | RAT_UMTS)) return HRIL_NETWORK_LTE_WCDMA;
    if (m == (RAT_UMTS | RAT_GSM)) return HRIL_NETWORK_WCDMA_GSM;
    if (m == (RAT_TDS | RAT_UMTS | RAT_GSM)) return HRIL_NETWORK_TDSCDMA_WCDMA_GSM;
    if (m == (RAT_LTE | RAT_TDS | RAT_UMTS | RAT_GSM)) return HRIL_NETWORK_LTE_TDSCDMA_WCDMA_GSM;
    return HRIL_NETWORK_LTE_WCDMA_GSM;
}

void ReqGetPreferredNetwork(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        QmiMsg m;
        bool sent = Ril::Get().nas.Request(NAS_GET_SYSTEM_SELECTION_PREFERENCE, m);
        uint16_t mask = RAT_LTE | RAT_UMTS | RAT_GSM;
        if (sent && m.Ok()) {
            m.GetU16(0x11, mask);
        }
        int32_t v = MaskToPref(mask);
        Ril::Get().Respond(D_NETWORK, req, sent ? HRIL_ERR_SUCCESS : HRIL_ERR_CMD_SEND_FAILURE, &v, sizeof(v));
    });
}

void ReqSetPreferredNetwork(const ReqDataInfo *req, const int32_t *data)
{
    int32_t t = data ? *data : HRIL_NETWORK_LTE_WCDMA_GSM;
    Ril::Get().Post([req, t] {
        TlvWriter w;
        w.U16(0x11, PrefToMask(t));
        w.U8(0x17, 0);
        QmiMsg m;
        bool sent = Ril::Get().nas.Request(NAS_SET_SYSTEM_SELECTION_PREFERENCE, w, m, 30000);
        Ril::Get().Respond(D_NETWORK, req, QmiErr(sent, m), nullptr, 0);
    });
}

void ReqGetNeighboringCellInfoList(const ReqDataInfo *req)
{
    CellInfoList l = {0, nullptr};
    Ril::Get().Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, &l, sizeof(l));
}

void ReqGetCurrentCellInfo(const ReqDataInfo *req)
{
    Ril::Get().Post([req] {
        Ril &r = Ril::Get();
        ServingInfo s;
        SignalInfo g;
        {
            std::lock_guard<std::mutex> lk(r.stateMu);
            s = r.serving;
            g = r.signal;
        }
        CurrentCellInfoVendor cell = {};
        CurrentCellInfoList list = {0, &cell};
        if (SlotOf(req) == 0 && s.valid && s.regState == 1) {
            cell.mcc = s.mcc;
            cell.mnc = s.mnc;
            int tech = MapRadioTech(s);
            if (tech == RADIO_TECHNOLOGY_LTE || tech == RADIO_TECHNOLOGY_LTE_CA || tech == RADIO_TECHNOLOGY_NR) {
                cell.ratType = NETWORK_TYPE_LTE;
                cell.ServiceCellParas.lte.cellId = static_cast<int32_t>(s.cid);
                cell.ServiceCellParas.lte.tac = s.tac;
                cell.ServiceCellParas.lte.rsrp = g.lteRsrp;
                cell.ServiceCellParas.lte.rsrq = g.lteRsrq;
                cell.ServiceCellParas.lte.rssi = g.lteRssi;
            } else if (tech == RADIO_TECHNOLOGY_GSM) {
                cell.ratType = NETWORK_TYPE_GSM;
                cell.ServiceCellParas.gsm.cellId = static_cast<int32_t>(s.cid);
                cell.ServiceCellParas.gsm.lac = s.lac;
                cell.ServiceCellParas.gsm.rxlev = g.gsmRssi;
            } else if (tech == RADIO_TECHNOLOGY_TD_SCDMA) {
                cell.ratType = NETWORK_TYPE_TDSCDMA;
                cell.ServiceCellParas.tdscdma.cellId = static_cast<int32_t>(s.cid);
                cell.ServiceCellParas.tdscdma.lac = s.lac;
                cell.ServiceCellParas.tdscdma.rscp = g.tdRscp;
            } else {
                cell.ratType = NETWORK_TYPE_WCDMA;
                cell.ServiceCellParas.wcdma.cellId = static_cast<int32_t>(s.cid);
                cell.ServiceCellParas.wcdma.lac = s.lac;
                cell.ServiceCellParas.wcdma.rscp = g.hasWcdmaRscp ? g.wcdmaRscp : g.wcdmaRssi;
                cell.ServiceCellParas.wcdma.rxlev = g.wcdmaRssi;
                cell.ServiceCellParas.wcdma.ecno = -(g.wcdmaEcio / 2);
            }
            list.itemNum = 1;
        }
        r.Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, &list, sizeof(list));
    });
}

void ReqGetPhysicalChannelConfig(const ReqDataInfo *req)
{
    HRilChannelConfigList l = {0, nullptr};
    Ril::Get().Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, &l, sizeof(l));
}

void ReqOk(const ReqDataInfo *req)
{
    Ril::Get().Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, nullptr, 0);
}

void ReqSetLocateUpdates(const ReqDataInfo *req, HRilRegNotifyMode)
{
    ReqOk(req);
}

void ReqSetNotificationFilter(const ReqDataInfo *req, const int32_t *)
{
    ReqOk(req);
}

void ReqSetDeviceState(const ReqDataInfo *req, const int32_t *, const int32_t *)
{
    ReqOk(req);
}

void ReqRespondInt(const ReqDataInfo *req, int32_t v)
{
    Ril::Get().Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, &v, sizeof(v));
}

void ReqGetRrcConnectionState(const ReqDataInfo *req)
{
    ReqRespondInt(req, 0);
}

void ReqSetNrOptionMode(const ReqDataInfo *req, const int32_t *)
{
    ReqOk(req);
}

void ReqGetNrOptionMode(const ReqDataInfo *req)
{
    ReqRespondInt(req, 0);
}

void ReqGetImsRegStatus(const ReqDataInfo *req)
{
    HRilImsRegStatusInfo i = {0, 0, 0};
    Ril::Get().Respond(D_NETWORK, req, HRIL_ERR_SUCCESS, &i, sizeof(i));
}
} // namespace

int MapRadioTech(const ServingInfo &s)
{
    if (HasIf(s, RIF_NR)) return RADIO_TECHNOLOGY_NR;
    if (HasIf(s, RIF_LTE)) return RADIO_TECHNOLOGY_LTE;
    if (HasIf(s, RIF_UMTS)) {
        if (HasCap(s, 0x0D) || HasCap(s, 0x0C)) return RADIO_TECHNOLOGY_HSPAP;
        if (HasCap(s, 0x03) || HasCap(s, 0x04)) return RADIO_TECHNOLOGY_HSPA;
        return RADIO_TECHNOLOGY_WCDMA;
    }
    if (HasIf(s, RIF_TDSCDMA)) return RADIO_TECHNOLOGY_TD_SCDMA;
    if (HasIf(s, RIF_GSM)) return RADIO_TECHNOLOGY_GSM;
    if (HasIf(s, RIF_EVDO)) return RADIO_TECHNOLOGY_EVDO;
    if (HasIf(s, RIF_CDMA_1X)) return RADIO_TECHNOLOGY_1XRTT;
    return RADIO_TECHNOLOGY_UNKNOWN;
}

std::string PlmnNumeric(const ServingInfo &s)
{
    char buf[16];
    snprintf(buf, sizeof(buf), s.mnc3 || s.mnc > 99 ? "%03u%03u" : "%03u%02u", s.mcc, s.mnc);
    return buf;
}

void FillVoiceRadioInfo(HRilVoiceRadioInfo &info)
{
    static const char *const sysNames[] = {"NO SERVICE", "GSM", "CDMA", "WCDMA", "TD-SCDMA", "WIMAX", "LTE",
        "LTE-CA", "NR"};
    Ril &r = Ril::Get();
    ServingInfo s;
    bool simReady;
    {
        std::lock_guard<std::mutex> lk(r.stateMu);
        s = r.serving;
        simReady = r.cards[0].cardState == 1 && r.cards[0].appState == 7;
    }
    info = {};
    bool reg = s.valid && s.regState == 1;
    info.srvStatus = reg ? HRIL_SERVICE_VALID : HRIL_NO_SRV_SERVICE;
    bool cs = reg && s.csAttach != 2, ps = reg && s.psAttach != 2;
    info.srvDomain = cs && ps ? HRIL_CS_PS_SERVICE : cs ? HRIL_CS_SERVICE : ps ? HRIL_PS_SERVICE : HRIL_NO_DOMAIN_SERVICE;
    info.roamStatus = s.roaming ? HRIL_ROAMING : HRIL_NO_ROAM;
    info.simStatus = simReady ? HRIL_USIM_VALID : HRIL_NO_USIM;
    info.lockStatus = HRIL_SIM_CARD_UNLOCK;
    int tech = reg ? MapRadioTech(s) : RADIO_TECHNOLOGY_UNKNOWN;
    HRilSysMode mode = HRIL_NO_SYSMODE_SERVICE;
    switch (tech) {
        case RADIO_TECHNOLOGY_GSM: mode = HRIL_GSM_MODE; break;
        case RADIO_TECHNOLOGY_WCDMA:
        case RADIO_TECHNOLOGY_HSPA:
        case RADIO_TECHNOLOGY_HSPAP: mode = HRIL_WCDMA_MODE; break;
        case RADIO_TECHNOLOGY_TD_SCDMA: mode = HRIL_TDSCDMA_MODE; break;
        case RADIO_TECHNOLOGY_LTE: mode = HRIL_LTE_MODE; break;
        case RADIO_TECHNOLOGY_LTE_CA: mode = HRIL_LTE_CA_MODE; break;
        case RADIO_TECHNOLOGY_NR: mode = HRIL_NR_MODE; break;
        case RADIO_TECHNOLOGY_1XRTT:
        case RADIO_TECHNOLOGY_EVDO: mode = HRIL_CDMA_MODE; break;
        default: break;
    }
    info.sysMode = mode;
    info.sysModeName = const_cast<char *>(sysNames[mode]);
    info.actType = static_cast<HRilRadioTech>(tech);
    info.actName = const_cast<char *>(sysNames[mode]);
}

void NetworkRefresh(bool notify)
{
    QmiMsg m;
    if (Ril::Get().nas.Request(NAS_GET_SERVING_SYSTEM, m) && m.Ok()) {
        OnServing(m, notify);
    }
    QmiMsg g;
    if (Ril::Get().nas.Request(NAS_GET_SIGNAL_INFO, g) && g.Ok() && notify) {
        OnSignal(g);
    }
}

void NetworkOnUp()
{
    Ril &r = Ril::Get();
    static bool handlerSet = false;
    if (!handlerSet) {
        handlerSet = true;
        r.nas.OnIndication([](const QmiMsg &m) {
            switch (m.msgId) {
                case NAS_GET_SERVING_SYSTEM: /* Serving System indication shares the id */
                    Ril::Get().Post([m] { OnServing(m, true); });
                    break;
                case NAS_SIGNAL_INFO_IND:
                    Ril::Get().Post([m] { OnSignal(m); });
                    break;
                case NAS_NETWORK_TIME_IND:
                    Ril::Get().Post([m] { OnNetworkTime(m); });
                    break;
                default:
                    break;
            }
        });
    }
    TlvWriter w;
    w.U8(0x10, 0).U8(0x13, 1).U8(0x17, 1).U8(0x18, 0).U8(0x19, 1);
    QmiMsg m;
    r.nas.Request(NAS_REGISTER_INDICATIONS, w, m);
    /* signal indications when crossing these levels */
    TlvWriter c;
    int8_t rssi[] = {-110, -103, -97, -89, -81, -73};
    std::vector<uint8_t> rs = {static_cast<uint8_t>(sizeof(rssi))};
    for (auto v : rssi) rs.push_back(static_cast<uint8_t>(v));
    c.Raw(0x10, rs.data(), rs.size());
    int16_t rsrp[] = {-128, -118, -108, -98, -88};
    std::vector<uint8_t> rp = {static_cast<uint8_t>(sizeof(rsrp) / sizeof(rsrp[0]))};
    for (auto v : rsrp) {
        rp.push_back(static_cast<uint8_t>(v));
        rp.push_back(static_cast<uint8_t>(static_cast<uint16_t>(v) >> 8));
    }
    c.Raw(0x16, rp.data(), rp.size());
    QmiMsg cm;
    r.nas.Request(NAS_CONFIG_SIGNAL_INFO, c, cm);
    NetworkRefresh(true);
}

const HRilNetworkReq g_networkOps = {
    .GetImsRegStatus = ReqGetImsRegStatus,
    .GetSignalStrength = ReqGetSignalStrength,
    .GetCsRegStatus = ReqGetCsRegStatus,
    .GetPsRegStatus = ReqGetPsRegStatus,
    .GetOperatorInfo = ReqGetOperatorInfo,
    .GetNeighboringCellInfoList = ReqGetNeighboringCellInfoList,
    .GetCurrentCellInfo = ReqGetCurrentCellInfo,
    .GetNetworkSearchInformation = ReqGetNetworkSearchInformation,
    .GetNetworkSelectionMode = ReqGetNetworkSelectionMode,
    .SetNetworkSelectionMode = ReqSetNetworkSelectionMode,
    .GetPreferredNetwork = ReqGetPreferredNetwork,
    .SetPreferredNetwork = ReqSetPreferredNetwork,
    .GetPhysicalChannelConfig = ReqGetPhysicalChannelConfig,
    .SetLocateUpdates = ReqSetLocateUpdates,
    .SetNotificationFilter = ReqSetNotificationFilter,
    .SetDeviceState = ReqSetDeviceState,
    .GetRrcConnectionState = ReqGetRrcConnectionState,
    .SetNrOptionMode = ReqSetNrOptionMode,
    .GetNrOptionMode = ReqGetNrOptionMode,
    .GetNrSsbId = nullptr,
};

} // namespace sweetril
