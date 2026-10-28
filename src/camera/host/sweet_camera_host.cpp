/*
 * sweet_camera_host: OpenHarmony camera HDI (ohos.hdi.camera.v1_0) for the
 * Redmi Note 10 Pro, on top of sweet's Android camera HAL.
 *
 * The Android HAL (Qualcomm CamX) runs in cambridge, inside an Android mount
 * namespace (see ../bridge and ../launcher). This process:
 *   - connects to cambridge (abstract unix socket "sweetcam") and gets the
 *     description of every camera;
 *   - registers "camera_service" (device class camera) with the HDF service
 *     manager, the way Huawei's CameraDaemon did, so that the system camera
 *     service finds it;
 *   - implements ICameraHost / ICameraDevice / IStreamOperator: abilities are
 *     built from the Android characteristics, controls are mapped to Android
 *     request settings, and every frame produced by the HAL is copied into the
 *     BufferProducer of the OpenHarmony stream (NV21/NV12/RGBA/JPEG).
 *
 * License: Apache-2.0
 */
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <map>
#include <memory>
#include <mutex>
#include <poll.h>
#include <set>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <hilog/log.h>
#include <ipc_skeleton.h>
#include <iremote_object.h>
#include <message_parcel.h>
#include <message_option.h>
#include <object_collector.h>
#include <iservmgr_hdi.h>
#include <hdi_support.h>
#include <iproxy_broker.h>

#include "camera/v1_0/icamera_host.h"
#include "camera/v1_0/icamera_device.h"
#include "camera/v1_0/istream_operator.h"
#include "camera/v1_0/icamera_host_callback.h"
#include "camera/v1_0/icamera_device_callback.h"
#include "camera/v1_0/istream_operator_callback.h"
#include "camera_device_ability_items.h"
#include "camera_metadata_info.h"
#include "metadata_utils.h"

#include "surface_min.h"
#include "../sweetcam_proto.h"

using namespace OHOS;
using namespace OHOS::HDI::Camera::V1_0;
using OHOS::Camera::CameraMetadata;

namespace sweetcam {

static constexpr int32_t CAM_OK = OHOS::HDI::Camera::V1_0::CamRetCode::NO_ERROR;

/* ------------------------------------------------------------------ log */
static int g_kmsg = -1;

static void Log(bool kmsg, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void Log(bool kmsg, const char *fmt, ...)
{
    char buf[768];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    HiLogPrint(LOG_CORE, LOG_INFO, 0xD002510, "SweetCam", "%{public}s", buf);
    if (kmsg) {
        if (g_kmsg < 0) {
            g_kmsg = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
        }
        if (g_kmsg >= 0) {
            char k[800];
            int n = snprintf(k, sizeof(k), "<6>sweet-camera: %s\n", buf);
            (void)write(g_kmsg, k, n);
        }
    }
}
#define LOGI(...) Log(false, __VA_ARGS__)
#define LOGK(...) Log(true, __VA_ARGS__)

/* --------------------------------------------------------------- bridge */
class FrameSink {
public:
    virtual ~FrameSink() = default;
    virtual void OnFrame(const sc_frame &f) = 0;
    virtual void OnShutter(const sc_shutter &s) = 0;
    virtual void OnResult(const sc_result &r) = 0;
    virtual void OnError(const sc_error &e) = 0;
    virtual void OnCaptureDone(const sc_capture_done &d) = 0;
    virtual void OnDisconnect() = 0;
};

struct BufferSet {
    sc_buffers b;
    std::vector<int> fds;
};

class Bridge {
public:
    static Bridge &Get()
    {
        static Bridge b;
        return b;
    }

    bool Connect(int timeoutSec)
    {
        for (int i = 0; i < timeoutSec * 2; i++) {
            if (TryConnect()) {
                return true;
            }
            usleep(500000);
        }
        return false;
    }

    bool Hello(int timeoutMs)
    {
        {
            std::lock_guard<std::mutex> l(lock_);
            infoOk_ = false;
            cams_.clear();
        }
        sc_hdr h = { SC_HELLO, sizeof(sc_hdr) };
        if (!Send(&h, sizeof(h))) {
            return false;
        }
        std::unique_lock<std::mutex> l(lock_);
        return cv_.wait_for(l, std::chrono::milliseconds(timeoutMs), [this] {
            return infoOk_ && (int)cams_.size() >= info_.n_cams;
        });
    }

    bool Send(const void *msg, size_t len)
    {
        std::lock_guard<std::mutex> l(sendLock_);
        if (fd_ < 0) {
            return false;
        }
        return send(fd_, msg, len, MSG_NOSIGNAL) == (ssize_t)len;
    }

    /* request -> SC_REPLY */
    int Call(const void *msg, size_t len, int forType, int timeoutMs)
    {
        std::unique_lock<std::mutex> l(callLock_);
        {
            std::lock_guard<std::mutex> g(lock_);
            replies_.clear();
        }
        if (!Send(msg, len)) {
            return -EPIPE;
        }
        std::unique_lock<std::mutex> g(lock_);
        bool ok = cv_.wait_for(g, std::chrono::milliseconds(timeoutMs), [&] {
            for (auto &r : replies_) {
                if (r.for_type == forType) {
                    return true;
                }
            }
            return fd_ < 0;
        });
        if (!ok || fd_ < 0) {
            return -ETIMEDOUT;
        }
        for (auto &r : replies_) {
            if (r.for_type == forType) {
                return r.status;
            }
        }
        return -EIO;
    }

    bool TakeBuffers(int n, std::vector<BufferSet> &out, int timeoutMs)
    {
        std::unique_lock<std::mutex> l(lock_);
        bool ok = cv_.wait_for(l, std::chrono::milliseconds(timeoutMs), [&] { return (int)bufsets_.size() >= n; });
        if (!ok) {
            return false;
        }
        for (int i = 0; i < n; i++) {
            out.push_back(std::move(bufsets_.front()));
            bufsets_.pop_front();
        }
        return true;
    }

    void SetSink(FrameSink *s)
    {
        std::lock_guard<std::mutex> l(sinkLock_);
        sink_ = s;
    }

    const std::vector<sc_caminfo> &Cams() const { return cams_; }
    const sc_info &Info() const { return info_; }
    bool Connected() const { return fd_ >= 0; }

private:
    bool TryConnect()
    {
        int s = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
        if (s < 0) {
            return false;
        }
        sockaddr_un a = {};
        a.sun_family = AF_UNIX;
        memcpy(a.sun_path + 1, SC_SOCKET_NAME, strlen(SC_SOCKET_NAME));
        socklen_t al = offsetof(sockaddr_un, sun_path) + 1 + strlen(SC_SOCKET_NAME);
        if (connect(s, reinterpret_cast<sockaddr *>(&a), al) != 0) {
            close(s);
            return false;
        }
        int sz = 1 << 20;
        setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
        {
            std::lock_guard<std::mutex> l(sendLock_);
            fd_ = s;
        }
        if (reader_.joinable()) {
            reader_.join();
        }
        reader_ = std::thread([this, s] { ReadLoop(s); });
        LOGK("connected to cambridge");
        return true;
    }

    void ReadLoop(int s)
    {
        static thread_local std::vector<char> buf(65536);
        for (;;) {
            iovec iov = { buf.data(), buf.size() };
            char cbuf[CMSG_SPACE(sizeof(int) * SC_MAX_BUFS)];
            msghdr mh = {};
            mh.msg_iov = &iov;
            mh.msg_iovlen = 1;
            mh.msg_control = cbuf;
            mh.msg_controllen = sizeof(cbuf);
            ssize_t n = recvmsg(s, &mh, MSG_CMSG_CLOEXEC);
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n <= 0) {
                break;
            }
            std::vector<int> fds;
            for (cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) {
                if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
                    int cnt = (c->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                    int *p = reinterpret_cast<int *>(CMSG_DATA(c));
                    fds.insert(fds.end(), p, p + cnt);
                }
            }
            if ((size_t)n < sizeof(sc_hdr)) {
                continue;
            }
            Dispatch(buf.data(), (size_t)n, fds);
        }
        LOGK("cambridge connection lost");
        {
            std::lock_guard<std::mutex> l(sendLock_);
            close(fd_);
            fd_ = -1;
        }
        cv_.notify_all();
        std::lock_guard<std::mutex> l(sinkLock_);
        if (sink_) {
            sink_->OnDisconnect();
        }
    }

    template <typename T> static bool As(const char *p, size_t n, T &out)
    {
        if (n < sizeof(T)) {
            return false;
        }
        memcpy(&out, p, sizeof(T));
        return true;
    }

    void Dispatch(const char *p, size_t n, std::vector<int> &fds)
    {
        const sc_hdr *h = reinterpret_cast<const sc_hdr *>(p);
        switch (h->type) {
            case SC_INFO: {
                std::lock_guard<std::mutex> l(lock_);
                As(p, n, info_);
                infoOk_ = true;
                break;
            }
            case SC_CAMINFO: {
                sc_caminfo ci;
                if (As(p, n, ci)) {
                    std::lock_guard<std::mutex> l(lock_);
                    cams_.push_back(ci);
                }
                break;
            }
            case SC_REPLY: {
                sc_reply r;
                if (As(p, n, r)) {
                    std::lock_guard<std::mutex> l(lock_);
                    replies_.push_back(r);
                }
                break;
            }
            case SC_BUFFERS: {
                BufferSet bs;
                if (As(p, n, bs.b)) {
                    bs.fds = fds;
                    fds.clear();
                    std::lock_guard<std::mutex> l(lock_);
                    bufsets_.push_back(std::move(bs));
                }
                break;
            }
            default: {
                std::lock_guard<std::mutex> l(sinkLock_);
                if (sink_ == nullptr) {
                    if (h->type == SC_FRAME) {
                        sc_frame f;
                        if (As(p, n, f)) {
                            sc_release rl = { { SC_RELEASE, sizeof(sc_release) }, f.sid, f.idx };
                            Send(&rl, sizeof(rl));
                        }
                    }
                    break;
                }
                if (h->type == SC_FRAME) {
                    sc_frame f;
                    if (As(p, n, f)) sink_->OnFrame(f);
                } else if (h->type == SC_SHUTTER) {
                    sc_shutter s;
                    if (As(p, n, s)) sink_->OnShutter(s);
                } else if (h->type == SC_RESULT) {
                    sc_result r;
                    if (As(p, n, r)) sink_->OnResult(r);
                } else if (h->type == SC_ERROR) {
                    sc_error e;
                    if (As(p, n, e)) sink_->OnError(e);
                } else if (h->type == SC_CAPTURE_DONE) {
                    sc_capture_done d;
                    if (As(p, n, d)) sink_->OnCaptureDone(d);
                }
            }
        }
        for (int fd : fds) {
            close(fd);
        }
        cv_.notify_all();
    }

    int fd_ = -1;
    std::thread reader_;
    std::mutex lock_, sendLock_, callLock_, sinkLock_;
    std::condition_variable cv_;
    bool infoOk_ = false;
    sc_info info_ = {};
    std::vector<sc_caminfo> cams_;
    std::vector<sc_reply> replies_;
    std::deque<BufferSet> bufsets_;
    FrameSink *sink_ = nullptr;
};

/* ---------------------------------------------------- abilities (static) */
static bool AddI32(std::shared_ptr<CameraMetadata> &m, uint32_t tag, const std::vector<int32_t> &v)
{
    return v.empty() || m->addEntry(tag, v.data(), v.size());
}

static bool AddU8(std::shared_ptr<CameraMetadata> &m, uint32_t tag, const std::vector<uint8_t> &v)
{
    return v.empty() || m->addEntry(tag, v.data(), v.size());
}

static bool AddF(std::shared_ptr<CameraMetadata> &m, uint32_t tag, const std::vector<float> &v)
{
    return v.empty() || m->addEntry(tag, v.data(), v.size());
}

static bool SizeOkForPreview(const sc_size &s)
{
    return s.w <= 2400 && s.h <= 1800 && s.w >= 160 && (s.min_duration == 0 || s.min_duration <= 34000000);
}

static int CameraType(const sc_caminfo &ci, const std::vector<sc_caminfo> &all)
{
    if (ci.facing == 1) {
        return OHOS_CAMERA_TYPE_WIDE_ANGLE;
    }
    /* the first back camera is the main one; a back camera with a shorter focal length and a large sensor
     * output is the ultra wide; the others (macro, depth) are not a standard type */
    const sc_caminfo *main = nullptr;
    for (auto &c : all) {
        if (c.facing == 0) {
            main = &c;
            break;
        }
    }
    if (main == &ci || main == nullptr) {
        return OHOS_CAMERA_TYPE_WIDE_ANGLE;
    }
    int maxW = 0;
    for (int i = 0; i < ci.n_jpeg; i++) {
        maxW = std::max(maxW, ci.jpeg[i].w);
    }
    if (ci.focal_length > 0 && ci.focal_length < main->focal_length * 0.6f && maxW >= 3000) {
        return OHOS_CAMERA_TYPE_ULTRA_WIDE;
    }
    return OHOS_CAMERA_TYPE_UNSPECIFIED;
}

static std::shared_ptr<CameraMetadata> BuildAbility(const sc_caminfo &ci, const std::vector<sc_caminfo> &all)
{
    auto m = std::make_shared<CameraMetadata>(128, 16384);
    uint8_t pos = ci.facing == 1 ? OHOS_CAMERA_POSITION_FRONT : OHOS_CAMERA_POSITION_BACK;
    AddU8(m, OHOS_ABILITY_CAMERA_POSITION, { pos });
    AddU8(m, OHOS_ABILITY_CAMERA_TYPE, { (uint8_t)CameraType(ci, all) });
    AddU8(m, OHOS_ABILITY_CAMERA_CONNECTION_TYPE, { OHOS_CAMERA_CONNECTION_TYPE_BUILTIN });
    AddI32(m, OHOS_SENSOR_ORIENTATION, { ci.orientation });

    /* frame rates: the ranges the HAL advertises, max <= 60 */
    std::vector<int32_t> fps;
    for (int i = 0; i < ci.n_fps; i++) {
        if (ci.fps[i][1] <= 60) {
            fps.push_back(ci.fps[i][0]);
            fps.push_back(ci.fps[i][1]);
        }
    }
    if (fps.empty()) {
        fps = { 15, 30, 30, 30 };
    }
    AddI32(m, OHOS_ABILITY_FPS_RANGES, fps);

    /* stream configurations */
    std::vector<sc_size> prev, jpeg;
    for (int i = 0; i < ci.n_yuv; i++) {
        if (SizeOkForPreview(ci.yuv[i])) {
            prev.push_back(ci.yuv[i]);
        }
    }
    for (int i = 0; i < ci.n_jpeg; i++) {
        jpeg.push_back(ci.jpeg[i]);
    }
    auto bigFirst = [](const sc_size &a, const sc_size &b) { return a.w * a.h > b.w * b.h; };
    std::sort(prev.begin(), prev.end(), bigFirst);
    std::sort(jpeg.begin(), jpeg.end(), bigFirst);
    std::vector<int32_t> basic;
    for (auto &s : prev) {
        basic.insert(basic.end(), { OHOS_CAMERA_FORMAT_YCRCB_420_SP, s.w, s.h });
    }
    for (auto &s : jpeg) {
        basic.insert(basic.end(), { OHOS_CAMERA_FORMAT_JPEG, s.w, s.h });
    }
    AddI32(m, OHOS_ABILITY_STREAM_AVAILABLE_BASIC_CONFIGURATIONS, basic);

    /* extend configurations: mode 0 (normal); stream types 0 preview, 1 video, 2 still capture */
    int maxFps = 30;
    std::vector<int32_t> ext = { 0 };
    ext.push_back(0); /* preview */
    for (auto &s : prev) {
        ext.insert(ext.end(), { OHOS_CAMERA_FORMAT_YCRCB_420_SP, s.w, s.h, 0, 15, maxFps, -1 });
        ext.insert(ext.end(), { OHOS_CAMERA_FORMAT_YCBCR_420_SP, s.w, s.h, 0, 15, maxFps, -1 });
    }
    ext.push_back(-1);
    ext.push_back(1); /* video */
    for (auto &s : prev) {
        if (s.w >= 640) {
            ext.insert(ext.end(), { OHOS_CAMERA_FORMAT_YCRCB_420_SP, s.w, s.h, 30, 30, 30, -1 });
            ext.insert(ext.end(), { OHOS_CAMERA_FORMAT_YCBCR_420_SP, s.w, s.h, 30, 30, 30, -1 });
        }
    }
    ext.push_back(-1);
    ext.push_back(2); /* still */
    for (auto &s : jpeg) {
        ext.insert(ext.end(), { OHOS_CAMERA_FORMAT_JPEG, s.w, s.h, 0, 0, 0, -1 });
    }
    ext.push_back(-1);
    ext.push_back(-1); /* end of mode */
    AddI32(m, OHOS_ABILITY_STREAM_AVAILABLE_EXTEND_CONFIGURATIONS, ext);
    AddI32(m, OHOS_STREAM_AVAILABLE_FORMATS, { OHOS_CAMERA_FORMAT_JPEG, -1 });

    /* focus */
    bool af = false;
    for (int i = 0; i < ci.n_af; i++) {
        if (ci.af_modes[i] != 0) { /* AF_MODE_OFF */
            af = true;
        }
    }
    if (af) {
        AddU8(m, OHOS_ABILITY_FOCUS_MODES, { OHOS_CAMERA_FOCUS_MODE_CONTINUOUS_AUTO, OHOS_CAMERA_FOCUS_MODE_AUTO,
            OHOS_CAMERA_FOCUS_MODE_LOCKED });
        if (ci.min_focus_dist > 0) {
            AddF(m, OHOS_ABILITY_LENS_INFO_MINIMUM_FOCUS_DISTANCE, { ci.min_focus_dist });
        }
    }
    /* flash */
    AddU8(m, OHOS_ABILITY_FLASH_AVAILABLE, { (uint8_t)(ci.flash ? 1 : 0) });
    if (ci.flash) {
        AddU8(m, OHOS_ABILITY_FLASH_MODES, { OHOS_CAMERA_FLASH_MODE_CLOSE, OHOS_CAMERA_FLASH_MODE_OPEN,
            OHOS_CAMERA_FLASH_MODE_AUTO, OHOS_CAMERA_FLASH_MODE_ALWAYS_OPEN });
    } else {
        AddU8(m, OHOS_ABILITY_FLASH_MODES, { OHOS_CAMERA_FLASH_MODE_CLOSE });
    }
    /* exposure */
    AddU8(m, OHOS_ABILITY_EXPOSURE_MODES, { OHOS_CAMERA_EXPOSURE_MODE_CONTINUOUS_AUTO,
        OHOS_CAMERA_EXPOSURE_MODE_LOCKED, OHOS_CAMERA_EXPOSURE_MODE_AUTO });
    std::vector<uint8_t> meter = { OHOS_CAMERA_CENTER_WEIGHTED_METERING };
    if (ci.max_regions_ae > 0) {
        meter.push_back(OHOS_CAMERA_SPOT_METERING);
    }
    AddU8(m, OHOS_ABILITY_METER_MODES, meter);
    if (ci.ae_comp_step_den > 0) {
        AddI32(m, OHOS_ABILITY_AE_COMPENSATION_RANGE, { ci.ae_comp_min, ci.ae_comp_max });
        camera_rational_t step = { ci.ae_comp_step_num, ci.ae_comp_step_den };
        m->addEntry(OHOS_ABILITY_AE_COMPENSATION_STEP, &step, 1);
    }
    /* white balance */
    if (ci.n_awb > 0) {
        std::vector<uint8_t> awb(ci.awb_modes, ci.awb_modes + ci.n_awb);
        AddU8(m, OHOS_ABILITY_AWB_MODES, awb);
    }
    /* zoom */
    float zmax = ci.zoom_ratio_max > 1.0f ? ci.zoom_ratio_max : (ci.max_digital_zoom > 1.0f ? ci.max_digital_zoom : 1.0f);
    if (zmax > 10.0f) {
        zmax = 10.0f;
    }
    AddF(m, OHOS_ABILITY_ZOOM_RATIO_RANGE, { 1.0f, zmax });
    /* video stabilisation */
    std::vector<uint8_t> stab = { OHOS_CAMERA_VIDEO_STABILIZATION_OFF };
    for (int i = 0; i < ci.n_stab; i++) {
        if (ci.stab_modes[i] == 1) {
            stab.push_back(OHOS_CAMERA_VIDEO_STABILIZATION_AUTO);
        }
    }
    AddU8(m, OHOS_ABILITY_VIDEO_STABILIZATION_MODES, stab);
    /* lens / sensor */
    if (ci.focal_length > 0) {
        AddF(m, OHOS_ABILITY_FOCAL_LENGTH, { ci.focal_length });
    }
    if (ci.active[2] > 0) {
        AddI32(m, OHOS_SENSOR_INFO_ACTIVE_ARRAY_SIZE, { ci.active[0], ci.active[1], ci.active[2], ci.active[3] });
    }
    if (ci.pixel_array[0] > 0) {
        AddI32(m, OHOS_SENSOR_INFO_PIXEL_ARRAY_SIZE, { ci.pixel_array[0], ci.pixel_array[1] });
    }
    if (ci.iso_range[1] > 0) {
        AddI32(m, OHOS_SENSOR_INFO_SENSITIVITY_RANGE, { ci.iso_range[0], ci.iso_range[1] });
    }
    /* jpeg */
    if (ci.jpeg_max_size > 0) {
        AddI32(m, OHOS_JPEG_MAX_SIZE, { ci.jpeg_max_size });
    }
    std::vector<int32_t> th;
    for (int i = 0; i < ci.n_thumb; i++) {
        th.push_back(ci.thumb[i][0]);
        th.push_back(ci.thumb[i][1]);
    }
    AddI32(m, OHOS_JPEG_AVAILABLE_THUMBNAIL_SIZES, th);
    AddU8(m, OHOS_ABILITY_MUTE_MODES, { OHOS_CAMERA_MUTE_MODE_OFF, OHOS_CAMERA_MUTE_MODE_SOLID_COLOR_BLACK });
    AddU8(m, OHOS_ABILITY_PRELAUNCH_AVAILABLE, { 0 });
    return m;
}

/* ------------------------------------------------- settings -> controls */
struct Controls {
    sc_ctrl c;
    bool afRegionNew = false;
    int ohFocusMode = OHOS_CAMERA_FOCUS_MODE_CONTINUOUS_AUTO;
    float zoom = 1.0f;
    bool mute = false;

    Controls()
    {
        memset(&c, 0, sizeof(c));
        c.zoom = 1.0f;
        c.jpeg_quality = 95;
        c.awb_mode = 1;
        c.scene_mode = -1;
    }
};

static void ApplySettings(const std::vector<uint8_t> &vec, Controls &ct, const sc_caminfo &ci)
{
    if (vec.empty()) {
        return;
    }
    std::shared_ptr<CameraMetadata> meta;
    OHOS::Camera::MetadataUtils::ConvertVecToMetadata(vec, meta);
    if (meta == nullptr) {
        return;
    }
    common_metadata_header_t *h = meta->get();
    camera_metadata_item_t it;
    auto find = [&](uint32_t tag) {
        return OHOS::Camera::FindCameraMetadataItem(h, tag, &it) == 0 && it.count > 0;
    };
    if (find(OHOS_CONTROL_ZOOM_RATIO)) {
        ct.c.zoom = it.data.f[0];
    }
    if (find(OHOS_CONTROL_AE_EXPOSURE_COMPENSATION)) {
        ct.c.ae_comp = it.data.i32[0];
    }
    if (find(OHOS_CONTROL_FPS_RANGES) && it.count >= 2) {
        ct.c.fps_min = it.data.i32[0];
        ct.c.fps_max = it.data.i32[1];
    }
    if (find(OHOS_CONTROL_FOCUS_MODE)) {
        int fm = it.data.u8[0];
        if (fm != ct.ohFocusMode && fm == OHOS_CAMERA_FOCUS_MODE_AUTO) {
            ct.c.af_trigger = 1;
        }
        ct.ohFocusMode = fm;
        switch (fm) {
            case OHOS_CAMERA_FOCUS_MODE_CONTINUOUS_AUTO: ct.c.af_mode = 0; break;
            case OHOS_CAMERA_FOCUS_MODE_AUTO: ct.c.af_mode = 2; break;
            case OHOS_CAMERA_FOCUS_MODE_LOCKED: ct.c.af_mode = 2; break;
            default: ct.c.af_mode = 3; break;
        }
    }
    auto region = [&](int32_t out[5]) {
        if (it.count < 2 || ci.active[2] <= 0) {
            return;
        }
        float x = std::min(std::max(it.data.f[0], 0.0f), 1.0f);
        float y = std::min(std::max(it.data.f[1], 0.0f), 1.0f);
        int cx = (int)(x * ci.active[2]), cy = (int)(y * ci.active[3]);
        int hw = ci.active[2] / 16, hh = ci.active[3] / 16;
        out[0] = std::max(0, cx - hw);
        out[1] = std::max(0, cy - hh);
        out[2] = std::min(ci.active[2] - 1, cx + hw);
        out[3] = std::min(ci.active[3] - 1, cy + hh);
        out[4] = 1000;
    };
    if (find(OHOS_CONTROL_AF_REGIONS)) {
        region(ct.c.af_region);
        ct.c.af_trigger = 1;
    }
    if (find(OHOS_CONTROL_AE_REGIONS)) {
        region(ct.c.ae_region);
    }
    if (find(OHOS_CONTROL_FLASH_MODE)) {
        switch (it.data.u8[0]) {
            case OHOS_CAMERA_FLASH_MODE_OPEN: ct.c.flash_mode = 3; break;        /* light on */
            case OHOS_CAMERA_FLASH_MODE_AUTO: ct.c.flash_mode = 1; break;
            case OHOS_CAMERA_FLASH_MODE_ALWAYS_OPEN: ct.c.flash_mode = 2; break;
            default: ct.c.flash_mode = 0; break;
        }
    }
    if (find(OHOS_CONTROL_EXPOSURE_MODE)) {
        ct.c.ae_lock = it.data.u8[0] == OHOS_CAMERA_EXPOSURE_MODE_LOCKED;
    }
    if (find(OHOS_CONTROL_AE_LOCK)) {
        ct.c.ae_lock = it.data.u8[0] != 0;
    }
    if (find(OHOS_CONTROL_AWB_LOCK)) {
        ct.c.awb_lock = it.data.u8[0] != 0;
    }
    if (find(OHOS_CONTROL_AWB_MODE)) {
        ct.c.awb_mode = it.data.u8[0];
    }
    if (find(OHOS_CONTROL_VIDEO_STABILIZATION_MODE)) {
        ct.c.video_stab = it.data.u8[0] != OHOS_CAMERA_VIDEO_STABILIZATION_OFF;
    }
    if (find(OHOS_STATISTICS_FACE_DETECT_SWITCH)) {
        ct.c.face_detect = it.data.u8[0] != 0;
    }
    if (find(OHOS_JPEG_ORIENTATION)) {
        ct.c.jpeg_orientation = it.data.i32[0];
    }
    if (find(OHOS_JPEG_QUALITY)) {
        int q = it.data.u8[0];
        /* OpenHarmony uses levels (0 high, 1 middle, 2 low) or a 1-100 value */
        ct.c.jpeg_quality = q <= 2 ? (q == 0 ? 95 : q == 1 ? 85 : 70) : (uint8_t)std::min(q, 100);
    }
    if (find(OHOS_JPEG_GPS_COORDINATES) && it.count >= 3) {
        ct.c.gps[0] = it.data.d[0];
        ct.c.gps[1] = it.data.d[1];
        ct.c.gps[2] = it.data.d[2];
        ct.c.has_gps = 1;
        ct.c.gps_ts = time(nullptr);
    }
    if (find(OHOS_CONTROL_CAPTURE_MIRROR)) {
        ct.c.mirror = it.data.u8[0];
    }
}

/* ----------------------------------------------------------- streams */
struct HalBuffer {
    int fd = -1;
    void *map = nullptr;
    size_t mapLen = 0;
    sc_buflayout l = {};
};

struct Stream {
    StreamInfo info;
    int kind = SC_KIND_PREVIEW;
    int halW = 0, halH = 0;
    sptr<Surface> producer;
    std::vector<HalBuffer> bufs;
    int32_t streamingCapture = -1;
    int64_t frames = 0;
    int bit = -1;
};

static int StreamKind(const StreamInfo &si)
{
    if (si.format_ == GRAPHIC_PIXEL_FMT_BLOB || si.encodeType_ == ENCODE_TYPE_JPEG) {
        return SC_KIND_STILL;
    }
    switch (si.intent_) {
        case VIDEO: return SC_KIND_VIDEO;
        case STILL_CAPTURE: return SC_KIND_YUV_STILL;
        default: return SC_KIND_PREVIEW;
    }
}

/* choose the HAL size for a requested size: exact, or the smallest larger one with the same aspect ratio,
 * or the closest (the copy scales) */
static void PickHalSize(const sc_caminfo &ci, int kind, int w, int h, int &ow, int &oh)
{
    const sc_size *arr = kind == SC_KIND_STILL ? ci.jpeg : ci.yuv;
    int n = kind == SC_KIND_STILL ? ci.n_jpeg : ci.n_yuv;
    ow = w;
    oh = h;
    if (n <= 0) {
        return;
    }
    for (int i = 0; i < n; i++) {
        if (arr[i].w == w && arr[i].h == h) {
            return;
        }
    }
    double ar = (double)w / std::max(h, 1);
    int best = -1;
    long bestArea = 0;
    for (int i = 0; i < n; i++) {
        if (std::fabs((double)arr[i].w / std::max(arr[i].h, 1) - ar) > 0.02 || arr[i].w < w || arr[i].h < h) {
            continue;
        }
        long area = (long)arr[i].w * arr[i].h;
        if (kind != SC_KIND_STILL && !SizeOkForPreview(arr[i])) {
            continue;
        }
        if (best < 0 || area < bestArea) {
            best = i;
            bestArea = area;
        }
    }
    if (best < 0) {
        long target = (long)w * h, bestDiff = -1;
        for (int i = 0; i < n; i++) {
            if (kind != SC_KIND_STILL && !SizeOkForPreview(arr[i])) {
                continue;
            }
            long d = std::labs((long)arr[i].w * arr[i].h - target);
            if (bestDiff < 0 || d < bestDiff) {
                bestDiff = d;
                best = i;
            }
        }
    }
    if (best >= 0) {
        ow = arr[best].w;
        oh = arr[best].h;
    }
}

static void DmaSync(int fd, uint64_t flags)
{
    struct dma_buf_sync s = { flags };
    ioctl(fd, DMA_BUF_IOCTL_SYNC, &s);
}

static inline uint8_t Clamp(int v) { return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v; }

/* copy a 4:2:0 frame (any QTI layout) into an NV21/NV12/RGBA destination, nearest-neighbour scaling */
static void CopyYuv(const uint8_t *src, const sc_buflayout &L, int sw, int sh, uint8_t *dst, int dstride, int dw,
    int dh, int dfmt, bool mirror)
{
    const uint8_t *sy = src + L.y_off;
    const uint8_t *sc = src + L.c_off;
    const uint8_t *sv = src + L.v_off;
    bool same = sw == dw && sh == dh && !mirror;
    if (dfmt == GRAPHIC_PIXEL_FMT_RGBA_8888) {
        for (int y = 0; y < dh; y++) {
            int syy = same ? y : y * sh / dh;
            uint8_t *o = dst + (size_t)y * dstride;
            for (int x = 0; x < dw; x++) {
                int sxx = same ? x : x * sw / dw;
                if (mirror) sxx = sw - 1 - sxx;
                int Y = sy[(size_t)syy * L.y_stride + sxx];
                size_t ci = (size_t)(syy / 2) * L.c_stride + (sxx / 2) * L.c_step;
                int c0 = sc[ci], c1 = L.c_step == 1 ? sv[(size_t)(syy / 2) * L.c_stride + sxx / 2] : sc[ci + 1];
                int U = L.cb_first ? c0 : c1, V = L.cb_first ? c1 : c0;
                int C = Y - 16, D = U - 128, E = V - 128;
                o[x * 4 + 0] = Clamp((298 * C + 409 * E + 128) >> 8);
                o[x * 4 + 1] = Clamp((298 * C - 100 * D - 208 * E + 128) >> 8);
                o[x * 4 + 2] = Clamp((298 * C + 516 * D + 128) >> 8);
                o[x * 4 + 3] = 255;
            }
        }
        return;
    }
    bool dstCbFirst = dfmt == GRAPHIC_PIXEL_FMT_YCBCR_420_SP;
    uint8_t *dy = dst, *dc = dst + (size_t)dstride * dh;
    /* Y */
    for (int y = 0; y < dh; y++) {
        int syy = same ? y : y * sh / dh;
        const uint8_t *r = sy + (size_t)syy * L.y_stride;
        uint8_t *o = dy + (size_t)y * dstride;
        if (same) {
            memcpy(o, r, dw);
        } else {
            for (int x = 0; x < dw; x++) {
                int sxx = x * sw / dw;
                o[x] = r[mirror ? sw - 1 - sxx : sxx];
            }
        }
    }
    /* chroma */
    bool swap = (bool)L.cb_first != dstCbFirst;
    for (int y = 0; y < dh / 2; y++) {
        int syy = same ? y : y * (sh / 2) / (dh / 2);
        const uint8_t *r = sc + (size_t)syy * L.c_stride;
        const uint8_t *rv = sv + (size_t)syy * L.c_stride;
        uint8_t *o = dc + (size_t)y * dstride;
        if (same && !swap && L.c_step == 2) {
            memcpy(o, r, dw);
            continue;
        }
        for (int x = 0; x < dw / 2; x++) {
            int sxx = same ? x : x * (sw / 2) / (dw / 2);
            if (mirror) sxx = sw / 2 - 1 - sxx;
            uint8_t c0, c1;
            if (L.c_step == 1) {
                c0 = r[sxx];
                c1 = rv[sxx];
            } else {
                c0 = r[sxx * 2];
                c1 = r[sxx * 2 + 1];
            }
            o[x * 2] = swap ? c1 : c0;
            o[x * 2 + 1] = swap ? c0 : c1;
        }
    }
}

class StreamOperatorImpl;

/* ---------------------------------------------------------- device */
class CameraDeviceImpl : public ICameraDevice, public FrameSink {
public:
    CameraDeviceImpl(int idx, const sc_caminfo &ci, const sptr<ICameraDeviceCallback> &cb)
        : idx_(idx), ci_(ci), cb_(cb) {}
    ~CameraDeviceImpl() override = default;

    int32_t GetStreamOperator(const sptr<IStreamOperatorCallback> &callbackObj,
        sptr<IStreamOperator> &streamOperator) override;
    int32_t UpdateSettings(const std::vector<uint8_t> &settings) override;
    int32_t SetResultMode(ResultCallbackMode mode) override
    {
        resultMode_ = mode;
        return CAM_OK;
    }
    int32_t GetEnabledResults(std::vector<int32_t> &results) override
    {
        std::lock_guard<std::mutex> l(lock_);
        results.assign(enabled_.begin(), enabled_.end());
        return CAM_OK;
    }
    int32_t EnableResult(const std::vector<int32_t> &results) override
    {
        std::lock_guard<std::mutex> l(lock_);
        enabled_.insert(results.begin(), results.end());
        return CAM_OK;
    }
    int32_t DisableResult(const std::vector<int32_t> &results) override
    {
        std::lock_guard<std::mutex> l(lock_);
        for (auto r : results) {
            enabled_.erase(r);
        }
        return CAM_OK;
    }
    int32_t Close() override;

    /* FrameSink */
    void OnFrame(const sc_frame &f) override;
    void OnShutter(const sc_shutter &s) override;
    void OnResult(const sc_result &r) override;
    void OnError(const sc_error &e) override;
    void OnCaptureDone(const sc_capture_done &d) override;
    void OnDisconnect() override;

    bool Open();
    Controls &Ctrl() { return ctrl_; }
    std::mutex &CtrlLock() { return ctrlLock_; }
    const sc_caminfo &Info() const { return ci_; }
    bool Closed() const { return closed_; }
    void SendRepeating(bool full);

private:
    int idx_;
    sc_caminfo ci_;
    sptr<ICameraDeviceCallback> cb_;
    sptr<StreamOperatorImpl> op_;
    std::mutex lock_, ctrlLock_;
    std::set<int32_t> enabled_;
    ResultCallbackMode resultMode_ = PER_FRAME;
    Controls ctrl_;
    std::atomic<bool> closed_ { false };
    uint8_t lastAf_ = 0xff, lastAe_ = 0xff;
    uint32_t resultCount_ = 0;
};

/* ---------------------------------------------------------- operator */
class StreamOperatorImpl : public IStreamOperator {
public:
    StreamOperatorImpl(CameraDeviceImpl *dev, const sptr<IStreamOperatorCallback> &cb) : dev_(dev), cb_(cb)
    {
        worker_ = std::thread([this] { Worker(); });
    }
    ~StreamOperatorImpl() override
    {
        Shutdown();
    }

    void Shutdown()
    {
        {
            std::lock_guard<std::mutex> l(qlock_);
            if (stop_) {
                return;
            }
            stop_ = true;
        }
        qcv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        std::lock_guard<std::mutex> l(lock_);
        for (auto &kv : streams_) {
            UnmapStream(kv.second);
        }
        streams_.clear();
    }

    int32_t IsStreamsSupported(OperationMode mode, const std::vector<uint8_t> &modeSetting,
        const std::vector<StreamInfo> &infos, StreamSupportType &type) override
    {
        type = infos.size() <= SC_MAX_STREAMS ? DYNAMIC_SUPPORTED : NOT_SUPPORTED;
        for (auto &si : infos) {
            if (si.width_ <= 0 || si.height_ <= 0) {
                type = NOT_SUPPORTED;
            }
        }
        return CAM_OK;
    }

    int32_t CreateStreams(const std::vector<StreamInfo> &infos) override
    {
        std::lock_guard<std::mutex> l(lock_);
        for (auto &si : infos) {
            if (streams_.count(si.streamId_)) {
                LOGK("CreateStreams: stream %d exists", si.streamId_);
                return INVALID_ARGUMENT;
            }
        }
        for (auto &si : infos) {
            Stream s;
            s.info = si;
            s.kind = StreamKind(si);
            if (si.bufferQueue_ != nullptr && si.bufferQueue_->producer_ != nullptr) {
                s.producer = Surface::CreateSurfaceAsProducer(si.bufferQueue_->producer_);
            }
            LOGK("create stream %d: %dx%d format %d intent %d encode %d -> kind %d, producer %s", si.streamId_,
                si.width_, si.height_, si.format_, si.intent_, si.encodeType_, s.kind, s.producer ? "yes" : "no");
            streams_[si.streamId_] = s;
        }
        dirty_ = true;
        return CAM_OK;
    }

    int32_t ReleaseStreams(const std::vector<int32_t> &ids) override
    {
        std::vector<int32_t> ended;
        {
            std::lock_guard<std::mutex> l(lock_);
            for (auto id : ids) {
                auto it = streams_.find(id);
                if (it == streams_.end()) {
                    continue;
                }
                if (it->second.streamingCapture >= 0) {
                    ended.push_back(it->second.streamingCapture);
                }
                UnmapStream(it->second);
                streams_.erase(it);
            }
            dirty_ = true;
        }
        for (auto c : ended) {
            CancelCapture(c);
        }
        LOGI("released %zu streams", ids.size());
        return CAM_OK;
    }

    int32_t CommitStreams(OperationMode mode, const std::vector<uint8_t> &modeSetting) override
    {
        if (!modeSetting.empty()) {
            std::lock_guard<std::mutex> g(dev_->CtrlLock());
            ApplySettings(modeSetting, dev_->Ctrl(), dev_->Info());
        }
        return Configure() ? CAM_OK : DEVICE_ERROR;
    }

    int32_t GetStreamAttributes(std::vector<StreamAttribute> &attributes) override
    {
        std::lock_guard<std::mutex> l(lock_);
        for (auto &kv : streams_) {
            StreamAttribute a = {};
            a.streamId_ = kv.first;
            a.width_ = kv.second.info.width_;
            a.height_ = kv.second.info.height_;
            a.overrideFormat_ = kv.second.info.format_;
            a.overrideDataspace_ = kv.second.info.dataspace_;
            a.producerUsage_ = BUFFER_USAGE_CPU_READ | BUFFER_USAGE_CPU_WRITE | BUFFER_USAGE_MEM_DMA;
            a.producerBufferCount_ = 4;
            a.maxBatchCaptureCount_ = 1;
            a.maxCaptureCount_ = 1;
            attributes.push_back(a);
        }
        return CAM_OK;
    }

    int32_t AttachBufferQueue(int32_t streamId, const sptr<BufferProducerSequenceable> &bufferProducer) override
    {
        std::lock_guard<std::mutex> l(lock_);
        auto it = streams_.find(streamId);
        if (it == streams_.end() || bufferProducer == nullptr || bufferProducer->producer_ == nullptr) {
            return INVALID_ARGUMENT;
        }
        it->second.producer = Surface::CreateSurfaceAsProducer(bufferProducer->producer_);
        LOGI("stream %d: buffer queue attached", streamId);
        return CAM_OK;
    }

    int32_t DetachBufferQueue(int32_t streamId) override
    {
        std::lock_guard<std::mutex> l(lock_);
        auto it = streams_.find(streamId);
        if (it == streams_.end()) {
            return INVALID_ARGUMENT;
        }
        it->second.producer = nullptr;
        return CAM_OK;
    }

    int32_t Capture(int32_t captureId, const CaptureInfo &info, bool isStreaming) override
    {
        if (dirty_ && !Configure()) {
            return DEVICE_ERROR;
        }
        sc_ctrl ctrl;
        {
            std::lock_guard<std::mutex> g(dev_->CtrlLock());
            ApplySettings(info.captureSetting_, dev_->Ctrl(), dev_->Info());
            ctrl = dev_->Ctrl().c;
            dev_->Ctrl().c.af_trigger = 0;
        }
        uint32_t mask = 0;
        {
            std::lock_guard<std::mutex> l(lock_);
            for (auto id : info.streamIds_) {
                auto it = streams_.find(id);
                if (it == streams_.end() || it->second.bit < 0) {
                    LOGK("capture %d: unknown stream %d", captureId, id);
                    return INVALID_ARGUMENT;
                }
                mask |= 1u << it->second.bit;
                if (isStreaming) {
                    it->second.streamingCapture = captureId;
                    it->second.frames = 0;
                }
            }
            if (isStreaming) {
                streaming_[captureId] = info.streamIds_;
            } else {
                shots_[captureId] = { info.streamIds_, info.enableShutterCallback_, false };
            }
        }
        if (isStreaming) {
            LOGK("capture %d: streaming %zu streams", captureId, info.streamIds_.size());
            if (cb_) {
                cb_->OnCaptureStarted(captureId, info.streamIds_);
            }
            SendRepeat(captureId, ctrl);
        } else {
            sc_request q = {};
            q.hdr = { SC_SHOT, sizeof(sc_request) };
            q.tag = captureId;
            q.mask = mask;
            q.c = ctrl;
            LOGK("capture %d: single shot, mask 0x%x, orientation %d", captureId, mask, ctrl.jpeg_orientation);
            if (!Bridge::Get().Send(&q, sizeof(q))) {
                return DEVICE_ERROR;
            }
        }
        return CAM_OK;
    }

    int32_t CancelCapture(int32_t captureId) override
    {
        std::vector<CaptureEndedInfo> infos;
        sc_ctrl ctrl;
        {
            std::lock_guard<std::mutex> g(dev_->CtrlLock());
            ctrl = dev_->Ctrl().c;
        }
        {
            std::lock_guard<std::mutex> l(lock_);
            auto it = streaming_.find(captureId);
            if (it == streaming_.end()) {
                return CAM_OK;
            }
            for (auto id : it->second) {
                auto s = streams_.find(id);
                CaptureEndedInfo e = { id, 0 };
                if (s != streams_.end()) {
                    e.frameCount_ = (int32_t)s->second.frames;
                    s->second.streamingCapture = -1;
                }
                infos.push_back(e);
            }
            streaming_.erase(it);
        }
        SendRepeat(-1, ctrl);
        LOGK("capture %d cancelled", captureId);
        if (cb_) {
            cb_->OnCaptureEnded(captureId, infos);
        }
        return CAM_OK;
    }

    int32_t ChangeToOfflineStream(const std::vector<int32_t> &streamIds,
        const sptr<IStreamOperatorCallback> &callbackObj, sptr<IOfflineStreamOperator> &offlineOperator) override
    {
        return METHOD_NOT_SUPPORTED;
    }

    /* settings changed on the device: refresh the repeating request */
    void SettingsChanged(const sc_ctrl &c)
    {
        bool any;
        {
            std::lock_guard<std::mutex> l(lock_);
            any = !streaming_.empty();
        }
        if (any) {
            sc_request q = {};
            q.hdr = { SC_SETTINGS, sizeof(sc_request) };
            q.c = c;
            Bridge::Get().Send(&q, sizeof(q));
        }
    }

    /* from the bridge reader thread: queue, the worker copies */
    void QueueFrame(const sc_frame &f)
    {
        {
            std::lock_guard<std::mutex> l(qlock_);
            q_.push_back(f);
        }
        qcv_.notify_one();
    }

    void Shutter(const sc_shutter &s)
    {
        std::vector<int32_t> ids;
        bool cbOn = false, first = false;
        {
            std::lock_guard<std::mutex> l(lock_);
            auto it = shots_.find(s.tag);
            if (it == shots_.end()) {
                return;
            }
            ids = it->second.ids;
            cbOn = it->second.shutterCb;
            first = !it->second.started;
            it->second.started = true;
        }
        if (cb_ && first) {
            cb_->OnCaptureStarted(s.tag, ids);
            if (cbOn) {
                cb_->OnFrameShutter(s.tag, ids, (uint64_t)s.ts);
            }
        }
    }

    void CaptureDone(const sc_capture_done &d)
    {
        /* wait until the worker delivered the frames of this capture */
        {
            std::unique_lock<std::mutex> l(qlock_);
            qcv_.wait_for(l, std::chrono::milliseconds(2000), [this] { return q_.empty() && !busy_; });
        }
        std::vector<int32_t> ids;
        bool started;
        {
            std::lock_guard<std::mutex> l(lock_);
            auto it = shots_.find(d.tag);
            if (it == shots_.end()) {
                return;
            }
            ids = it->second.ids;
            started = it->second.started;
            shots_.erase(it);
        }
        if (!cb_) {
            return;
        }
        if (!started) {
            cb_->OnCaptureStarted(d.tag, ids);
        }
        if (d.ok) {
            std::vector<CaptureEndedInfo> infos;
            for (auto id : ids) {
                infos.push_back({ id, 1 });
            }
            cb_->OnCaptureEnded(d.tag, infos);
        } else {
            std::vector<CaptureErrorInfo> errs;
            for (auto id : ids) {
                errs.push_back({ id, BUFFER_LOST });
            }
            cb_->OnCaptureError(d.tag, errs);
        }
    }

    void Disconnected()
    {
        std::vector<int32_t> caps;
        {
            std::lock_guard<std::mutex> l(lock_);
            for (auto &kv : streaming_) {
                caps.push_back(kv.first);
            }
            for (auto &kv : shots_) {
                caps.push_back(kv.first);
            }
            streaming_.clear();
            shots_.clear();
        }
        if (cb_) {
            for (auto c : caps) {
                cb_->OnCaptureError(c, { { 0, OHOS::HDI::Camera::V1_0::UNKNOWN_ERROR } });
            }
        }
    }

private:
    struct Shot {
        std::vector<int32_t> ids;
        bool shutterCb;
        bool started;
    };

    void SendRepeat(int32_t tag, const sc_ctrl &c)
    {
        uint32_t mask = 0;
        {
            std::lock_guard<std::mutex> l(lock_);
            for (auto &kv : streaming_) {
                for (auto id : kv.second) {
                    auto s = streams_.find(id);
                    if (s != streams_.end() && s->second.bit >= 0) {
                        mask |= 1u << s->second.bit;
                    }
                }
            }
            if (tag < 0 && !streaming_.empty()) {
                tag = streaming_.begin()->first;
            }
        }
        sc_request q = {};
        q.hdr = { SC_REPEAT, sizeof(sc_request) };
        q.tag = tag;
        q.mask = mask;
        q.c = c;
        Bridge::Get().Send(&q, sizeof(q));
    }

    void UnmapStream(Stream &s)
    {
        for (auto &b : s.bufs) {
            if (b.map) {
                munmap(b.map, b.mapLen);
            }
            if (b.fd >= 0) {
                close(b.fd);
            }
        }
        s.bufs.clear();
        s.bit = -1;
    }

    bool Configure()
    {
        sc_config c = {};
        c.hdr = { SC_CONFIG, sizeof(sc_config) };
        std::vector<int32_t> order;
        {
            std::lock_guard<std::mutex> l(lock_);
            for (auto &kv : streams_) {
                if (c.n >= SC_MAX_STREAMS) {
                    break;
                }
                Stream &s = kv.second;
                UnmapStream(s);
                int hw, hh;
                PickHalSize(dev_->Info(), s.kind, s.info.width_, s.info.height_, hw, hh);
                s.halW = hw;
                s.halH = hh;
                s.bit = c.n;
                c.s[c.n] = { kv.first, s.kind, hw, hh };
                if (s.kind == SC_KIND_VIDEO) {
                    c.video_mode = 1;
                }
                order.push_back(kv.first);
                c.n++;
            }
            dirty_ = false;
        }
        /* stop the repeating request before reconfiguring */
        int r = Bridge::Get().Call(&c, sizeof(c), SC_CONFIG, 15000);
        if (r != 0) {
            LOGK("configure %d streams failed: %d", c.n, r);
            return false;
        }
        std::vector<BufferSet> sets;
        if (!Bridge::Get().TakeBuffers(c.n, sets, 5000)) {
            LOGK("no buffers from cambridge");
            return false;
        }
        std::lock_guard<std::mutex> l(lock_);
        for (auto &bs : sets) {
            auto it = streams_.find(bs.b.sid);
            if (it == streams_.end()) {
                for (int fd : bs.fds) {
                    close(fd);
                }
                continue;
            }
            Stream &s = it->second;
            for (int i = 0; i < bs.b.n && i < (int)bs.fds.size(); i++) {
                HalBuffer hb;
                hb.fd = bs.fds[i];
                hb.l = bs.b.l[i];
                hb.mapLen = hb.l.size;
                hb.map = mmap(nullptr, hb.mapLen, PROT_READ, MAP_SHARED, hb.fd, hb.l.offset);
                if (hb.map == MAP_FAILED) {
                    LOGK("mmap of a HAL buffer failed: %d", errno);
                    hb.map = nullptr;
                }
                s.bufs.push_back(hb);
            }
            LOGK("stream %d: HAL %dx%d, %zu buffers, y stride %u, chroma at %u step %u cb_first %u", bs.b.sid,
                s.halW, s.halH, s.bufs.size(), bs.b.l[0].y_stride, bs.b.l[0].c_off, bs.b.l[0].c_step,
                bs.b.l[0].cb_first);
        }
        return true;
    }

    void Worker()
    {
        for (;;) {
            sc_frame f;
            {
                std::unique_lock<std::mutex> l(qlock_);
                qcv_.wait(l, [this] { return stop_ || !q_.empty(); });
                if (stop_) {
                    return;
                }
                /* preview: only the newest frame of a stream matters */
                f = q_.front();
                q_.pop_front();
                busy_ = true;
            }
            Deliver(f);
            sc_release rl = { { SC_RELEASE, sizeof(sc_release) }, f.sid, f.idx };
            Bridge::Get().Send(&rl, sizeof(rl));
            {
                std::lock_guard<std::mutex> l(qlock_);
                busy_ = false;
            }
            qcv_.notify_all();
        }
    }

    void Deliver(const sc_frame &f)
    {
        std::unique_lock<std::mutex> l(lock_);
        auto it = streams_.find(f.sid);
        if (it == streams_.end() || f.status != 0) {
            return;
        }
        Stream &s = it->second;
        if (f.idx < 0 || f.idx >= (int)s.bufs.size() || s.bufs[f.idx].map == nullptr || s.producer == nullptr) {
            return;
        }
        HalBuffer &hb = s.bufs[f.idx];
        sptr<Surface> prod = s.producer;
        StreamInfo si = s.info;
        int kind = s.kind, sw = s.halW, sh = s.halH;
        int32_t captureId = s.streamingCapture >= 0 ? s.streamingCapture : f.tag;
        bool mute;
        bool mirror;
        {
            std::lock_guard<std::mutex> g(dev_->CtrlLock());
            mute = dev_->Ctrl().mute;
            mirror = false;
        }
        l.unlock();

        PaddedRequestConfig rc = {};
        rc.c.width = si.width_;
        rc.c.height = si.height_;
        rc.c.strideAlignment = 8;
        rc.c.format = si.format_;
        rc.c.usage = BUFFER_USAGE_CPU_READ | BUFFER_USAGE_CPU_WRITE | BUFFER_USAGE_MEM_DMA;
        rc.c.timeout = 0;
        sptr<SurfaceBuffer> sb;
        int32_t fence = -1;
        GSError ret = PS(prod)->ProducerSurface::RequestBuffer(sb, fence, rc.c);
        if (ret != GSERROR_OK || sb == nullptr) {
            if ((dropped_++ % 60) == 0) {
                LOGI("stream %d: no free buffer (%d), frame dropped", f.sid, ret);
            }
            return;
        }
        if (fence >= 0) {
            pollfd p = { fence, POLLIN, 0 };
            poll(&p, 1, 100);
            close(fence);
        }
        SurfaceBufferImpl *b = SB(sb);
        uint8_t *dst = static_cast<uint8_t *>(b->SurfaceBufferImpl::GetVirAddr());
        if (dst == nullptr) {
            b->SurfaceBufferImpl::Map();
            dst = static_cast<uint8_t *>(b->SurfaceBufferImpl::GetVirAddr());
        }
        if (dst == nullptr) {
            LOGK("stream %d: cannot map the output buffer", f.sid);
            PS(prod)->ProducerSurface::CancelBuffer(sb);
            return;
        }
        uint32_t dsize = b->SurfaceBufferImpl::GetSize();
        int dstride = b->SurfaceBufferImpl::GetStride();
        int32_t dataSize = 0;
        DmaSync(hb.fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ);
        const uint8_t *src = static_cast<const uint8_t *>(hb.map);
        if (kind == SC_KIND_STILL) {
            dataSize = (int32_t)std::min<uint32_t>(f.data_size, dsize);
            memcpy(dst, src, dataSize);
            LOGK("capture %d: JPEG %d bytes -> buffer of %u", f.tag, dataSize, dsize);
        } else if (mute) {
            memset(dst, 16, (size_t)dstride * si.height_);
            memset(dst + (size_t)dstride * si.height_, 128, dsize - (size_t)dstride * si.height_);
        } else {
            if (dstride <= 0) {
                dstride = si.width_ * (si.format_ == GRAPHIC_PIXEL_FMT_RGBA_8888 ? 4 : 1);
            }
            CopyYuv(src, hb.l, sw, sh, dst, dstride, si.width_, si.height_, si.format_, mirror);
            dataSize = (int32_t)dsize;
        }
        DmaSync(hb.fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ);
        b->SurfaceBufferImpl::FlushCache();

        sptr<BufferExtraData> ed = b->SurfaceBufferImpl::GetExtraData();
        if (ed != nullptr) {
            auto *e = static_cast<BufferExtraDataImpl *>(ed.GetRefPtr());
            e->BufferExtraDataImpl::ExtraSet("dataSize", dataSize);
            e->BufferExtraDataImpl::ExtraSet("isKeyFrame", (int32_t)0);
            e->BufferExtraDataImpl::ExtraSet("timeStamp", (int64_t)f.ts);
            e->BufferExtraDataImpl::ExtraSet("streamId", (int32_t)f.sid);
            e->BufferExtraDataImpl::ExtraSet("captureId", captureId);
            e->BufferExtraDataImpl::ExtraSet("dataWidth", (int32_t)si.width_);
            e->BufferExtraDataImpl::ExtraSet("dataHeight", (int32_t)si.height_);
            if (kind == SC_KIND_STILL) {
                e->BufferExtraDataImpl::ExtraSet("isDegradedImage", (int32_t)0);
                e->BufferExtraDataImpl::ExtraSet("imageId", (int64_t)f.ts);
                e->BufferExtraDataImpl::ExtraSet("deferredProcessingType", (int32_t)0);
            }
        }
        PaddedFlushConfig fc = {};
        fc.c.damage.x = 0;
        fc.c.damage.y = 0;
        fc.c.damage.w = si.width_;
        fc.c.damage.h = si.height_;
        fc.c.timestamp = f.ts;
        ret = PS(prod)->ProducerSurface::FlushBuffer(sb, -1, fc.c);
        if (ret != GSERROR_OK) {
            LOGI("stream %d: FlushBuffer %d", f.sid, ret);
        }
        std::lock_guard<std::mutex> g(lock_);
        auto it2 = streams_.find(f.sid);
        if (it2 != streams_.end()) {
            it2->second.frames++;
            if (it2->second.frames == 1) {
                LOGK("stream %d: first frame delivered (%dx%d fmt %d stride %d)", f.sid, si.width_, si.height_,
                    si.format_, dstride);
            }
        }
    }

    CameraDeviceImpl *dev_;
    sptr<IStreamOperatorCallback> cb_;
    std::mutex lock_;
    std::map<int32_t, Stream> streams_;
    std::map<int32_t, std::vector<int32_t>> streaming_;
    std::map<int32_t, Shot> shots_;
    std::atomic<bool> dirty_ { false };
    std::thread worker_;
    std::mutex qlock_;
    std::condition_variable qcv_;
    std::deque<sc_frame> q_;
    bool stop_ = false;
    bool busy_ = false;
    uint32_t dropped_ = 0;
};

/* ---------------------------------------------------------- device impl */
bool CameraDeviceImpl::Open()
{
    sc_open o = { { SC_OPEN, sizeof(sc_open) }, idx_ };
    Bridge::Get().SetSink(this);
    int r = Bridge::Get().Call(&o, sizeof(o), SC_OPEN, 15000);
    if (r != 0) {
        LOGK("open camera %d failed: %d", idx_, r);
        Bridge::Get().SetSink(nullptr);
        return false;
    }
    LOGK("camera %d (HAL id %d) open", idx_, ci_.hal_id);
    return true;
}

int32_t CameraDeviceImpl::GetStreamOperator(const sptr<IStreamOperatorCallback> &callbackObj,
    sptr<IStreamOperator> &streamOperator)
{
    std::lock_guard<std::mutex> l(lock_);
    if (op_ == nullptr) {
        op_ = new StreamOperatorImpl(this, callbackObj);
    }
    streamOperator = op_;
    return CAM_OK;
}

int32_t CameraDeviceImpl::UpdateSettings(const std::vector<uint8_t> &settings)
{
    sc_ctrl c;
    {
        std::lock_guard<std::mutex> g(ctrlLock_);
        ApplySettings(settings, ctrl_, ci_);
        c = ctrl_.c;
        ctrl_.c.af_trigger = 0;
    }
    sptr<StreamOperatorImpl> op;
    {
        std::lock_guard<std::mutex> l(lock_);
        op = op_;
    }
    if (op) {
        op->SettingsChanged(c);
    }
    return CAM_OK;
}

int32_t CameraDeviceImpl::Close()
{
    if (closed_.exchange(true)) {
        return CAM_OK;
    }
    sptr<StreamOperatorImpl> op;
    {
        std::lock_guard<std::mutex> l(lock_);
        op = op_;
    }
    sc_hdr h = { SC_CLOSE, sizeof(sc_hdr) };
    Bridge::Get().Call(&h, sizeof(h), SC_CLOSE, 10000);
    Bridge::Get().SetSink(nullptr);
    if (op) {
        op->Shutdown();
    }
    LOGK("camera %d closed", idx_);
    return CAM_OK;
}

void CameraDeviceImpl::OnFrame(const sc_frame &f)
{
    sptr<StreamOperatorImpl> op;
    {
        std::lock_guard<std::mutex> l(lock_);
        op = op_;
    }
    if (op && !closed_) {
        op->QueueFrame(f);
    } else {
        sc_release rl = { { SC_RELEASE, sizeof(sc_release) }, f.sid, f.idx };
        Bridge::Get().Send(&rl, sizeof(rl));
    }
}

void CameraDeviceImpl::OnShutter(const sc_shutter &s)
{
    sptr<StreamOperatorImpl> op;
    {
        std::lock_guard<std::mutex> l(lock_);
        op = op_;
    }
    if (op) {
        op->Shutter(s);
    }
}

void CameraDeviceImpl::OnResult(const sc_result &r)
{
    if (cb_ == nullptr || closed_ || !(r.have & 1)) {
        return;
    }
    uint8_t af = 0xff, ae = 0xff;
    switch (r.af_state) {
        case 1: case 3: af = OHOS_CAMERA_FOCUS_STATE_SCAN; break;          /* passive/active scan */
        case 2: case 4: af = OHOS_CAMERA_FOCUS_STATE_FOCUSED; break;       /* passive focused, focused locked */
        case 5: case 6: af = OHOS_CAMERA_FOCUS_STATE_UNFOCUSED; break;
        default: break;
    }
    ae = (r.ae_state == 1 || r.ae_state == 5) ? OHOS_CAMERA_EXPOSURE_STATE_SCAN : OHOS_CAMERA_EXPOSURE_STATE_CONVERGED;
    bool changed = af != lastAf_ || ae != lastAe_;
    if (!changed && resultMode_ == ON_CHANGED) {
        return;
    }
    if (!changed && (resultCount_++ % 10) != 0) {
        return;
    }
    lastAf_ = af;
    lastAe_ = ae;
    auto m = std::make_shared<CameraMetadata>(16, 256);
    if (af != 0xff) {
        m->addEntry(OHOS_CONTROL_FOCUS_STATE, &af, 1);
    }
    m->addEntry(OHOS_CONTROL_EXPOSURE_STATE, &ae, 1);
    float zoom;
    {
        std::lock_guard<std::mutex> g(ctrlLock_);
        zoom = ctrl_.c.zoom < 1.0f ? 1.0f : ctrl_.c.zoom;
    }
    m->addEntry(OHOS_CONTROL_ZOOM_RATIO, &zoom, 1);
    if (r.have & 2) {
        int64_t exp = r.exposure_ns;
        m->addEntry(OHOS_SENSOR_EXPOSURE_TIME, &exp, 1);
    }
    int64_t ts = r.ts;
    m->addEntry(OHOS_SENSOR_INFO_TIMESTAMP, &ts, 1);
    std::vector<uint8_t> vec;
    OHOS::Camera::MetadataUtils::ConvertMetadataToVec(m, vec);
    cb_->OnResult((uint64_t)r.ts, vec);
}

void CameraDeviceImpl::OnError(const sc_error &e)
{
    if (e.code == 1 && cb_) { /* device error */
        cb_->OnError(DRIVER_ERROR, e.code);
    }
}

void CameraDeviceImpl::OnCaptureDone(const sc_capture_done &d)
{
    sptr<StreamOperatorImpl> op;
    {
        std::lock_guard<std::mutex> l(lock_);
        op = op_;
    }
    if (op) {
        /* not on the reader thread: it waits for the worker */
        std::thread([op, d] { op->CaptureDone(d); }).detach();
    }
}

void CameraDeviceImpl::OnDisconnect()
{
    sptr<StreamOperatorImpl> op;
    {
        std::lock_guard<std::mutex> l(lock_);
        op = op_;
    }
    if (op) {
        op->Disconnected();
    }
    if (cb_ && !closed_) {
        cb_->OnError(DEVICE_DISCONNECT, 0);
    }
}

/* ---------------------------------------------------------- host */
class CameraHostImpl : public ICameraHost {
public:
    CameraHostImpl()
    {
        auto &cams = Bridge::Get().Cams();
        for (size_t i = 0; i < cams.size(); i++) {
            char id[16];
            snprintf(id, sizeof(id), "lcam%03zu", i + 1);
            ids_.push_back(id);
            std::vector<uint8_t> vec;
            OHOS::Camera::MetadataUtils::ConvertMetadataToVec(BuildAbility(cams[i], cams), vec);
            abilities_.push_back(vec);
            LOGK("%s: HAL camera %d, %s, %d degrees, %d preview / %d photo sizes, ability %zu bytes", id,
                cams[i].hal_id, cams[i].facing == 1 ? "front" : "back", cams[i].orientation, cams[i].n_yuv,
                cams[i].n_jpeg, vec.size());
        }
    }

    int32_t SetCallback(const sptr<ICameraHostCallback> &callbackObj) override
    {
        cb_ = callbackObj;
        return CAM_OK;
    }

    int32_t GetCameraIds(std::vector<std::string> &cameraIds) override
    {
        cameraIds = ids_;
        return CAM_OK;
    }

    int32_t GetCameraAbility(const std::string &cameraId, std::vector<uint8_t> &cameraAbility) override
    {
        int i = Index(cameraId);
        if (i < 0) {
            return INVALID_ARGUMENT;
        }
        cameraAbility = abilities_[i];
        return CAM_OK;
    }

    int32_t OpenCamera(const std::string &cameraId, const sptr<ICameraDeviceCallback> &callbackObj,
        sptr<ICameraDevice> &device) override
    {
        int i = Index(cameraId);
        if (i < 0) {
            return INVALID_ARGUMENT;
        }
        std::lock_guard<std::mutex> l(lock_);
        if (open_ != nullptr && !open_->Closed()) {
            LOGK("OpenCamera %s: closing the camera still open", cameraId.c_str());
            open_->Close();
        }
        if (!Bridge::Get().Connected()) {
            Bridge::Get().Connect(5);
        }
        sptr<CameraDeviceImpl> dev = new CameraDeviceImpl(i, Bridge::Get().Cams()[i], callbackObj);
        if (!dev->Open()) {
            return DEVICE_ERROR;
        }
        open_ = dev;
        device = dev;
        return CAM_OK;
    }

    int32_t SetFlashlight(const std::string &cameraId, bool isEnable) override
    {
        int i = Index(cameraId);
        if (i < 0) {
            return INVALID_ARGUMENT;
        }
        sc_torch t = { { SC_TORCH, sizeof(sc_torch) }, i, isEnable ? 1 : 0 };
        int r = Bridge::Get().Call(&t, sizeof(t), SC_TORCH, 5000);
        if (cb_) {
            cb_->OnFlashlightStatus(cameraId, r == 0 ? (isEnable ? FLASHLIGHT_ON : FLASHLIGHT_OFF) : FLASHLIGHT_UNAVAILABLE);
        }
        return r == 0 ? CAM_OK : METHOD_NOT_SUPPORTED;
    }

private:
    int Index(const std::string &id)
    {
        for (size_t i = 0; i < ids_.size(); i++) {
            if (ids_[i] == id) {
                return (int)i;
            }
        }
        return -1;
    }

    std::vector<std::string> ids_;
    std::vector<std::vector<uint8_t>> abilities_;
    sptr<ICameraHostCallback> cb_;
    std::mutex lock_;
    sptr<CameraDeviceImpl> open_;
};

/* ------------------------------------------------- service registration */
static bool RegisterService(const sptr<IRemoteObject> &stub)
{
    using OHOS::HDI::ServiceManager::V1_0::IServiceManager;
    for (int attempt = 0; attempt < 60; attempt++) {
        sptr<IServiceManager> sm = IServiceManager::Get();
        sptr<IRemoteObject> remote = sm ? OHOS::HDI::hdi_objcast<IServiceManager>(sm) : nullptr;
        if (remote != nullptr) {
            MessageParcel data, reply;
            MessageOption option;
            data.WriteInterfaceToken(u"HDI.IServiceManager.V1_0");
            data.WriteCString("camera_service");
            data.WriteUint16(0x1 << 6);           /* DEVICE_CLASS_CAMERA */
            data.WriteUint32(0);                  /* devId */
            data.WriteRemoteObject(stub);
            data.WriteCString("");
            data.WriteCString("ohos.hdi.camera.v1_0.ICameraHost");
            int r = remote->SendRequest(1 /* DEVSVC_MANAGER_ADD_SERVICE */, data, reply, option);
            if (r == 0) {
                LOGK("camera_service registered with the HDF service manager");
                return true;
            }
            LOGK("AddService(camera_service) = %d", r);
        }
        sleep(2);
    }
    return false;
}

} // namespace sweetcam

using namespace sweetcam;

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    LOGK("starting");
    Bridge &b = Bridge::Get();
    /* cambridge loads the Android HAL first: wait for it */
    for (int i = 0; !b.Connect(30) || !b.Hello(20000); i++) {
        LOGK("waiting for cambridge (%d)", i);
        if (i > 20) {
            break;
        }
    }
    if (b.Info().status != 0) {
        LOGK("cambridge: Android camera HAL not available: %s", b.Info().msg);
    }
    LOGK("%d cameras from the Android HAL", (int)b.Cams().size());

    sptr<ICameraHost> host = new CameraHostImpl();
    sptr<IRemoteObject> stub =
        OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(host, ICameraHost::GetDescriptor());
    if (stub == nullptr) {
        LOGK("cannot create the ICameraHost stub");
        return 1;
    }
    IPCSkeleton::SetMaxWorkThreadNum(8);
    if (!RegisterService(stub)) {
        LOGK("registration failed");
        return 1;
    }
    IPCSkeleton::JoinWorkThread();
    return 0;
}
