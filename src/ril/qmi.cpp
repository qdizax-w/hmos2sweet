/*
 * Minimal QMI client over QRTR for the sweet RIL. See qmi.h.
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include "qmi.h"

#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef AF_QIPCRTR
#define AF_QIPCRTR 42
#endif

#ifndef QMI_LOG_TAG
#define QMI_LOG_TAG "SweetRil"
#endif
#ifndef QMI_KLOG_PREFIX
#define QMI_KLOG_PREFIX "sweet-ril: "
#endif

extern "C" int HiLogPrint(int type, int level, unsigned int domain, const char *tag, const char *fmt, ...);

namespace sweetril {

namespace {
struct SockAddrQrtr {
    unsigned short family;
    uint32_t node;
    uint32_t port;
};
constexpr uint32_t QRTR_TYPE_NEW_SERVER = 4;
constexpr uint32_t QRTR_TYPE_DEL_SERVER = 5;
constexpr uint32_t QRTR_TYPE_NEW_LOOKUP = 10;
constexpr uint32_t QRTR_PORT_CTRL = 0xfffffffeu;
constexpr uint8_t QMI_TYPE_REQUEST = 0x00;
constexpr uint8_t QMI_TYPE_RESPONSE = 0x02;
constexpr uint8_t QMI_TYPE_INDICATION = 0x04;
} // namespace

/* ------------------------------------------------------------------ log */
void Log(int level, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    HiLogPrint(3 /* LOG_CORE */, level, 0xD001F06, QMI_LOG_TAG, "%{public}s", buf);
}

void Klog(const char *fmt, ...)
{
    char buf[300];
    int n = snprintf(buf, sizeof(buf), "<14>" QMI_KLOG_PREFIX);
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n - 2, fmt, ap);
    va_end(ap);
    if (n > static_cast<int>(sizeof(buf)) - 2) {
        n = sizeof(buf) - 2;
    }
    buf[n++] = '\n';
    int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        (void)write(fd, buf, n);
        close(fd);
    }
    buf[n - 1] = 0;
    HiLogPrint(3, 4, 0xD001F06, QMI_LOG_TAG, "%{public}s", buf + 4);
}

std::string HexEncode(const uint8_t *p, size_t n)
{
    static const char *d = "0123456789ABCDEF";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) {
        s.push_back(d[p[i] >> 4]);
        s.push_back(d[p[i] & 15]);
    }
    return s;
}

std::string HexEncode(const std::vector<uint8_t> &v)
{
    return HexEncode(v.data(), v.size());
}

static int HexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::vector<uint8_t> HexDecode(const char *s)
{
    std::vector<uint8_t> out;
    if (s == nullptr) {
        return out;
    }
    size_t n = strlen(s);
    for (size_t i = 0; i + 1 < n; i += 2) {
        int a = HexVal(s[i]);
        int b = HexVal(s[i + 1]);
        if (a < 0 || b < 0) {
            break;
        }
        out.push_back(static_cast<uint8_t>(a << 4 | b));
    }
    return out;
}

/* ------------------------------------------------------------ TlvWriter */
TlvWriter &TlvWriter::Raw(uint8_t type, const void *data, size_t len)
{
    buf_.push_back(type);
    buf_.push_back(static_cast<uint8_t>(len));
    buf_.push_back(static_cast<uint8_t>(len >> 8));
    const uint8_t *b = static_cast<const uint8_t *>(data);
    buf_.insert(buf_.end(), b, b + len);
    return *this;
}

TlvWriter &TlvWriter::U16(uint8_t type, uint16_t v)
{
    uint8_t b[2] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)};
    return Raw(type, b, 2);
}

TlvWriter &TlvWriter::U32(uint8_t type, uint32_t v)
{
    uint8_t b[4];
    for (int i = 0; i < 4; i++) {
        b[i] = static_cast<uint8_t>(v >> (8 * i));
    }
    return Raw(type, b, 4);
}

TlvWriter &TlvWriter::U64(uint8_t type, uint64_t v)
{
    uint8_t b[8];
    for (int i = 0; i < 8; i++) {
        b[i] = static_cast<uint8_t>(v >> (8 * i));
    }
    return Raw(type, b, 8);
}

/* --------------------------------------------------------------- Reader */
bool Reader::Need(size_t n)
{
    if (!ok_ || off_ + n > n_) {
        ok_ = false;
        return false;
    }
    return true;
}

uint8_t Reader::U8()
{
    if (!Need(1)) return 0;
    return p_[off_++];
}

uint16_t Reader::U16()
{
    if (!Need(2)) return 0;
    uint16_t v = static_cast<uint16_t>(p_[off_] | p_[off_ + 1] << 8);
    off_ += 2;
    return v;
}

uint32_t Reader::U32()
{
    if (!Need(4)) return 0;
    uint32_t v = 0;
    for (int i = 3; i >= 0; i--) {
        v = v << 8 | p_[off_ + i];
    }
    off_ += 4;
    return v;
}

uint64_t Reader::U64()
{
    uint64_t lo = U32();
    uint64_t hi = U32();
    return lo | hi << 32;
}

std::vector<uint8_t> Reader::Bytes(size_t n)
{
    if (!Need(n)) return {};
    std::vector<uint8_t> v(p_ + off_, p_ + off_ + n);
    off_ += n;
    return v;
}

std::string Reader::Str(size_t n)
{
    if (!Need(n)) return {};
    std::string s(reinterpret_cast<const char *>(p_ + off_), n);
    off_ += n;
    /* fixed-size fields may be NUL padded */
    size_t z = s.find('\0');
    if (z != std::string::npos) {
        s.resize(z);
    }
    return s;
}

void Reader::Skip(size_t n)
{
    if (Need(n)) off_ += n;
}

/* --------------------------------------------------------------- QmiMsg */
bool QmiMsg::GetU8(uint8_t t, uint8_t &v) const
{
    const auto *b = Get(t);
    if (b == nullptr || b->size() < 1) return false;
    v = (*b)[0];
    return true;
}

bool QmiMsg::GetU16(uint8_t t, uint16_t &v) const
{
    const auto *b = Get(t);
    if (b == nullptr || b->size() < 2) return false;
    v = static_cast<uint16_t>((*b)[0] | (*b)[1] << 8);
    return true;
}

bool QmiMsg::GetU32(uint8_t t, uint32_t &v) const
{
    const auto *b = Get(t);
    if (b == nullptr || b->size() < 4) return false;
    Reader r(*b);
    v = r.U32();
    return true;
}

bool QmiMsg::GetStr(uint8_t t, std::string &s) const
{
    const auto *b = Get(t);
    if (b == nullptr) return false;
    s.assign(b->begin(), b->end());
    size_t z = s.find('\0');
    if (z != std::string::npos) s.resize(z);
    return true;
}

static bool ParseMsg(const uint8_t *buf, size_t n, QmiMsg &m)
{
    if (n < 7) return false;
    m.type = buf[0];
    m.txn = static_cast<uint16_t>(buf[1] | buf[2] << 8);
    m.msgId = static_cast<uint16_t>(buf[3] | buf[4] << 8);
    size_t len = static_cast<size_t>(buf[5] | buf[6] << 8);
    if (len + 7 > n) len = n - 7;
    size_t off = 7;
    while (off + 3 <= 7 + len) {
        uint8_t t = buf[off];
        size_t l = static_cast<size_t>(buf[off + 1] | buf[off + 2] << 8);
        off += 3;
        if (off + l > 7 + len) break;
        m.tlv[t].assign(buf + off, buf + off + l);
        off += l;
    }
    uint16_t r = 0, e = 0;
    const auto *res = m.Get(0x02);
    if (res && res->size() >= 4) {
        r = static_cast<uint16_t>((*res)[0] | (*res)[1] << 8);
        e = static_cast<uint16_t>((*res)[2] | (*res)[3] << 8);
        m.result = r;
        m.error = e;
    } else if (m.type == QMI_TYPE_INDICATION) {
        m.result = 0;
        m.error = 0;
    }
    m.transportOk = true;
    return true;
}

/* ------------------------------------------------------------ QmiClient */
QmiClient::QmiClient(uint32_t service, const char *name) : service_(service), name_(name) {}

QmiClient::~QmiClient()
{
    stop_ = true;
    if (reader_.joinable()) {
        reader_.join();
    }
    if (fd_ >= 0) {
        close(fd_);
    }
}

void QmiClient::Attach(uint32_t node, uint32_t port)
{
    std::vector<std::function<void()>> handlers;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (fd_ < 0) {
            fd_ = socket(AF_QIPCRTR, SOCK_DGRAM | SOCK_CLOEXEC, 0);
            if (fd_ < 0) {
                RLOGE("%s: socket(AF_QIPCRTR) failed: %s", name_, strerror(errno));
                Klog("%s: socket(AF_QIPCRTR) failed errno %d", name_, errno);
                return;
            }
            struct timeval tv = {0, 500000};
            setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            reader_ = std::thread([this] { ReaderLoop(); });
        }
        node_ = node;
        port_ = port;
        up_ = true;
        handlers = upHandlers_;
    }
    Klog("QMI %s up at %u:%u", name_, node, port);
    cv_.notify_all();
    if (!handlers.empty()) {
        std::thread([handlers] {
            for (auto &h : handlers) {
                h();
            }
        }).detach();
    }
}

void QmiClient::Detach()
{
    std::lock_guard<std::mutex> lk(mu_);
    if (!up_) return;
    up_ = false;
    for (auto &p : pending_) {
        p.second->done = true;
        p.second->msg.transportOk = false;
    }
    pending_.clear();
    cv_.notify_all();
    Klog("QMI %s gone", name_);
}

bool QmiClient::WaitUp(int timeoutMs)
{
    std::unique_lock<std::mutex> lk(mu_);
    return cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this] { return up_.load(); });
}

void QmiClient::OnIndication(IndicationHandler h)
{
    std::lock_guard<std::mutex> lk(mu_);
    indHandlers_.push_back(std::move(h));
}

void QmiClient::OnUp(std::function<void()> h)
{
    bool fire = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        upHandlers_.push_back(h);
        fire = up_;
    }
    if (fire) {
        std::thread(h).detach();
    }
}

bool QmiClient::Request(uint16_t msgId, const TlvWriter &in, QmiMsg &out, int timeoutMs)
{
    out = QmiMsg();
    auto p = std::make_shared<Pending>();
    std::vector<uint8_t> m;
    SockAddrQrtr to = {AF_QIPCRTR, 0, 0};
    uint16_t txn;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!up_ || fd_ < 0) {
            return false;
        }
        txn = ++txn_;
        if (txn == 0) txn = ++txn_;
        const auto &d = in.Data();
        m.resize(7);
        m[0] = QMI_TYPE_REQUEST;
        m[1] = static_cast<uint8_t>(txn);
        m[2] = static_cast<uint8_t>(txn >> 8);
        m[3] = static_cast<uint8_t>(msgId);
        m[4] = static_cast<uint8_t>(msgId >> 8);
        m[5] = static_cast<uint8_t>(d.size());
        m[6] = static_cast<uint8_t>(d.size() >> 8);
        m.insert(m.end(), d.begin(), d.end());
        to.node = node_;
        to.port = port_;
        pending_[txn] = p;
    }
    ssize_t s = sendto(fd_, m.data(), m.size(), 0, reinterpret_cast<sockaddr *>(&to), sizeof(to));
    std::unique_lock<std::mutex> lk(mu_);
    if (s != static_cast<ssize_t>(m.size())) {
        pending_.erase(txn);
        RLOGE("%s: send 0x%04x failed: %s", name_, msgId, strerror(errno));
        return false;
    }
    bool got = cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [&p] { return p->done; });
    pending_.erase(txn);
    if (!got) {
        RLOGE("%s: request 0x%04x timed out", name_, msgId);
        return false;
    }
    out = p->msg;
    if (!out.transportOk) {
        return false;
    }
    if (out.result != 0) {
        RLOGW("%s: request 0x%04x error %u", name_, msgId, out.error);
    }
    return true;
}

void QmiClient::ReaderLoop()
{
    std::vector<uint8_t> buf(65536);
    while (!stop_) {
        SockAddrQrtr from = {};
        socklen_t sl = sizeof(from);
        ssize_t n = recvfrom(fd_, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr *>(&from), &sl);
        if (n <= 0) {
            /* timeouts come back as EAGAIN; any other error (e.g. ENETRESET
             * while the modem restarts) would otherwise spin this thread */
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                static int reported;
                if (reported++ < 5) {
                    Klog("%s: recv failed errno %d", name_, errno);
                }
                usleep(200000);
            }
            continue;
        }
        QmiMsg msg;
        if (!ParseMsg(buf.data(), static_cast<size_t>(n), msg)) {
            continue;
        }
        if (msg.type == QMI_TYPE_RESPONSE) {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = pending_.find(msg.txn);
            if (it != pending_.end()) {
                it->second->msg = std::move(msg);
                it->second->done = true;
                cv_.notify_all();
            }
        } else if (msg.type == QMI_TYPE_INDICATION) {
            std::vector<IndicationHandler> hs;
            {
                std::lock_guard<std::mutex> lk(mu_);
                hs = indHandlers_;
            }
            for (auto &h : hs) {
                h(msg);
            }
        }
    }
}

/* ------------------------------------------------------- ServiceWatcher */
ServiceWatcher &ServiceWatcher::Get()
{
    static ServiceWatcher w;
    return w;
}

void ServiceWatcher::Add(QmiClient *c)
{
    std::lock_guard<std::mutex> lk(mu_);
    clients_.push_back(c);
}

void ServiceWatcher::Start()
{
    std::lock_guard<std::mutex> lk(mu_);
    if (started_) return;
    started_ = true;
    th_ = std::thread([this] { Loop(); });
    th_.detach();
}

void ServiceWatcher::Loop()
{
    for (;;) {
        int fd = socket(AF_QIPCRTR, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            Klog("watcher: socket(AF_QIPCRTR) failed errno %d, retrying", errno);
            sleep(2);
            continue;
        }
        SockAddrQrtr me = {};
        socklen_t sl = sizeof(me);
        /* force an address, then talk to the local name service */
        uint32_t dummy[5] = {QRTR_TYPE_NEW_LOOKUP, 0, 0, 0, 0};
        (void)dummy;
        std::vector<uint32_t> svcs;
        {
            std::lock_guard<std::mutex> lk(mu_);
            for (auto *c : clients_) {
                bool seen = false;
                for (auto s : svcs) seen |= (s == c->Service());
                if (!seen) svcs.push_back(c->Service());
            }
        }
        bool ok = true;
        for (auto s : svcs) {
            uint32_t pkt[5] = {QRTR_TYPE_NEW_LOOKUP, s, 0, 0, 0};
            if (getsockname(fd, reinterpret_cast<sockaddr *>(&me), &sl) < 0) {
                ok = false;
                break;
            }
            SockAddrQrtr ns = {AF_QIPCRTR, me.node, QRTR_PORT_CTRL};
            if (sendto(fd, pkt, sizeof(pkt), 0, reinterpret_cast<sockaddr *>(&ns), sizeof(ns)) < 0) {
                Klog("watcher: lookup 0x%x failed errno %d", s, errno);
                ok = false;
                break;
            }
        }
        if (!ok) {
            close(fd);
            sleep(2);
            continue;
        }
        for (;;) {
            uint32_t r[8] = {};
            ssize_t n = recv(fd, r, sizeof(r), 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (n == 0) break;
            if (n < 20) continue;
            uint32_t cmd = r[0], svc = r[1], node = r[3], port = r[4];
            if (svc == 0) continue; /* end of the initial listing */
            std::vector<QmiClient *> cs;
            {
                std::lock_guard<std::mutex> lk(mu_);
                cs = clients_;
            }
            for (auto *c : cs) {
                if (c->Service() != svc) continue;
                if (cmd == QRTR_TYPE_NEW_SERVER && !c->Up()) {
                    c->Attach(node, port);
                } else if (cmd == QRTR_TYPE_DEL_SERVER) {
                    c->Detach();
                }
            }
        }
        close(fd);
        sleep(1);
    }
}

} // namespace sweetril
