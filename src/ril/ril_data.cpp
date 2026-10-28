/*
 * sweet RIL: mobile data (QMI WDS/WDA over the IPA rmnet data path).
 *
 * Data path on sm6150 (downstream 4.14 kernel): the IPA driver exposes
 * rmnet_ipa0; sweet-rproc configures its QMAP pipes and creates the
 * rmnet_data0..3 mux devices (mux ids 1..4) at boot. Here we tell the
 * modem the data format (raw IP, QMAP) for the embedded IPA endpoint, bind
 * one WDS client per PDN to a mux id and start the network; the telephony
 * stack configures addresses/routes on rmnet_dataN from our response.
 *
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "ril.h"

#include <arpa/inet.h>
#include <cstdio>
#include <memory>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace sweetril {
namespace {

constexpr int MAX_CALLS = 4;
constexpr uint16_t WDS_START_NETWORK = 0x0020;
constexpr uint16_t WDS_STOP_NETWORK = 0x0021;
constexpr uint16_t WDS_PKT_SRVC_STATUS_IND = 0x0022;
constexpr uint16_t WDS_GET_CURRENT_SETTINGS = 0x002D;
constexpr uint16_t WDS_SET_IP_FAMILY = 0x004D;
constexpr uint16_t WDS_BIND_MUX_DATA_PORT = 0x00A2;
constexpr uint16_t WDA_SET_DATA_FORMAT = 0x0020;
constexpr uint32_t EP_TYPE_EMBEDDED = 0x04;

/* rmnet extended ioctls (include/uapi/linux/msm_rmnet.h) */
constexpr unsigned long RMNET_IOCTL_EXTENDED = 0x000089FD;
constexpr uint32_t RMNET_IOCTL_GET_EP_PAIR = 0x0010;
struct RmnetExt {
    uint32_t cmd;
    union {
        uint32_t data;
        struct {
            uint32_t consumer;
            uint32_t producer;
        } ep;
        uint8_t pad[20];
    } u;
};

struct DataCall {
    bool used = false;
    bool active = false;
    int cid = -1;
    int mux = 0;
    uint32_t handle = 0;
    std::string apn, type, ifname, address, gateway, dns, dnsSec;
    int mtu = 0;
    std::unique_ptr<QmiClient> wds;
};

std::mutex g_dataMu;
DataCall g_calls[MAX_CALLS];
uint32_t g_epId = 0;
bool g_formatSet = false;

bool ReadEpId(uint32_t &ep)
{
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    RmnetExt ext = {};
    ext.cmd = RMNET_IOCTL_GET_EP_PAIR;
    struct ifreq ifr = {};
    strncpy(ifr.ifr_name, "rmnet_ipa0", IFNAMSIZ - 1);
    ifr.ifr_ifru.ifru_data = reinterpret_cast<char *>(&ext);
    int rc = ioctl(fd, RMNET_IOCTL_EXTENDED, &ifr);
    close(fd);
    if (rc < 0) {
        Klog("data: GET_EP_PAIR on rmnet_ipa0 failed errno %d", errno);
        return false;
    }
    ep = (ext.u.ep.consumer & 0xFF) | ((ext.u.ep.producer & 0xFF) << 8);
    Klog("data: IPA ep pair cons %u prod %u -> iface id 0x%x", ext.u.ep.consumer, ext.u.ep.producer, ep);
    return true;
}

bool EnsureDataFormat()
{
    if (g_formatSet) return true;
    if (!ReadEpId(g_epId)) return false;
    TlvWriter w;
    w.U8(0x10, 0);          /* no QoS header */
    w.U32(0x11, 0x02);      /* raw IP */
    w.U32(0x12, 0x05);      /* UL QMAP */
    w.U32(0x13, 0x05);      /* DL QMAP */
    w.U32(0x15, 32);        /* DL max datagrams */
    w.U32(0x16, 16384);     /* DL max size */
    uint8_t ep[8];
    uint32_t t = EP_TYPE_EMBEDDED, id = g_epId;
    for (int i = 0; i < 4; i++) {
        ep[i] = static_cast<uint8_t>(t >> (8 * i));
        ep[4 + i] = static_cast<uint8_t>(id >> (8 * i));
    }
    w.Raw(0x17, ep, 8);
    QmiMsg m;
    bool sent = Ril::Get().wda.Request(WDA_SET_DATA_FORMAT, w, m);
    Klog("data: WDA set data format %s (err %u)", sent && m.Ok() ? "ok" : "FAILED", m.error);
    g_formatSet = sent && m.Ok();
    return g_formatSet;
}

std::string Ip4(uint32_t a)
{
    char b[20];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF);
    return b;
}

int PrefixLen(uint32_t mask)
{
    int n = 0;
    while (mask & 0x80000000u) {
        n++;
        mask <<= 1;
    }
    return n ? n : 32;
}

std::string Ip6(Reader &r)
{
    uint8_t a[16] = {};
    std::vector<uint8_t> raw = r.Bytes(16); /* network byte order */
    if (raw.size() == 16) {
        memcpy(a, raw.data(), 16);
    }
    char b[INET6_ADDRSTRLEN] = {};
    inet_ntop(AF_INET6, a, b, sizeof(b));
    return b;
}

void FillResponse(const DataCall &c, HRilDataCallResponse &r)
{
    r = {};
    r.reason = HRIL_PDP_ERR_NONE;
    r.retryTime = -1;
    r.cid = c.cid;
    r.active = c.active ? 2 : 0;
    r.type = const_cast<char *>(c.type.c_str());
    r.netPortName = const_cast<char *>(c.ifname.c_str());
    r.address = const_cast<char *>(c.address.c_str());
    r.dns = const_cast<char *>(c.dns.c_str());
    r.dnsSec = const_cast<char *>(c.dnsSec.c_str());
    r.gateway = const_cast<char *>(c.gateway.c_str());
    r.maxTransferUnit = c.mtu;
    r.pCscfPrimAddr = const_cast<char *>("");
    r.pCscfSecAddr = const_cast<char *>("");
    r.pduSessionId = 0;
}

void NotifyList()
{
    std::vector<HRilDataCallResponse> list;
    std::lock_guard<std::mutex> lk(g_dataMu);
    for (auto &c : g_calls) {
        if (!c.used) continue;
        HRilDataCallResponse r;
        FillResponse(c, r);
        list.push_back(r);
    }
    Ril::Get().Notify(D_DATA, 0, HNOTI_DATA_PDP_CONTEXT_LIST_UPDATED, list.empty() ? nullptr : list.data(),
        list.size() * sizeof(HRilDataCallResponse));
}

void ReleaseCall(int i)
{
    g_calls[i].active = false;
    g_calls[i].used = false;
    g_calls[i].wds.reset();
}

void RespondFail(const ReqDataInfo *req, int cid, int reason)
{
    HRilDataCallResponse r = {};
    r.reason = reason;
    r.retryTime = 5000;
    r.cid = cid;
    r.active = 0;
    r.type = const_cast<char *>("");
    r.netPortName = const_cast<char *>("");
    r.address = const_cast<char *>("");
    r.dns = const_cast<char *>("");
    r.dnsSec = const_cast<char *>("");
    r.gateway = const_cast<char *>("");
    r.pCscfPrimAddr = const_cast<char *>("");
    r.pCscfSecAddr = const_cast<char *>("");
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, &r, sizeof(r));
}

void Activate(const ReqDataInfo *req, int cid, std::string apn, std::string type, std::string user, std::string pass,
    int verType)
{
    Ril &ril = Ril::Get();
    if (!EnsureDataFormat()) {
        RespondFail(req, cid, HRIL_PDP_ERR_RETRY);
        return;
    }
    int slot = -1;
    {
        std::lock_guard<std::mutex> lk(g_dataMu);
        for (int i = 0; i < MAX_CALLS; i++) {
            if (!g_calls[i].used) {
                slot = i;
                g_calls[i].used = true;
                break;
            }
        }
    }
    if (slot < 0) {
        RespondFail(req, cid, HRIL_PDP_ERR_MAX_NUM_OF_PDP_CONTEXTS);
        return;
    }
    uint32_t node = 0, port = 0;
    ril.wds.Endpoint(node, port);
    auto client = std::make_unique<QmiClient>(QMI_SVC_WDS, "WDS-call");
    client->Attach(node, port);
    int mux = slot + 1;
    /* bind this client to the mux channel of rmnet_data<slot> */
    TlvWriter b;
    uint8_t ep[8];
    for (int i = 0; i < 4; i++) {
        ep[i] = static_cast<uint8_t>(EP_TYPE_EMBEDDED >> (8 * i));
        ep[4 + i] = static_cast<uint8_t>(g_epId >> (8 * i));
    }
    b.Raw(0x10, ep, 8);
    b.U8(0x11, static_cast<uint8_t>(mux));
    QmiMsg bm;
    bool sent = client->Request(WDS_BIND_MUX_DATA_PORT, b, bm);
    Klog("data: bind mux %d %s (err %u)", mux, sent && bm.Ok() ? "ok" : "FAILED", bm.error);
    bool v6 = type == "IPV6";
    QmiMsg fm;
    client->Request(WDS_SET_IP_FAMILY, TlvWriter().U8(0x01, v6 ? 6 : 4), fm);
    TlvWriter s;
    s.Str(0x14, apn);
    if (!user.empty()) {
        s.U8(0x16, static_cast<uint8_t>(verType == 1 ? 0x01 : verType == 2 ? 0x02 : 0x03));
        s.Str(0x17, user);
        s.Str(0x18, pass);
    }
    s.U8(0x19, v6 ? 6 : 4);
    QmiMsg sm;
    sent = client->Request(WDS_START_NETWORK, s, sm, 90000);
    uint32_t handle = 0;
    if (!sent || !sm.Ok() || !sm.GetU32(0x01, handle)) {
        uint16_t why = 0;
        sm.GetU16(0x10, why);
        int16_t vtype = 0, vreason = 0;
        if (sm.Has(0x11)) {
            Reader r = sm.R(0x11);
            vtype = r.S16();
            vreason = r.S16();
        }
        Klog("data: start network '%s' FAILED err %u reason %u verbose %d/%d", apn.c_str(), sm.error, why, vtype,
            vreason);
        {
            std::lock_guard<std::mutex> lk(g_dataMu);
            ReleaseCall(slot);
        }
        /* 3GPP verbose reasons (type 6) carry the ESM/SM cause */
        RespondFail(req, cid, vtype == 6 && vreason > 0 ? vreason : HRIL_PDP_ERR_RETRY);
        return;
    }
    QmiMsg cs;
    uint32_t mask = (1 << 4) | (1 << 8) | (1 << 9) | (1 << 13) | (1 << 15);
    client->Request(WDS_GET_CURRENT_SETTINGS, TlvWriter().U32(0x10, mask), cs);
    DataCall &c = g_calls[slot];
    std::lock_guard<std::mutex> lk(g_dataMu);
    c.cid = cid;
    c.mux = mux;
    c.handle = handle;
    c.apn = apn;
    c.ifname = "rmnet_data" + std::to_string(slot);
    c.type = v6 ? "IPV6" : "IP";
    uint32_t v = 0, mskv = 0;
    if (cs.GetU32(0x1E, v)) {
        cs.GetU32(0x21, mskv);
        c.address = Ip4(v) + "/" + std::to_string(PrefixLen(mskv ? mskv : 0xFFFFFFFFu));
    }
    if (cs.GetU32(0x20, v)) c.gateway = Ip4(v);
    if (cs.GetU32(0x15, v)) c.dns = Ip4(v);
    if (cs.GetU32(0x16, v)) c.dnsSec = Ip4(v);
    if (cs.Has(0x25)) {
        Reader r = cs.R(0x25);
        std::string a = Ip6(r);
        uint8_t plen = r.U8();
        c.address = a + "/" + std::to_string(plen);
    }
    if (cs.Has(0x26)) {
        Reader r = cs.R(0x26);
        c.gateway = Ip6(r);
    }
    if (cs.Has(0x27) && c.dns.empty()) {
        Reader r = cs.R(0x27);
        c.dns = Ip6(r);
    }
    if (cs.Has(0x28) && c.dnsSec.empty()) {
        Reader r = cs.R(0x28);
        c.dnsSec = Ip6(r);
    }
    uint32_t mtu = 0;
    c.mtu = cs.GetU32(0x29, mtu) ? static_cast<int>(mtu) : 1500;
    c.active = true;
    int myslot = slot;
    c.wds = std::move(client);
    c.wds->OnIndication([myslot](const QmiMsg &m) {
        if (m.msgId != WDS_PKT_SRVC_STATUS_IND || !m.Has(0x01)) return;
        Reader r = m.R(0x01);
        uint8_t st = r.U8();
        if (st == 1) { /* disconnected */
            Ril::Get().Post([myslot] {
                {
                    std::lock_guard<std::mutex> lk(g_dataMu);
                    if (!g_calls[myslot].used) return;
                    Klog("data: %s dropped by the network", g_calls[myslot].ifname.c_str());
                    g_calls[myslot].active = false;
                }
                NotifyList();
                std::lock_guard<std::mutex> lk(g_dataMu);
                ReleaseCall(myslot);
            });
        }
    });
    Klog("data: %s up on %s addr %s gw %s dns %s mtu %d", apn.c_str(), c.ifname.c_str(), c.address.c_str(),
        c.gateway.c_str(), c.dns.c_str(), c.mtu);
    HRilDataCallResponse r;
    FillResponse(c, r);
    ril.Respond(D_DATA, req, HRIL_ERR_SUCCESS, &r, sizeof(r));
}

void ReqActivatePdpContext(const ReqDataInfo *req, const HRilDataInfo *data)
{
    if (data == nullptr) {
        Ril::Get().Respond(D_DATA, req, HRIL_ERR_INVALID_PARAMETER, nullptr, 0);
        return;
    }
    int cid = data->cid;
    std::string apn = data->apn ? data->apn : "";
    std::string type = data->type ? data->type : "IP";
    std::string user = data->userName ? data->userName : "";
    std::string pass = data->password ? data->password : "";
    int ver = data->verType;
    if (SlotOf(req) != 0) {
        RespondFail(req, cid, HRIL_PDP_ERR_RETRY);
        return;
    }
    std::thread([=] { Activate(req, cid, apn, type, user, pass, ver); }).detach();
}

void ReqActivatePdpContextWithApnTypes(const ReqDataInfo *req, const HRilDataInfoWithApnTypes *data)
{
    if (data == nullptr) {
        Ril::Get().Respond(D_DATA, req, HRIL_ERR_INVALID_PARAMETER, nullptr, 0);
        return;
    }
    HRilDataInfo d = {};
    d.cid = data->cid;
    d.reason = data->reason;
    d.rat = data->rat;
    d.roamingEnable = data->roamingEnable;
    d.verType = data->verType;
    d.userName = data->userName;
    d.password = data->password;
    d.apn = data->apn;
    d.type = data->type;
    d.roamingType = data->roamingType;
    ReqActivatePdpContext(req, &d);
}

void ReqDeactivatePdpContext(const ReqDataInfo *req, const HRilDataInfo *data)
{
    int cid = data ? data->cid : -1;
    std::thread([req, cid] {
        std::unique_ptr<QmiClient> client;
        uint32_t handle = 0;
        int slot = -1;
        {
            std::lock_guard<std::mutex> lk(g_dataMu);
            for (int i = 0; i < MAX_CALLS; i++) {
                if (g_calls[i].used && g_calls[i].cid == cid) {
                    slot = i;
                    handle = g_calls[i].handle;
                    client = std::move(g_calls[i].wds);
                    g_calls[i].active = false;
                    g_calls[i].used = false;
                    break;
                }
            }
        }
        if (client) {
            QmiMsg m;
            client->Request(WDS_STOP_NETWORK, TlvWriter().U32(0x01, handle), m, 30000);
            Klog("data: rmnet_data%d stopped", slot);
        }
        Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, nullptr, 0);
    }).detach();
}

void ReqGetPdpContextList(const ReqDataInfo *req)
{
    std::vector<HRilDataCallResponse> list;
    std::lock_guard<std::mutex> lk(g_dataMu);
    for (auto &c : g_calls) {
        if (!c.used) continue;
        HRilDataCallResponse r;
        FillResponse(c, r);
        list.push_back(r);
    }
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, list.empty() ? nullptr : list.data(),
        list.size() * sizeof(HRilDataCallResponse));
}

void ReqOkData(const ReqDataInfo *req, const HRilDataInfo *)
{
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, nullptr, 0);
}

void ReqGetLinkBandwidthInfo(const ReqDataInfo *req, int32_t cid)
{
    HRilLinkBandwidthInfo i = {};
    i.cid = cid;
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, &i, sizeof(i));
}

void ReqSetLinkBandwidthReportingRule(const ReqDataInfo *req, const HRilLinkBandwidthReportingRule *)
{
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, nullptr, 0);
}

void ReqSetDataProfileInfo(const ReqDataInfo *req, const HRilDataInfo *, int32_t)
{
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, nullptr, 0);
}

void ReqSendDataPerformanceMode(const ReqDataInfo *req, const HRilDataPerformanceInfo *)
{
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, nullptr, 0);
}

void ReqSendDataSleepMode(const ReqDataInfo *req, const HRilDataSleepInfo *)
{
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, nullptr, 0);
}

void ReqSetDataPermitted(const ReqDataInfo *req, int32_t)
{
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, nullptr, 0);
}

void ReqGetLinkCapability(const ReqDataInfo *req)
{
    HRilDataLinkCapability c = {150000, 50000, 0, 0};
    Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, &c, sizeof(c));
}

void ReqCleanAllConnections(const ReqDataInfo *req)
{
    std::thread([req] {
        for (int i = 0; i < MAX_CALLS; i++) {
            std::unique_ptr<QmiClient> client;
            uint32_t handle = 0;
            {
                std::lock_guard<std::mutex> lk(g_dataMu);
                if (!g_calls[i].used) continue;
                handle = g_calls[i].handle;
                client = std::move(g_calls[i].wds);
                g_calls[i].used = false;
                g_calls[i].active = false;
            }
            if (client) {
                QmiMsg m;
                client->Request(WDS_STOP_NETWORK, TlvWriter().U32(0x01, handle), m, 30000);
            }
        }
        Ril::Get().Respond(D_DATA, req, HRIL_ERR_SUCCESS, nullptr, 0);
    }).detach();
}
} // namespace

void DataOnUp()
{
    /* the IPA netdev shows up a few seconds after the modem */
    for (int i = 0; i < 30 && !EnsureDataFormat(); i++) {
        sleep(2);
    }
}

const HRilDataReq g_dataOps = {
    .SetInitApnInfo = ReqOkData,
    .ActivatePdpContext = ReqActivatePdpContext,
    .DeactivatePdpContext = ReqDeactivatePdpContext,
    .GetPdpContextList = ReqGetPdpContextList,
    .GetLinkBandwidthInfo = ReqGetLinkBandwidthInfo,
    .SetLinkBandwidthReportingRule = ReqSetLinkBandwidthReportingRule,
    .SetDataProfileInfo = ReqSetDataProfileInfo,
    .SendDataPerformanceMode = ReqSendDataPerformanceMode,
    .SendDataSleepMode = ReqSendDataSleepMode,
    .SetDataPermitted = ReqSetDataPermitted,
    .GetLinkCapability = ReqGetLinkCapability,
    .CleanAllConnections = ReqCleanAllConnections,
    .ActivatePdpContextWithApnTypes = ReqActivatePdpContextWithApnTypes,
};

} // namespace sweetril
