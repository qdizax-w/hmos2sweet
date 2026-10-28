/*
 * Minimal QMI client over QRTR (AF_QIPCRTR) for the sweet RIL.
 *
 * On sm6150 the modem exports its QMI services (DMS, NAS, UIM, VOICE, WMS,
 * WDS, ...) on the QRTR bus. A client is just a datagram socket: requests go
 * to the service's (node, port), responses and indications come back on the
 * same socket. Message layout: type(u8) txn(u16) msgid(u16) len(u16) TLVs.
 *
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#ifndef SWEET_RIL_QMI_H
#define SWEET_RIL_QMI_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sweetril {

enum QmiServiceId : uint32_t {
    QMI_SVC_WDS = 0x01,
    QMI_SVC_DMS = 0x02,
    QMI_SVC_NAS = 0x03,
    QMI_SVC_WMS = 0x05,
    QMI_SVC_VOICE = 0x09,
    QMI_SVC_UIM = 0x0B,
    QMI_SVC_WDA = 0x1A,
    QMI_SVC_DPM = 0x2F,
};

/* builds the TLV part of a request */
class TlvWriter {
public:
    TlvWriter &Raw(uint8_t type, const void *data, size_t len);
    TlvWriter &U8(uint8_t type, uint8_t v) { return Raw(type, &v, 1); }
    TlvWriter &U16(uint8_t type, uint16_t v);
    TlvWriter &U32(uint8_t type, uint32_t v);
    TlvWriter &U64(uint8_t type, uint64_t v);
    TlvWriter &Str(uint8_t type, const std::string &s) { return Raw(type, s.data(), s.size()); }
    TlvWriter &Bytes(uint8_t type, const std::vector<uint8_t> &b) { return Raw(type, b.data(), b.size()); }
    const std::vector<uint8_t> &Data() const { return buf_; }

private:
    std::vector<uint8_t> buf_;
};

/* sequential little-endian reader over one TLV value */
class Reader {
public:
    Reader(const uint8_t *p, size_t n) : p_(p), n_(n) {}
    explicit Reader(const std::vector<uint8_t> &v) : p_(v.data()), n_(v.size()) {}
    bool Ok() const { return ok_; }
    size_t Left() const { return ok_ ? n_ - off_ : 0; }
    uint8_t U8();
    int8_t S8() { return static_cast<int8_t>(U8()); }
    uint16_t U16();
    int16_t S16() { return static_cast<int16_t>(U16()); }
    uint32_t U32();
    int32_t S32() { return static_cast<int32_t>(U32()); }
    uint64_t U64();
    std::vector<uint8_t> Bytes(size_t n);
    std::string Str(size_t n);
    std::string Str8() { return Str(U8()); }       /* u8 length prefix */
    std::string Str16() { return Str(U16()); }     /* u16 length prefix */
    void Skip(size_t n);

private:
    bool Need(size_t n);
    const uint8_t *p_;
    size_t n_;
    size_t off_ = 0;
    bool ok_ = true;
};

/* a parsed response or indication */
struct QmiMsg {
    uint8_t type = 0;
    uint16_t txn = 0;
    uint16_t msgId = 0;
    std::map<uint8_t, std::vector<uint8_t>> tlv;
    uint16_t result = 1;   /* 0 = success */
    uint16_t error = 0xffff;
    bool transportOk = false;

    bool Ok() const { return transportOk && result == 0; }
    bool Has(uint8_t t) const { return tlv.count(t) != 0; }
    const std::vector<uint8_t> *Get(uint8_t t) const
    {
        auto it = tlv.find(t);
        return it == tlv.end() ? nullptr : &it->second;
    }
    bool GetU8(uint8_t t, uint8_t &v) const;
    bool GetU16(uint8_t t, uint16_t &v) const;
    bool GetU32(uint8_t t, uint32_t &v) const;
    bool GetStr(uint8_t t, std::string &s) const;   /* whole TLV as a string */
    Reader R(uint8_t t) const
    {
        const auto *v = Get(t);
        static const std::vector<uint8_t> empty;
        return Reader(v ? *v : empty);
    }
};

using IndicationHandler = std::function<void(const QmiMsg &)>;

class QmiClient {
public:
    QmiClient(uint32_t service, const char *name);
    ~QmiClient();
    const char *Name() const { return name_; }
    uint32_t Service() const { return service_; }

    /* called by the service watcher */
    void Attach(uint32_t node, uint32_t port);
    void Detach();
    bool Endpoint(uint32_t &node, uint32_t &port)
    {
        std::lock_guard<std::mutex> lk(mu_);
        node = node_;
        port = port_;
        return up_;
    }
    bool Up() const { return up_.load(); }
    /* waits until the service shows up on the bus */
    bool WaitUp(int timeoutMs);

    bool Request(uint16_t msgId, const TlvWriter &in, QmiMsg &out, int timeoutMs = 15000);
    bool Request(uint16_t msgId, QmiMsg &out, int timeoutMs = 15000)
    {
        TlvWriter empty;
        return Request(msgId, empty, out, timeoutMs);
    }
    void OnIndication(IndicationHandler h);
    void OnUp(std::function<void()> h);

private:
    void ReaderLoop();
    struct Pending {
        bool done = false;
        QmiMsg msg;
    };
    uint32_t service_;
    const char *name_;
    int fd_ = -1;
    std::atomic<bool> up_{false};
    std::atomic<bool> stop_{false};
    uint32_t node_ = 0;
    uint32_t port_ = 0;
    uint16_t txn_ = 0;
    std::mutex mu_;
    std::condition_variable cv_;
    std::map<uint16_t, std::shared_ptr<Pending>> pending_;
    std::vector<IndicationHandler> indHandlers_;
    std::vector<std::function<void()>> upHandlers_;
    std::thread reader_;
};

/* watches the QRTR name service for the services we use */
class ServiceWatcher {
public:
    static ServiceWatcher &Get();
    void Add(QmiClient *c);
    void Start();

private:
    void Loop();
    std::mutex mu_;
    std::vector<QmiClient *> clients_;
    std::thread th_;
    bool started_ = false;
};

void Log(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Klog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define RLOGD(...) ::sweetril::Log(3, __VA_ARGS__)
#define RLOGI(...) ::sweetril::Log(4, __VA_ARGS__)
#define RLOGW(...) ::sweetril::Log(5, __VA_ARGS__)
#define RLOGE(...) ::sweetril::Log(6, __VA_ARGS__)

std::string HexEncode(const uint8_t *p, size_t n);
std::string HexEncode(const std::vector<uint8_t> &v);
std::vector<uint8_t> HexDecode(const char *s);

} // namespace sweetril
#endif
