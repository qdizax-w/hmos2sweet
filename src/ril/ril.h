/*
 * sweet RIL: HarmonyOS/OpenHarmony ril_adapter vendor library on top of the
 * Qualcomm modem's QMI services (Redmi Note 10 Pro, sm6150).
 *
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#ifndef SWEET_RIL_H
#define SWEET_RIL_H

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "hril.h"
#include "hril_notification.h"
#include "qmi.h"

namespace sweetril {

/* ---- QMI message ids used here (see libqmi data/qmi-service-*.json) */
enum : uint16_t {
    DMS_GET_REVISION = 0x0023,
    DMS_GET_IDS = 0x0025,
    DMS_GET_OPERATING_MODE = 0x002D,
    DMS_SET_OPERATING_MODE = 0x002E,
    DMS_EVENT_REPORT = 0x0001,

    NAS_REGISTER_INDICATIONS = 0x0003,
    NAS_NETWORK_SCAN = 0x0021,
    NAS_INITIATE_NETWORK_REGISTER = 0x0022,
    NAS_GET_SERVING_SYSTEM = 0x0024,
    NAS_SET_SYSTEM_SELECTION_PREFERENCE = 0x0033,
    NAS_GET_SYSTEM_SELECTION_PREFERENCE = 0x0034,
    NAS_GET_OPERATOR_NAME = 0x0039,
    NAS_GET_CELL_LOCATION_INFO = 0x0043,
    NAS_GET_PLMN_NAME = 0x0044,
    NAS_NETWORK_TIME_IND = 0x004C,
    NAS_GET_SYSTEM_INFO = 0x004D,
    NAS_SYSTEM_INFO_IND = 0x004E,
    NAS_GET_SIGNAL_INFO = 0x004F,
    NAS_CONFIG_SIGNAL_INFO = 0x0050,
    NAS_SIGNAL_INFO_IND = 0x0051,

    UIM_READ_TRANSPARENT = 0x0020,
    UIM_READ_RECORD = 0x0021,
    UIM_WRITE_TRANSPARENT = 0x0022,
    UIM_WRITE_RECORD = 0x0023,
    UIM_GET_FILE_ATTRIBUTES = 0x0024,
    UIM_SET_PIN_PROTECTION = 0x0025,
    UIM_VERIFY_PIN = 0x0026,
    UIM_UNBLOCK_PIN = 0x0027,
    UIM_CHANGE_PIN = 0x0028,
    UIM_REGISTER_EVENTS = 0x002E,
    UIM_GET_CARD_STATUS = 0x002F,
    UIM_CARD_STATUS_IND = 0x0032,

    VOICE_INDICATION_REGISTER = 0x0003,
    VOICE_DIAL_CALL = 0x0020,
    VOICE_END_CALL = 0x0021,
    VOICE_ANSWER_CALL = 0x0022,
    VOICE_BURST_DTMF = 0x0028,
    VOICE_START_CONT_DTMF = 0x0029,
    VOICE_STOP_CONT_DTMF = 0x002A,
    VOICE_ALL_CALL_STATUS_IND = 0x002E,
    VOICE_GET_ALL_CALL_INFO = 0x002F,
    VOICE_MANAGE_CALLS = 0x0031,

    WMS_SET_EVENT_REPORT = 0x0001,
    WMS_EVENT_REPORT_IND = 0x0001,
    WMS_RAW_SEND = 0x0020,
    WMS_RAW_WRITE = 0x0021,
    WMS_RAW_READ = 0x0022,
    WMS_DELETE = 0x0024,
    WMS_SET_ROUTES = 0x0032,
    WMS_GET_SMSC_ADDRESS = 0x0034,
    WMS_SET_SMSC_ADDRESS = 0x0035,
    WMS_SEND_ACK = 0x0037,
    WMS_SET_MEMORY_STATUS = 0x0041,
};

struct CardInfo {
    bool valid = false;
    uint8_t cardState = 0;       /* QmiUimCardState: 0 absent, 1 present, 2 error */
    uint8_t appType = 0;         /* QmiUimCardApplicationType */
    uint8_t appState = 0;        /* QmiUimCardApplicationState */
    uint8_t persoState = 0;
    uint8_t pin1State = 0;       /* QmiUimPinState */
    uint8_t pin1Retries = 0;
    uint8_t puk1Retries = 0;
    uint8_t pin2State = 0;
    uint8_t pin2Retries = 0;
    uint8_t puk2Retries = 0;
    bool upinReplacesPin1 = false;
    std::vector<uint8_t> aid;
    std::string iccid;
};

struct ServingInfo {
    bool valid = false;
    uint8_t regState = 0;        /* QmiNasRegistrationState */
    uint8_t csAttach = 0;        /* QmiNasAttachState */
    uint8_t psAttach = 0;
    std::vector<int8_t> radioIfs;
    std::vector<uint8_t> dataCaps;
    bool roaming = false;
    uint16_t mcc = 0;
    uint16_t mnc = 0;
    bool mnc3 = false;
    std::string description;
    uint16_t lac = 0;
    uint32_t cid = 0;
    uint16_t tac = 0;
    bool hasTac = false;
};

struct SignalInfo {
    bool hasGsm = false, hasWcdma = false, hasLte = false, hasNr = false, hasTd = false;
    int8_t gsmRssi = 0;
    int8_t wcdmaRssi = 0;
    int16_t wcdmaEcio = 0;
    int16_t wcdmaRscp = 0;
    bool hasWcdmaRscp = false;
    int8_t lteRssi = 0;
    int8_t lteRsrq = 0;
    int16_t lteRsrp = 0;
    int16_t lteSnr = 0;
    int16_t nrRsrp = 0;
    int16_t nrSnr = 0;
    int16_t nrRsrq = 0;
    int8_t tdRscp = 0;
};

class Ril {
public:
    static Ril &Get();
    void Init(const struct HRilReport *ops);

    /* clients of the modem's QMI services (subscription 1) */
    QmiClient dms{QMI_SVC_DMS, "DMS"};
    QmiClient nas{QMI_SVC_NAS, "NAS"};
    QmiClient uim{QMI_SVC_UIM, "UIM"};
    QmiClient voice{QMI_SVC_VOICE, "VOICE"};
    QmiClient wms{QMI_SVC_WMS, "WMS"};
    QmiClient wds{QMI_SVC_WDS, "WDS"};
    QmiClient wda{QMI_SVC_WDA, "WDA"};

    /* reporting to hril */
    void Respond(int domain, const ReqDataInfo *req, int32_t err, const void *data, size_t len);
    void Notify(int domain, int32_t slot, int32_t notifyId, const void *data, size_t len);
    /* runs fn on the request worker thread (QMI round trips must not block hril) */
    void Post(std::function<void()> fn);

    std::mutex stateMu;
    int radioState = HRIL_RADIO_POWER_STATE_UNAVAILABLE;
    CardInfo cards[2];
    ServingInfo serving;
    SignalInfo signal;
    int lastCallFailCause = 16;
    bool muted = false;
    int32_t clirMode = 0;   /* +CLIR: 0 default, 1 hide, 2 show */

private:
    void Worker();
    const struct HRilReport *report_ = nullptr;
    std::mutex qMu_;
    std::condition_variable qCv_;
    std::vector<std::function<void()>> queue_;
};

enum Domain { D_CALL, D_DATA, D_MODEM, D_NETWORK, D_SIM, D_SMS };

/* ops tables, one file each */
extern HRilCallReq g_callOps;
void InstallSsOps(HRilCallReq &ops);
void SsOnUp();
void ImsInit();
bool ImsVoiceReady();
extern const HRilSimReq g_simOps;
extern const HRilSmsReq g_smsOps;
extern const HRilDataReq g_dataOps;
extern const HRilNetworkReq g_networkOps;
extern const HRilModemReq g_modemOps;

/* service bring-up hooks, called when each QMI service appears */
void ModemOnUp();
void SimOnUp();
void NetworkOnUp();
void CallOnUp();
void SmsOnUp();
void DataOnUp();

/* helpers shared between files */
void SimRefreshCardStatus(bool notify);
void NetworkRefresh(bool notify);
int SlotOf(const ReqDataInfo *req);
int MapRadioTech(const ServingInfo &s);
void FillVoiceRadioInfo(HRilVoiceRadioInfo &info);
std::string PlmnNumeric(const ServingInfo &s);

/* QMI result -> hril error */
inline int32_t QmiErr(bool sent, const QmiMsg &m)
{
    if (!sent) return HRIL_ERR_CMD_SEND_FAILURE;
    return m.result == 0 ? HRIL_ERR_SUCCESS : HRIL_ERR_GENERIC_FAILURE;
}

} // namespace sweetril
#endif
