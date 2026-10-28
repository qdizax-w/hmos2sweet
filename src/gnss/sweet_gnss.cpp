/*
 * vendorGnssAdapter.so for sweet: GNSS through the modem's QMI LOC service.
 *
 * The HarmonyOS GNSS HDI service (libgnss_interface_service_2.0.z.so, the
 * OpenHarmony implementation) dlopens "vendorGnssAdapter.so" and reads the
 * GnssVendorInterface symbol. The sm6150 GNSS engine runs on the modem and
 * reports positions, satellites and NMEA as QMI LOC indications.
 *
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include <cmath>
#include <cstring>
#include <ctime>
#include <mutex>
#include <set>

#include "location_vendor_lib.h"
#include "qmi.h"

using namespace OHOS::HDI::Location;
using namespace sweetril;

namespace {

constexpr uint32_t QMI_SVC_LOC = 0x10;
enum : uint16_t {
    LOC_REGISTER_EVENTS = 0x0021,
    LOC_START = 0x0022,
    LOC_STOP = 0x0023,
    LOC_POSITION_REPORT_IND = 0x0024,
    LOC_SV_INFO_IND = 0x0025,
    LOC_NMEA_IND = 0x0026,
    LOC_ENGINE_STATE_IND = 0x002B,
    LOC_INJECT_UTC_TIME = 0x0038,
    LOC_INJECT_POSITION = 0x0039,
    LOC_SET_NMEA_TYPES = 0x003E,
    LOC_DELETE_ASSISTANCE_DATA = 0x0044,
    LOC_SET_OPERATION_MODE = 0x004A,
};
constexpr uint8_t SESSION_ID = 1;

QmiClient g_loc(QMI_SVC_LOC, "LOC");
std::mutex g_mu;
GnssCallbackStruct g_cb;
bool g_haveCb = false;
bool g_started = false;
uint32_t g_interval = 1000;
std::set<uint16_t> g_usedSvs; /* QMI numbering */

uint64_t BootNs()
{
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

bool GetF32(const QmiMsg &m, uint8_t t, float &v)
{
    const auto *b = m.Get(t);
    if (b == nullptr || b->size() < 4) return false;
    memcpy(&v, b->data(), 4);
    return true;
}

bool GetF64(const QmiMsg &m, uint8_t t, double &v)
{
    const auto *b = m.Get(t);
    if (b == nullptr || b->size() < 8) return false;
    memcpy(&v, b->data(), 8);
    return true;
}

void OnPosition(const QmiMsg &m)
{
    uint32_t status = 2;
    m.GetU32(0x01, status);
    if (m.Has(0x2C)) {
        Reader r = m.R(0x2C);
        uint8_t n = r.U8();
        std::set<uint16_t> used;
        for (uint8_t i = 0; i < n && r.Ok(); i++) used.insert(r.U16());
        std::lock_guard<std::mutex> lk(g_mu);
        g_usedSvs = used;
    }
    if (status != 0) return; /* only final fixes */
    GnssLocation loc = {};
    loc.size = sizeof(loc);
    double lat = 0, lon = 0;
    if (!GetF64(m, 0x10, lat) || !GetF64(m, 0x11, lon)) return;
    uint32_t valid = uint32_t(GnssLocationValidity::GNSS_LOCATION_LAT_VALID) |
        uint32_t(GnssLocationValidity::GNSS_LOCATION_LONG_VALID);
    loc.latitude = lat;
    loc.longitude = lon;
    float f;
    if (GetF32(m, 0x1A, f)) {
        loc.altitude = f;
        valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_ALTITUDE_VALID);
    }
    if (GetF32(m, 0x18, f)) {
        loc.speed = f;
        valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_SPEED_VALID);
    }
    if (GetF32(m, 0x20, f)) {
        loc.bearing = f;
        valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_BEARING_VALID);
    }
    if (GetF32(m, 0x12, f)) {
        loc.horizontalAccuracy = f;
        valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_HORIZONTAL_ACCURACY_VALID);
    }
    if (GetF32(m, 0x1C, f)) {
        loc.verticalAccuracy = f;
        valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_VERTICAL_ACCURACY_VALID);
    }
    if (GetF32(m, 0x19, f)) {
        loc.speedAccuracy = f;
        valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_SPEED_ACCURACY_VALID);
    }
    if (GetF32(m, 0x21, f)) {
        loc.bearingAccuracy = f;
        valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_BEARING_ACCURACY_VALID);
    }
    const auto *ts = m.Get(0x25);
    if (ts && ts->size() >= 8) {
        Reader r(*ts);
        loc.timeForFix = int64_t(r.U64());
        valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_TIME_VALID);
    } else {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        loc.timeForFix = int64_t(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
    }
    loc.timeSinceBoot = BootNs();
    valid |= uint32_t(GnssLocationValidity::GNSS_LOCATION_TIME_SINCE_BOOT_VALID);
    loc.fieldValidity = valid;
    OnLocationChange cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_haveCb) cb = g_cb.gnssCallback.locationUpdate;
    }
    if (cb) cb(&loc);
}

void OnSvInfo(const QmiMsg &m)
{
    if (!m.Has(0x10)) return;
    static GnssSatelliteStatus st;
    memset(&st, 0, sizeof(st));
    st.size = sizeof(st);
    Reader r = m.R(0x10);
    uint8_t n = r.U8();
    std::set<uint16_t> used;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        used = g_usedSvs;
    }
    for (uint8_t i = 0; i < n && r.Ok() && st.satellitesNum < SATELLITE_NUM_MAXIMUM; i++) {
        uint32_t valid = r.U32();
        uint32_t sys = r.U32();
        uint16_t id = r.U16();
        r.U8();          /* health */
        r.U32();         /* status */
        r.U8();          /* navigation data */
        float elev, az, snr;
        uint32_t e = r.U32(), a = r.U32(), s = r.U32();
        memcpy(&elev, &e, 4);
        memcpy(&az, &a, 4);
        memcpy(&snr, &s, 4);
        if (!r.Ok()) break;
        SatelliteStatusInfo &si = st.satellitesList[st.satellitesNum];
        si.size = sizeof(si);
        int16_t sid = int16_t(id);
        uint8_t c = uint8_t(ConstellationCategory::CONSTELLATION_CATEGORY_UNKNOWN);
        switch (sys) {
            case 1: c = uint8_t(ConstellationCategory::CONSTELLATION_CATEGORY_GPS); break;
            case 2: c = uint8_t(ConstellationCategory::CONSTELLATION_CATEGORY_GALILEO); sid -= 300; break;
            case 3: c = uint8_t(ConstellationCategory::CONSTELLATION_CATEGORY_SBAS); break;
            case 4: c = uint8_t(ConstellationCategory::CONSTELLATION_CATEGORY_BEIDOU); sid -= 200; break;
            case 5: c = uint8_t(ConstellationCategory::CONSTELLATION_CATEGORY_GLONASS); sid -= 64; break;
            default: break;
        }
        if (id >= 193 && id <= 200) {
            c = uint8_t(ConstellationCategory::CONSTELLATION_CATEGORY_QZSS);
        }
        si.satelliteId = sid;
        si.constellationCategory = c;
        si.cn0 = (valid & (1 << 7)) ? snr : 0.f;
        si.elevation = (valid & (1 << 5)) ? elev : 0.f;
        si.azimuth = (valid & (1 << 6)) ? az : 0.f;
        si.carrierFrequency = 0.f;
        si.satelliteAdditionalInfo = used.count(id) ?
            uint32_t(SatelliteAdditionalInfo::SATELLITES_ADDITIONAL_INFO_USED_IN_FIX) : 0;
        st.satellitesNum++;
    }
    OnSatelliteStatusChange cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_haveCb) cb = g_cb.gnssCallback.satelliteStatusUpdate;
    }
    if (cb) cb(&st);
}

void OnNmea(const QmiMsg &m)
{
    std::string s;
    if (!m.GetStr(0x01, s) || s.empty()) return;
    OnGnssNmeaChange cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_haveCb) cb = g_cb.gnssCallback.nmeaUpdate;
    }
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (cb) cb(int64_t(now.tv_sec) * 1000 + now.tv_nsec / 1000000, s.c_str(), int(s.size()));
}

void ReportStatus(GnssWorkingStatus s)
{
    OnGnssWorkingStatusChange cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_haveCb) cb = g_cb.gnssCallback.gnssWorkingStatusUpdate;
    }
    uint16_t v = uint16_t(s);
    if (cb) cb(&v);
}

void LocUp()
{
    QmiMsg m;
    uint64_t mask = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 7);
    g_loc.Request(LOC_REGISTER_EVENTS, TlvWriter().U64(0x01, mask), m);
    QmiMsg o;
    g_loc.Request(LOC_SET_OPERATION_MODE, TlvWriter().U32(0x01, 4 /* standalone */), o);
    QmiMsg n;
    g_loc.Request(LOC_SET_NMEA_TYPES, TlvWriter().U32(0x01, 0x1F | (1 << 7) | (1 << 8) | (1 << 25)), n);
    Klog("LOC up: register %s", m.Ok() ? "ok" : "FAILED");
    bool restart;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        restart = g_started;
    }
    if (restart) {
        TlvWriter w;
        w.U8(0x01, SESSION_ID).U32(0x10, 1).U32(0x12, 2).U32(0x13, g_interval);
        QmiMsg s;
        g_loc.Request(LOC_START, w, s);
    }
}

void EnsureClient()
{
    static bool once = false;
    if (once) return;
    once = true;
    g_loc.OnIndication([](const QmiMsg &m) {
        switch (m.msgId) {
            case LOC_POSITION_REPORT_IND: OnPosition(m); break;
            case LOC_SV_INFO_IND: OnSvInfo(m); break;
            case LOC_NMEA_IND: OnNmea(m); break;
            default: break;
        }
    });
    g_loc.OnUp(LocUp);
    ServiceWatcher::Get().Add(&g_loc);
    ServiceWatcher::Get().Start();
}

int EnableGnss(GnssCallbackStruct *callbacks)
{
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (callbacks) {
            g_cb = *callbacks;
            g_haveCb = true;
        }
    }
    EnsureClient();
    g_loc.WaitUp(5000);
    ReportStatus(GnssWorkingStatus::GNSS_WORKING_STATUS_POWER_ON);
    OnCapabilitiesChange cap = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_haveCb) cap = g_cb.gnssCallback.capabilitiesUpdate;
    }
    if (cap) cap(0);
    return 0;
}

int DisableGnss()
{
    ReportStatus(GnssWorkingStatus::GNSS_WORKING_STATUS_POWER_OFF);
    return 0;
}

int StartGnss(uint32_t category)
{
    (void)category;
    EnsureClient();
    if (!g_loc.WaitUp(5000)) {
        Klog("start: LOC service not available");
    }
    TlvWriter w;
    w.U8(0x01, SESSION_ID).U32(0x10, 1).U32(0x12, 2).U32(0x13, g_interval);
    QmiMsg m;
    bool sent = g_loc.Request(LOC_START, w, m);
    Klog("start (interval %u ms): %s", g_interval, sent && m.Ok() ? "ok" : "FAILED");
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_started = true;
    }
    ReportStatus(GnssWorkingStatus::GNSS_WORKING_STATUS_NAVIGATING_BEGIN);
    return sent && m.Ok() ? 0 : -1;
}

int StopGnss(uint32_t category)
{
    (void)category;
    QmiMsg m;
    g_loc.Request(LOC_STOP, TlvWriter().U8(0x01, SESSION_ID), m);
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_started = false;
    }
    Klog("stop");
    ReportStatus(GnssWorkingStatus::GNSS_WORKING_STATUS_NAVIGATING_END);
    return 0;
}

int InjectReferenceInfo(int category, GnssReferenceInfo *info)
{
    if (info == nullptr) return -1;
    if (category == int(GnssRefInfoCategory::GNSS_REF_INFO_CATEGORY_TIME)) {
        /* UTC time in ms + uncertainty */
        TlvWriter w;
        w.U64(0x01, uint64_t(info->u.time.time));
        w.U32(0x02, uint32_t(info->u.time.uncertaintyOfTime > 0 ? info->u.time.uncertaintyOfTime : 1000));
        QmiMsg m;
        g_loc.Request(LOC_INJECT_UTC_TIME, w, m);
    }
    return 0;
}

int SetGnssConfigPara(GnssConfigParameter *para)
{
    if (para && para->startCategory == uint32_t(GnssStartCategory::GNSS_START_CATEGORY_NORMAL) &&
        para->u.gnssBasicConfig.minInterval > 0) {
        g_interval = para->u.gnssBasicConfig.minInterval;
    }
    return 0;
}

void RemoveAuxiliaryData(uint16_t flags)
{
    (void)flags;
    QmiMsg m;
    g_loc.Request(LOC_DELETE_ASSISTANCE_DATA, TlvWriter().U8(0x01, 1 /* delete all */), m);
}

int InjectExtendedEphemeris(char *, int)
{
    return 0;
}

int GetCachedLocationsSize()
{
    return 0;
}

void FlushCachedGnssLocations() {}

const void *GetGnssModuleIface(int)
{
    return nullptr;
}

const GnssVendorInterface g_iface = {
    sizeof(GnssVendorInterface),
    EnableGnss,
    DisableGnss,
    StartGnss,
    StopGnss,
    InjectReferenceInfo,
    SetGnssConfigPara,
    RemoveAuxiliaryData,
    InjectExtendedEphemeris,
    GetCachedLocationsSize,
    FlushCachedGnssLocations,
    GetGnssModuleIface,
};

const GnssVendorInterface *GetGnssInterface()
{
    return &g_iface;
}
} // namespace

extern "C" __attribute__((visibility("default"))) struct GnssVendorDevice GnssVendorInterface = {
    sizeof(struct GnssVendorDevice),
    GetGnssInterface,
};
