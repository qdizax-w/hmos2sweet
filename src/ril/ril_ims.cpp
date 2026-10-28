/*
 * sweet RIL: VoLTE on the modem's own IMS stack.
 *
 * On sm6150 the IMS client (SIP, voice media) runs on the modem. Android
 * drives it from userspace through a handful of QMI services; we do the same,
 * without the Qualcomm daemons:
 *
 *  - NAS  Set System Selection Preference, Voice Domain Preference: PS only
 *         ("calls only over VoLTE") or PS preferred
 *         (persist.sweet.volte_only = 0)
 *  - IMS settings (0x12) Set IMS Services Enabled Setting: VoLTE, IMS, UT
 *         (supplementary services over IMS), SMS and USSD over IMS on
 *  - IMSA (0x21) registration / service status, logged and used to tell the
 *         call list which domain a call is on
 *  - IMS DCM (0x302) PDP Activate Request for the "ims" APN when the modem
 *         stays unregistered on LTE: on Android imsdatadaemon does this
 *  - PDC (0x24) the active carrier configuration (MBN) is logged: VoLTE
 *         needs the operator's MBN
 *
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "ril.h"

#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <atomic>
#include <thread>
#include <unistd.h>

namespace sweetril {
namespace {

enum : uint32_t {
    QMI_SVC_IMS = 0x12,
    QMI_SVC_PDC = 0x24,
    QMI_SVC_IMSA = 0x21,
    QMI_SVC_IMSDCM = 0x302,
};

enum : uint16_t {
    IMS_SET_SERVICES_ENABLED = 0x008F,
    IMS_GET_SERVICES_ENABLED = 0x0090,
    IMSA_GET_REG_STATUS = 0x0020,
    IMSA_GET_SERVICES_STATUS = 0x0021,
    IMSA_REGISTER_INDICATIONS = 0x0022,
    IMSA_REG_STATUS_IND = 0x0023,
    IMSA_SERVICES_STATUS_IND = 0x0024,
    IMSDCM_PDP_ACTIVATE = 0x0020,
    PDC_REGISTER = 0x0020,
    PDC_GET_SELECTED_CONFIG = 0x0022,
    PDC_LIST_CONFIGS = 0x0024,
    PDC_GET_CONFIG_INFO = 0x0028,
    WDS_GET_PROFILE_LIST = 0x002A,
    WDS_GET_PROFILE_SETTINGS = 0x002B,
    WDS_CREATE_PROFILE = 0x0027,
};

QmiClient g_ims{QMI_SVC_IMS, "IMS"};
QmiClient g_imsa{QMI_SVC_IMSA, "IMSA"};
QmiClient g_imsdcm{QMI_SVC_IMSDCM, "IMSDCM"};
QmiClient g_pdc{QMI_SVC_PDC, "PDC"};

std::atomic<uint32_t> g_regStatus{0};    /* QmiImsaImsRegistrationStatus: 0 not, 1 registering, 2 registered */
std::atomic<uint32_t> g_voiceStatus{0};  /* QmiImsaServiceStatus: 0 unavailable, 1 limited, 2 available */

bool ParamOn(const char *name, bool def)
{
    using GetParameterFn = int (*)(const char *, const char *, char *, unsigned int);
    static GetParameterFn get = [] {
        void *h = dlopen("libbegetutil.z.so", RTLD_NOW);
        return h ? reinterpret_cast<GetParameterFn>(dlsym(h, "GetParameter")) : nullptr;
    }();
    if (get == nullptr) {
        return def;
    }
    char v[16] = {0};
    if (get(name, "", v, sizeof(v)) <= 0 || v[0] == 0) {
        return def;
    }
    return v[0] == '1' || v[0] == 't' || v[0] == 'y';
}

bool OnLte()
{
    Ril &r = Ril::Get();
    std::lock_guard<std::mutex> lk(r.stateMu);
    if (!r.serving.valid || r.serving.psAttach != 1) {
        return false;
    }
    for (auto rif : r.serving.radioIfs) {
        if (rif == 0x08) return true; /* LTE */
    }
    return false;
}

void SetVoiceDomain()
{
    bool only = ParamOn("persist.sweet.volte_only", true);
    uint32_t pref = only ? 0x01 /* PS only */ : 0x03 /* PS preferred */;
    QmiMsg m;
    bool sent = Ril::Get().nas.Request(NAS_SET_SYSTEM_SELECTION_PREFERENCE,
        TlvWriter().U8(0x17, 0x01 /* permanent */).U32(0x23, pref), m);
    Klog("ims: voice domain %s -> %s (err %u)", only ? "PS only (VoLTE only)" : "PS preferred",
        sent && m.Ok() ? "ok" : "failed", m.error);
}

void EnableImsServices()
{
    if (!g_ims.WaitUp(1000)) {
        Klog("ims: IMS settings service not present");
        return;
    }
    TlvWriter w;
    w.U8(0x10, 1)  /* VoLTE */
        .U8(0x18, 1)  /* IMS service */
        .U8(0x19, 1)  /* UT: supplementary services over IMS */
        .U8(0x1A, 1)  /* SMS over IMS */
        .U8(0x1C, 1); /* USSD over IMS */
    QmiMsg m;
    bool sent = g_ims.Request(IMS_SET_SERVICES_ENABLED, w, m);
    Klog("ims: enable VoLTE/IMS services -> %s (err %u)", sent && m.Ok() ? "ok" : "failed", m.error);
    QmiMsg g;
    if (g_ims.Request(IMS_GET_SERVICES_ENABLED, g) && g.Ok()) {
        uint8_t volte = 0, reg = 0;
        g.GetU8(0x11, volte);
        g.GetU8(0x19, reg);
        Klog("ims: settings now VoLTE %u, IMS registration %u", volte, reg);
    }
}

void OnImsa(const QmiMsg &m)
{
    if (m.msgId == IMSA_REG_STATUS_IND) {
        uint32_t st = 0, tech = 0;
        uint16_t code = 0;
        std::string msg;
        m.GetU32(0x11, st);
        m.GetU32(0x13, tech);
        m.GetU16(0x10, code);
        m.GetStr(0x12, msg);
        g_regStatus = st;
        Klog("ims: registration %s (tech %u, code %u %s)", st == 2 ? "REGISTERED" : st == 1 ? "registering" :
            "not registered", tech, code, msg.c_str());
    } else if (m.msgId == IMSA_SERVICES_STATUS_IND) {
        uint32_t voice = 0;
        if (m.GetU32(0x11, voice)) {
            g_voiceStatus = voice;
            Klog("ims: voice service %s", voice == 2 ? "available (VoLTE ready)" : voice == 1 ? "limited" :
                "unavailable");
        }
    }
}

void ImsaUp()
{
    static bool handler = false;
    if (!handler) {
        handler = true;
        g_imsa.OnIndication(OnImsa);
    }
    QmiMsg m;
    g_imsa.Request(IMSA_REGISTER_INDICATIONS, TlvWriter().U8(0x10, 1).U8(0x11, 1), m);
    QmiMsg s;
    if (g_imsa.Request(IMSA_GET_REG_STATUS, s) && s.Ok()) {
        uint32_t st = 0, tech = 0;
        s.GetU32(0x12, st);
        s.GetU32(0x14, tech);
        g_regStatus = st;
        Klog("ims: registration status %u (tech %u)", st, tech);
    }
}

/* id of the WDS profile whose APN is "ims" (created if missing) */
int ImsProfile()
{
    Ril &r = Ril::Get();
    QmiMsg l;
    if (r.wds.Request(WDS_GET_PROFILE_LIST, TlvWriter().U8(0x10, 0 /* 3GPP */), l) && l.Ok() && l.Has(0x01)) {
        Reader rd = l.R(0x01);
        uint8_t n = rd.U8();
        for (uint8_t i = 0; i < n && rd.Ok(); i++) {
            uint8_t type = rd.U8();
            uint8_t idx = rd.U8();
            std::string name = rd.Str8();
            uint8_t id[2] = {type, idx};
            QmiMsg ps;
            if (r.wds.Request(WDS_GET_PROFILE_SETTINGS, TlvWriter().Raw(0x01, id, 2), ps) && ps.Ok()) {
                std::string apn;
                ps.GetStr(0x14, apn);
                if (strcasecmp(apn.c_str(), "ims") == 0) {
                    Klog("ims: WDS profile %u (%s) has APN ims", idx, name.c_str());
                    return idx;
                }
            }
        }
    }
    TlvWriter w;
    w.U8(0x01, 0).Str(0x10, "IMS").U8(0x11, 0x03 /* IPv4v6 */).Str(0x14, "ims").U8(0x1F, 1 /* P-CSCF via PCO */)
        .U8(0x22, 1 /* IMCN */);
    QmiMsg c;
    if (r.wds.Request(WDS_CREATE_PROFILE, w, c) && c.Ok() && c.Has(0x01)) {
        Reader rd = c.R(0x01);
        rd.U8();
        uint8_t idx = rd.U8();
        Klog("ims: created WDS profile %u for APN ims", idx);
        return idx;
    }
    Klog("ims: no WDS profile for the ims APN (err %u)", c.error);
    return 0;
}

void ActivateImsPdn()
{
    if (!g_imsdcm.WaitUp(500)) {
        static bool logged;
        if (!logged) {
            logged = true;
            Klog("ims: IMS DCM service not present, the modem manages the IMS PDN itself");
        }
        return;
    }
    static bool handler = false;
    if (!handler) {
        handler = true;
        g_imsdcm.OnIndication([](const QmiMsg &m) {
            if (m.msgId == IMSDCM_PDP_ACTIVATE) {
                uint8_t id = 0;
                m.GetU8(0x01, id);
                std::string addr;
                if (m.Has(0x11)) {
                    Reader r = m.R(0x11);
                    r.U32();
                    addr = r.Str8();
                }
                Klog("ims: IMS PDN %u up (result %u err %u) %s", id, m.result, m.error, addr.c_str());
            }
        });
    }
    int profile = ImsProfile();
    for (uint32_t fam : {0u /* v4 */, 1u /* v6 */}) {
        std::vector<uint8_t> p;
        const char *apn = "ims";
        p.push_back(static_cast<uint8_t>(strlen(apn)));
        p.insert(p.end(), apn, apn + strlen(apn));
        auto u32 = [&p](uint32_t v) {
            for (int i = 0; i < 4; i++) p.push_back(static_cast<uint8_t>(v >> (8 * i)));
        };
        u32(0x00);          /* APN type IMS */
        u32(0x01);          /* RAT LTE */
        u32(fam);
        u32(static_cast<uint32_t>(profile));
        static uint32_t seq = 1;
        QmiMsg m;
        bool sent = g_imsdcm.Request(IMSDCM_PDP_ACTIVATE, TlvWriter().Raw(0x01, p.data(), p.size()).U32(0x10, seq++), m);
        Klog("ims: IMS PDN request (%s, profile %d) -> %s (err %u)", fam ? "IPv6" : "IPv4", profile,
            sent && m.Ok() ? "accepted" : "refused", m.error);
    }
}

void LogCarrierConfig()
{
    if (!g_pdc.WaitUp(500)) {
        return;
    }
    static bool handler = false;
    if (!handler) {
        handler = true;
        g_pdc.OnIndication([](const QmiMsg &m) {
            if (m.msgId == PDC_GET_SELECTED_CONFIG) {
                std::string id;
                if (const auto *v = m.Get(0x11)) {
                    id = HexEncode(v->size() > 1 ? std::vector<uint8_t>(v->begin() + 1, v->end()) : *v);
                }
                Klog("ims: active carrier config (MBN) id %s", id.empty() ? "none" : id.c_str());
            } else if (m.msgId == PDC_LIST_CONFIGS) {
                if (m.Has(0x11)) {
                    Reader r = m.R(0x11);
                    uint8_t n = r.U8();
                    Klog("ims: %u carrier configs on the modem", n);
                    for (uint8_t i = 0; i < n && r.Ok(); i++) {
                        uint32_t type = r.U32();
                        uint8_t len = r.U8();
                        std::vector<uint8_t> id = r.Bytes(len);
                        if (!r.Ok()) break;
                        std::vector<uint8_t> body;
                        for (int k = 0; k < 4; k++) body.push_back(static_cast<uint8_t>(type >> (8 * k)));
                        body.push_back(len);
                        body.insert(body.end(), id.begin(), id.end());
                        QmiMsg q;
                        g_pdc.Request(PDC_GET_CONFIG_INFO, TlvWriter().Raw(0x01, body.data(), body.size())
                            .U32(0x10, 100 + i), q);
                    }
                }
            } else if (m.msgId == PDC_GET_CONFIG_INFO) {
                std::string desc;
                if (m.Has(0x12)) {
                    Reader r = m.R(0x12);
                    desc = r.Str8();
                }
                uint32_t ver = 0;
                m.GetU32(0x13, ver);
                Klog("ims: MBN '%s' version 0x%08x", desc.c_str(), ver);
            }
        });
    }
    QmiMsg r;
    g_pdc.Request(PDC_REGISTER, TlvWriter().U8(0x10, 1), r);
    QmiMsg a;
    g_pdc.Request(PDC_GET_SELECTED_CONFIG, TlvWriter().U32(0x01, 1 /* software */).U32(0x10, 1), a);
    QmiMsg b;
    g_pdc.Request(PDC_LIST_CONFIGS, TlvWriter().U32(0x10, 2).U32(0x11, 1), b);
}

void ImsThread()
{
    /* wait for the core services */
    Ril &r = Ril::Get();
    while (!r.nas.Up() || !r.wds.Up()) {
        sleep(2);
    }
    sleep(5);
    SetVoiceDomain();
    EnableImsServices();
    if (g_imsa.WaitUp(2000)) {
        ImsaUp();
    } else {
        Klog("ims: IMSA service not present");
    }
    LogCarrierConfig();
    int unregisteredOnLte = 0;
    bool pdnAsked = false;
    for (;;) {
        sleep(10);
        if (!OnLte()) {
            unregisteredOnLte = 0;
            continue;
        }
        if (g_regStatus == 2) {
            unregisteredOnLte = 0;
            continue;
        }
        /* 40 s on LTE without IMS registration: ask for the IMS PDN */
        if (++unregisteredOnLte >= 4 && !pdnAsked) {
            pdnAsked = true;
            ActivateImsPdn();
        }
        /* the SIM may arrive late: enable the services again from time to time */
        if (unregisteredOnLte % 30 == 0) {
            EnableImsServices();
            pdnAsked = false;
        }
    }
}
} // namespace

bool ImsVoiceReady()
{
    return g_regStatus == 2 && g_voiceStatus == 2;
}

void ImsInit()
{
    auto &w = ServiceWatcher::Get();
    w.Add(&g_ims);
    w.Add(&g_imsa);
    w.Add(&g_imsdcm);
    w.Add(&g_pdc);
    std::thread(ImsThread).detach();
}

} // namespace sweetril
