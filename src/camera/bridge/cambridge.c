/*
 * cambridge: hosts sweet's Android camera HAL (Qualcomm CamX camera.qcom.so,
 * camera3 / HAL module API 2.x) for HarmonyOS.
 *
 * Runs (bionic, Android linker) inside the private mount namespace prepared
 * by sweetcam-launch, where /system, /vendor and /apex are the Android ones.
 * The HarmonyOS camera host (sweet_camera_host) connects to the abstract
 * unix socket "sweetcam" (see sweetcam_proto.h): it gets a description of
 * every camera, configures streams, starts the repeating preview request and
 * single captures. Output buffers are allocated here with the Qualcomm
 * gralloc and handed to the host once (dma-buf fds); per frame only small
 * messages go over the socket.
 *
 * License: Apache-2.0
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <hardware/camera3.h>
#include <hardware/camera_common.h>
#include <hardware/gralloc1.h>
#include <hardware/hardware.h>
#include <system/camera_metadata.h>
#include <system/graphics.h>

#include "../sweetcam_proto.h"

/* ------------------------------------------------------------------ log */
static FILE *g_log;
static int g_kmsg = -1;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

static void vlogf(int important, const char *fmt, va_list ap)
{
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    pthread_mutex_lock(&g_log_lock);
    if (g_log) {
        fprintf(g_log, "[%5ld.%03ld] %s\n", (long)ts.tv_sec, ts.tv_nsec / 1000000, buf);
        fflush(g_log);
    }
    if (important && g_kmsg >= 0) {
        char k[1100];
        int n = snprintf(k, sizeof(k), "<6>cambridge: %s\n", buf);
        write(g_kmsg, k, n);
    }
    pthread_mutex_unlock(&g_log_lock);
}

static void LOGI(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlogf(0, fmt, ap); va_end(ap); }
static void LOGK(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlogf(1, fmt, ap); va_end(ap); }

/* liblog redirection: everything the HAL logs goes to our file */
struct alog_message {
    size_t struct_size;
    int32_t buffer_id;
    int32_t priority;
    const char *tag;
    const char *file;
    uint32_t line;
    const char *message;
};

static int g_min_prio = 5; /* ANDROID_LOG_WARN */

static void alog_logger(const struct alog_message *m)
{
    if (!g_log || !m || m->priority < g_min_prio)
        return;
    static const char lv[] = "??VDIWEF";
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    pthread_mutex_lock(&g_log_lock);
    fprintf(g_log, "[%5ld.%03ld] %c/%s: %s\n", (long)ts.tv_sec, ts.tv_nsec / 1000000,
        lv[(m->priority >= 0 && m->priority < 8) ? m->priority : 0], m->tag ? m->tag : "", m->message ? m->message : "");
    fflush(g_log);
    pthread_mutex_unlock(&g_log_lock);
}

static void setup_logging(void)
{
    const char *dir = "/data/log/sweetcam";
    mkdir(dir, 0770);
    char path[128], old[128];
    snprintf(path, sizeof(path), "%s/cambridge.log", dir);
    snprintf(old, sizeof(old), "%s/cambridge.old.log", dir);
    rename(path, old);
    g_log = fopen(path, "w");
    g_kmsg = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    const char *p = getenv("SWEETCAM_LOGLEVEL");
    if (p)
        g_min_prio = atoi(p);
    void *ll = dlopen("liblog.so", RTLD_NOW);
    if (ll) {
        void (*set_logger)(void (*)(const struct alog_message *)) = dlsym(ll, "__android_log_set_logger");
        if (set_logger)
            set_logger(alog_logger);
        else
            LOGI("liblog has no __android_log_set_logger");
    }
}

/* ------------------------------------------------------------- gralloc1 */
static gralloc1_device_t *g_gr;
static GRALLOC1_PFN_CREATE_DESCRIPTOR gr_create_desc;
static GRALLOC1_PFN_DESTROY_DESCRIPTOR gr_destroy_desc;
static GRALLOC1_PFN_SET_DIMENSIONS gr_set_dim;
static GRALLOC1_PFN_SET_FORMAT gr_set_format;
static GRALLOC1_PFN_SET_PRODUCER_USAGE gr_set_prod;
static GRALLOC1_PFN_SET_CONSUMER_USAGE gr_set_cons;
static GRALLOC1_PFN_ALLOCATE gr_alloc;
static GRALLOC1_PFN_RELEASE gr_release;
static GRALLOC1_PFN_LOCK_FLEX gr_lock_flex;
static GRALLOC1_PFN_UNLOCK gr_unlock;
static GRALLOC1_PFN_GET_STRIDE gr_get_stride;

static const hw_module_t *load_hmi(const char *path)
{
    void *h = dlopen(path, RTLD_NOW);
    if (!h) {
        LOGK("dlopen %s: %s", path, dlerror());
        return NULL;
    }
    hw_module_t *m = (hw_module_t *)dlsym(h, HAL_MODULE_INFO_SYM_AS_STR);
    if (!m) {
        LOGK("%s has no HMI", path);
        return NULL;
    }
    m->dso = h;
    return m;
}

static int gralloc_init(void)
{
    const hw_module_t *m = load_hmi("/vendor/lib64/hw/gralloc.qcom.so");
    if (!m)
        return -1;
    hw_device_t *dev = NULL;
    int r = m->methods->open(m, GRALLOC_HARDWARE_MODULE_ID, &dev);
    if (r || !dev) {
        LOGK("gralloc open: %d", r);
        return -1;
    }
    g_gr = (gralloc1_device_t *)dev;
#define GF(var, id) var = (void *)g_gr->getFunction(g_gr, id)
    GF(gr_create_desc, GRALLOC1_FUNCTION_CREATE_DESCRIPTOR);
    GF(gr_destroy_desc, GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR);
    GF(gr_set_dim, GRALLOC1_FUNCTION_SET_DIMENSIONS);
    GF(gr_set_format, GRALLOC1_FUNCTION_SET_FORMAT);
    GF(gr_set_prod, GRALLOC1_FUNCTION_SET_PRODUCER_USAGE);
    GF(gr_set_cons, GRALLOC1_FUNCTION_SET_CONSUMER_USAGE);
    GF(gr_alloc, GRALLOC1_FUNCTION_ALLOCATE);
    GF(gr_release, GRALLOC1_FUNCTION_RELEASE);
    GF(gr_lock_flex, GRALLOC1_FUNCTION_LOCK_FLEX);
    GF(gr_unlock, GRALLOC1_FUNCTION_UNLOCK);
    GF(gr_get_stride, GRALLOC1_FUNCTION_GET_STRIDE);
#undef GF
    if (!gr_create_desc || !gr_set_dim || !gr_set_format || !gr_set_prod || !gr_set_cons || !gr_alloc ||
        !gr_release) {
        LOGK("gralloc1 functions missing");
        return -1;
    }
    return 0;
}

/* private_handle_t (QTI gralloc): fds then ints */
#define QTI_MAGIC 0x676d736d /* 'gmsm' */
struct qti_handle_view {
    int fd, fd_meta, magic, flags, width, height, uw, uh, format, buffer_type;
    unsigned layer_count;
    uint32_t id_lo, id_hi, usage_lo, usage_hi;
    unsigned size, offset;
};

static const struct qti_handle_view *qti_view(buffer_handle_t h)
{
    if (!h || h->numFds < 1 || h->numInts < 15)
        return NULL;
    const struct qti_handle_view *v = (const struct qti_handle_view *)&h->data[0];
    return v->magic == QTI_MAGIC ? v : NULL;
}

/* ------------------------------------------------------------ the HAL */
static camera_module_t *g_mod;
static int g_ncam;
static int g_hal_ids[SC_MAX_CAMS];
static struct sc_caminfo g_info[SC_MAX_CAMS];
static char g_hal_err[128];

static void cb_device_status(const struct camera_module_callbacks *c, int id, int st)
{
    LOGI("camera %d device status %d", id, st);
}

static void cb_torch_status(const struct camera_module_callbacks *c, const char *id, int st)
{
    LOGI("camera %s torch status %d", id, st);
}

static camera_module_callbacks_t g_module_cbs = { cb_device_status, cb_torch_status };

static int ro_find(const camera_metadata_t *m, uint32_t tag, camera_metadata_ro_entry_t *e)
{
    return find_camera_metadata_ro_entry(m, tag, e) == 0 && e->count > 0;
}

static void fill_info(int idx, int hal_id, const struct camera_info *ci)
{
    struct sc_caminfo *I = &g_info[idx];
    memset(I, 0, sizeof(*I));
    I->hdr.type = SC_CAMINFO;
    I->hdr.len = sizeof(*I);
    I->index = idx;
    I->hal_id = hal_id;
    I->facing = ci->facing == CAMERA_FACING_FRONT ? 1 : (ci->facing == CAMERA_FACING_BACK ? 0 : 2);
    I->orientation = ci->orientation;
    const camera_metadata_t *m = ci->static_camera_characteristics;
    camera_metadata_ro_entry_t e;
    if (!m)
        return;
    if (ro_find(m, ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL, &e))
        I->hw_level = e.data.u8[0];
    if (ro_find(m, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &e)) {
        for (size_t i = 0; i + 3 < e.count; i += 4) {
            int32_t fmt = e.data.i32[i], w = e.data.i32[i + 1], h = e.data.i32[i + 2], out = e.data.i32[i + 3];
            if (out != ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT)
                continue;
            if (fmt == HAL_PIXEL_FORMAT_YCbCr_420_888 && I->n_yuv < SC_MAX_SIZES) {
                I->yuv[I->n_yuv].w = w;
                I->yuv[I->n_yuv++].h = h;
            } else if (fmt == HAL_PIXEL_FORMAT_BLOB && I->n_jpeg < SC_MAX_SIZES) {
                I->jpeg[I->n_jpeg].w = w;
                I->jpeg[I->n_jpeg++].h = h;
            }
        }
    }
    if (ro_find(m, ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, &e)) {
        for (size_t i = 0; i + 3 < e.count; i += 4) {
            int64_t fmt = e.data.i64[i], w = e.data.i64[i + 1], h = e.data.i64[i + 2], d = e.data.i64[i + 3];
            struct sc_size *arr = fmt == HAL_PIXEL_FORMAT_YCbCr_420_888 ? I->yuv : fmt == HAL_PIXEL_FORMAT_BLOB ? I->jpeg : NULL;
            int n = fmt == HAL_PIXEL_FORMAT_YCbCr_420_888 ? I->n_yuv : I->n_jpeg;
            for (int k = 0; arr && k < n; k++)
                if (arr[k].w == w && arr[k].h == h)
                    arr[k].min_duration = d;
        }
    }
    if (ro_find(m, ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, &e))
        for (size_t i = 0; i + 1 < e.count && I->n_fps < 16; i += 2) {
            I->fps[I->n_fps][0] = e.data.i32[i];
            I->fps[I->n_fps++][1] = e.data.i32[i + 1];
        }
    if (ro_find(m, ANDROID_CONTROL_AE_COMPENSATION_RANGE, &e) && e.count >= 2) {
        I->ae_comp_min = e.data.i32[0];
        I->ae_comp_max = e.data.i32[1];
    }
    if (ro_find(m, ANDROID_CONTROL_AE_COMPENSATION_STEP, &e)) {
        I->ae_comp_step_num = e.data.r[0].numerator;
        I->ae_comp_step_den = e.data.r[0].denominator;
    }
    if (ro_find(m, ANDROID_FLASH_INFO_AVAILABLE, &e))
        I->flash = e.data.u8[0];
#define COPY_U8(tag, n, arr) \
    if (ro_find(m, tag, &e)) \
        for (size_t i = 0; i < e.count && n < sizeof(arr); i++) \
            arr[n++] = e.data.u8[i];
    COPY_U8(ANDROID_CONTROL_AF_AVAILABLE_MODES, I->n_af, I->af_modes);
    COPY_U8(ANDROID_CONTROL_AWB_AVAILABLE_MODES, I->n_awb, I->awb_modes);
    COPY_U8(ANDROID_CONTROL_AE_AVAILABLE_MODES, I->n_ae, I->ae_modes);
    COPY_U8(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES, I->n_stab, I->stab_modes);
    COPY_U8(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION, I->n_ois, I->ois_modes);
    COPY_U8(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES, I->n_face, I->face_modes);
    COPY_U8(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, I->n_scene, I->scene_modes);
#undef COPY_U8
    if (ro_find(m, ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, &e))
        I->max_faces = e.data.i32[0];
    if (ro_find(m, ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, &e))
        I->max_digital_zoom = e.data.f[0];
#ifdef ANDROID_CONTROL_ZOOM_RATIO_RANGE
    if (ro_find(m, ANDROID_CONTROL_ZOOM_RATIO_RANGE, &e) && e.count >= 2) {
        I->zoom_ratio_min = e.data.f[0];
        I->zoom_ratio_max = e.data.f[1];
    }
#endif
    if (ro_find(m, ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, &e) && e.count >= 4)
        memcpy(I->active, e.data.i32, sizeof(I->active));
    if (ro_find(m, ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, &e) && e.count >= 2)
        memcpy(I->pixel_array, e.data.i32, sizeof(I->pixel_array));
    if (ro_find(m, ANDROID_SENSOR_INFO_PHYSICAL_SIZE, &e) && e.count >= 2)
        memcpy(I->phys_size, e.data.f, sizeof(I->phys_size));
    if (ro_find(m, ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &e))
        I->focal_length = e.data.f[0];
    if (ro_find(m, ANDROID_LENS_INFO_AVAILABLE_APERTURES, &e))
        I->aperture = e.data.f[0];
    if (ro_find(m, ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &e))
        I->min_focus_dist = e.data.f[0];
    if (ro_find(m, ANDROID_SENSOR_INFO_SENSITIVITY_RANGE, &e) && e.count >= 2)
        memcpy(I->iso_range, e.data.i32, sizeof(I->iso_range));
    if (ro_find(m, ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, &e) && e.count >= 2)
        memcpy(I->exposure_range, e.data.i64, sizeof(I->exposure_range));
    if (ro_find(m, ANDROID_JPEG_MAX_SIZE, &e))
        I->jpeg_max_size = e.data.i32[0];
    if (ro_find(m, ANDROID_CONTROL_MAX_REGIONS, &e) && e.count >= 3) {
        I->max_regions_ae = e.data.i32[0];
        I->max_regions_awb = e.data.i32[1];
        I->max_regions_af = e.data.i32[2];
    }
    if (ro_find(m, ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &e))
        I->partial_results = e.data.i32[0];
    if (ro_find(m, ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES, &e))
        for (size_t i = 0; i + 1 < e.count && I->n_thumb < 8; i += 2) {
            I->thumb[I->n_thumb][0] = e.data.i32[i];
            I->thumb[I->n_thumb++][1] = e.data.i32[i + 1];
        }
    if (ro_find(m, ANDROID_REQUEST_AVAILABLE_CAPABILITIES, &e))
        for (size_t i = 0; i < e.count; i++)
            if (e.data.u8[i] == ANDROID_REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA)
                I->logical = 1;
}

static int hal_init(void)
{
    const hw_module_t *m = load_hmi("/vendor/lib64/hw/camera.qcom.so");
    if (!m) {
        snprintf(g_hal_err, sizeof(g_hal_err), "camera.qcom.so: %s", dlerror() ? dlerror() : "no HMI");
        return -1;
    }
    g_mod = (camera_module_t *)m;
    LOGK("camera module '%s' by '%s', api %d.%d", m->name, m->author, m->module_api_version >> 8,
        m->module_api_version & 0xff);
    if (g_mod->get_vendor_tag_ops) {
        static vendor_tag_ops_t ops;
        memset(&ops, 0, sizeof(ops));
        g_mod->get_vendor_tag_ops(&ops);
        if (ops.get_tag_count)
            set_camera_metadata_vendor_ops(&ops);
    }
    if (m->module_api_version >= CAMERA_MODULE_API_VERSION_2_4 && g_mod->init) {
        int r = g_mod->init();
        if (r) {
            LOGK("module init failed %d", r);
        }
    }
    if (g_mod->set_callbacks)
        g_mod->set_callbacks(&g_module_cbs);
    int n = g_mod->get_number_of_cameras();
    LOGK("%d cameras", n);
    g_ncam = 0;
    for (int id = 0; id < n && g_ncam < SC_MAX_CAMS; id++) {
        struct camera_info ci;
        memset(&ci, 0, sizeof(ci));
        int r = g_mod->get_camera_info(id, &ci);
        if (r) {
            LOGK("get_camera_info(%d) = %d", id, r);
            continue;
        }
        fill_info(g_ncam, id, &ci);
        struct sc_caminfo *I = &g_info[g_ncam];
        LOGK("cam %d: facing %d orient %d level %d dev 0x%x yuv %d jpeg %d (max %dx%d) focal %.2f logical %d",
            id, ci.facing, ci.orientation, I->hw_level, ci.device_version, I->n_yuv, I->n_jpeg,
            I->n_jpeg ? I->jpeg[0].w : 0, I->n_jpeg ? I->jpeg[0].h : 0, I->focal_length, I->logical);
        if (ci.device_version < CAMERA_DEVICE_API_VERSION_3_2 || I->n_yuv == 0)
            continue;
        g_hal_ids[g_ncam++] = id;
    }
    return 0;
}

/* ---------------------------------------------------------- the device */
enum { B_FREE = 0, B_HAL, B_HOST };

struct sbuf {
    buffer_handle_t h;
    int state;
};

struct sstream {
    int used;
    struct sc_stream_cfg cfg;
    camera3_stream_t st;
    int nbuf;
    uint32_t blob_size;
    struct sbuf b[SC_MAX_BUFS];
};

struct inflight {
    uint32_t frame;
    int32_t tag;
    int is_shot;
    int pending_bufs;
    int got_result;
    int failed;
    int64_t ts;
};

#define MAX_INFLIGHT 32

static camera3_device_t *g_dev;
static int g_dev_idx = -1;
static struct sstream g_st[SC_MAX_STREAMS];
static int g_nst;
static int g_video_mode;
static camera_metadata_t *g_tmpl_preview, *g_tmpl_still, *g_tmpl_video;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static uint32_t g_frame_no;
static struct inflight g_infl[MAX_INFLIGHT];
static int g_ninfl;

/* request state */
static int g_repeat_on;
static struct sc_request g_repeat;
#define SHOTQ 8
static struct sc_request g_shots[SHOTQ];
static int g_nshots;
static int g_req_thread_run;
static pthread_t g_req_thread;
static int g_configured;

static int g_sock = -1;
static pthread_mutex_t g_sock_lock = PTHREAD_MUTEX_INITIALIZER;

static int send_msg(const void *msg, size_t len, const int *fds, int nfds)
{
    struct iovec iov = { (void *)msg, len };
    char cbuf[CMSG_SPACE(sizeof(int) * SC_MAX_BUFS)];
    struct msghdr mh;
    memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    if (nfds > 0) {
        memset(cbuf, 0, sizeof(cbuf));
        mh.msg_control = cbuf;
        mh.msg_controllen = CMSG_SPACE(sizeof(int) * nfds);
        struct cmsghdr *c = CMSG_FIRSTHDR(&mh);
        c->cmsg_level = SOL_SOCKET;
        c->cmsg_type = SCM_RIGHTS;
        c->cmsg_len = CMSG_LEN(sizeof(int) * nfds);
        memcpy(CMSG_DATA(c), fds, sizeof(int) * nfds);
    }
    pthread_mutex_lock(&g_sock_lock);
    int s = g_sock;
    ssize_t r = s >= 0 ? sendmsg(s, &mh, MSG_NOSIGNAL) : -1;
    pthread_mutex_unlock(&g_sock_lock);
    return r == (ssize_t)len ? 0 : -1;
}

static void reply(int for_type, int status, const char *msg)
{
    struct sc_reply r;
    memset(&r, 0, sizeof(r));
    r.hdr.type = SC_REPLY;
    r.hdr.len = sizeof(r);
    r.for_type = for_type;
    r.status = status;
    if (msg)
        snprintf(r.msg, sizeof(r.msg), "%s", msg);
    send_msg(&r, sizeof(r), NULL, 0);
}

static struct inflight *infl_find(uint32_t frame)
{
    for (int i = 0; i < g_ninfl; i++)
        if (g_infl[i].frame == frame)
            return &g_infl[i];
    return NULL;
}

static void infl_maybe_done(struct inflight *f)
{
    if (f->pending_bufs > 0 || !f->got_result)
        return;
    if (f->is_shot) {
        struct sc_capture_done d = { { SC_CAPTURE_DONE, sizeof(d) }, f->tag, f->frame, !f->failed };
        send_msg(&d, sizeof(d), NULL, 0);
    }
    int i = (int)(f - g_infl);
    g_infl[i] = g_infl[--g_ninfl];
    pthread_cond_broadcast(&g_cond);
}

static int find_buffer(buffer_handle_t *bh, int *si, int *bi)
{
    for (int s = 0; s < g_nst; s++)
        for (int b = 0; b < g_st[s].nbuf; b++)
            if (&g_st[s].b[b].h == bh || g_st[s].b[b].h == *bh) {
                *si = s;
                *bi = b;
                return 0;
            }
    return -1;
}

static int wait_fence(int fd, int ms)
{
    if (fd < 0)
        return 0;
    struct pollfd p = { fd, POLLIN, 0 };
    int r = poll(&p, 1, ms);
    close(fd);
    return r > 0 ? 0 : -1;
}

static void parse_result(const camera_metadata_t *m, struct sc_result *R)
{
    camera_metadata_ro_entry_t e;
    if (ro_find(m, ANDROID_CONTROL_AF_STATE, &e)) {
        R->af_state = e.data.u8[0];
        R->have |= 1;
    }
    if (ro_find(m, ANDROID_CONTROL_AE_STATE, &e)) {
        R->ae_state = e.data.u8[0];
        R->have |= 1;
    }
    if (ro_find(m, ANDROID_CONTROL_AWB_STATE, &e)) {
        R->awb_state = e.data.u8[0];
        R->have |= 1;
    }
    if (ro_find(m, ANDROID_FLASH_STATE, &e))
        R->flash_state = e.data.u8[0];
    if (ro_find(m, ANDROID_CONTROL_AF_MODE, &e))
        R->af_mode = e.data.u8[0];
    if (ro_find(m, ANDROID_CONTROL_AE_MODE, &e))
        R->ae_mode = e.data.u8[0];
    if (ro_find(m, ANDROID_CONTROL_AWB_MODE, &e))
        R->awb_mode = e.data.u8[0];
    if (ro_find(m, ANDROID_SENSOR_EXPOSURE_TIME, &e)) {
        R->exposure_ns = e.data.i64[0];
        R->have |= 2;
    }
    if (ro_find(m, ANDROID_SENSOR_SENSITIVITY, &e)) {
        R->iso = e.data.i32[0];
        R->have |= 2;
    }
    if (ro_find(m, ANDROID_SENSOR_TIMESTAMP, &e))
        R->ts = e.data.i64[0];
    if (ro_find(m, ANDROID_LENS_FOCUS_DISTANCE, &e)) {
        R->focus_distance = e.data.f[0];
        R->have |= 8;
    }
    if (ro_find(m, ANDROID_SCALER_CROP_REGION, &e) && e.count >= 4)
        memcpy(R->crop, e.data.i32, sizeof(R->crop));
    if (ro_find(m, ANDROID_STATISTICS_FACE_RECTANGLES, &e)) {
        int n = (int)(e.count / 4);
        if (n > SC_MAX_FACES)
            n = SC_MAX_FACES;
        for (int i = 0; i < n; i++)
            memcpy(R->faces[i].rect, &e.data.i32[i * 4], sizeof(R->faces[i].rect));
        R->n_faces = (uint8_t)n;
        camera_metadata_ro_entry_t s;
        if (ro_find(m, ANDROID_STATISTICS_FACE_SCORES, &s))
            for (int i = 0; i < n && i < (int)s.count; i++)
                R->faces[i].score = s.data.u8[i];
        R->have |= 4;
    }
}

static void cb_process_capture_result(const struct camera3_callback_ops *ops, const camera3_capture_result_t *r)
{
    pthread_mutex_lock(&g_lock);
    struct inflight *f = infl_find(r->frame_number);
    int32_t tag = f ? f->tag : -1;
    if (r->result) {
        struct sc_result R;
        memset(&R, 0, sizeof(R));
        R.hdr.type = SC_RESULT;
        R.hdr.len = sizeof(R);
        R.tag = tag;
        R.frame = r->frame_number;
        parse_result(r->result, &R);
        if (R.ts && f)
            f->ts = R.ts;
        if (!R.ts && f)
            R.ts = f->ts;
        if (R.have)
            send_msg(&R, sizeof(R), NULL, 0);
        int total = g_dev_idx >= 0 && g_info[g_dev_idx].partial_results > 0 ? g_info[g_dev_idx].partial_results : 1;
        if (f && (int)r->partial_result >= total)
            f->got_result = 1;
    }
    for (uint32_t i = 0; i < r->num_output_buffers; i++) {
        const camera3_stream_buffer_t *sb = &r->output_buffers[i];
        int si, bi;
        if (find_buffer(sb->buffer, &si, &bi)) {
            LOGI("result: unknown buffer");
            continue;
        }
        int bad = sb->status != CAMERA3_BUFFER_STATUS_OK;
        if (wait_fence(sb->release_fence, 1000))
            bad = 1;
        struct sstream *S = &g_st[si];
        struct sc_frame F;
        memset(&F, 0, sizeof(F));
        F.hdr.type = SC_FRAME;
        F.hdr.len = sizeof(F);
        F.sid = S->cfg.sid;
        F.idx = bi;
        F.tag = tag;
        F.frame = r->frame_number;
        F.status = bad;
        F.ts = f ? f->ts : 0;
        if (!bad && S->cfg.kind == SC_KIND_STILL) {
            const struct qti_handle_view *v = qti_view(S->b[bi].h);
            /* the camera3_jpeg_blob trailer sits at the end of the buffer */
            uint32_t size = S->blob_size;
            if (v && v->size >= sizeof(struct camera3_jpeg_blob)) {
                void *p = mmap(NULL, v->size, PROT_READ, MAP_SHARED, v->fd, v->offset);
                if (p != MAP_FAILED) {
                    uint32_t len = size <= v->size ? size : v->size;
                    struct camera3_jpeg_blob blob;
                    memcpy(&blob, (char *)p + len - sizeof(blob), sizeof(blob));
                    if (blob.jpeg_blob_id == CAMERA3_JPEG_BLOB_ID && blob.jpeg_size <= len)
                        F.data_size = blob.jpeg_size;
                    else {
                        /* scan for the end of image marker */
                        const unsigned char *c = p;
                        for (uint32_t k = 2; k + 1 < len; k++)
                            if (c[k] == 0xff && c[k + 1] == 0xd9)
                                F.data_size = k + 2;
                    }
                    munmap(p, v->size);
                }
            }
            LOGK("jpeg frame %u: %u bytes", r->frame_number, F.data_size);
            if (!F.data_size)
                F.status = 1;
        }
        S->b[bi].state = bad ? B_FREE : B_HOST;
        if (bad && f)
            f->failed = 1;
        send_msg(&F, sizeof(F), NULL, 0);
        if (f)
            f->pending_bufs--;
    }
    if (f)
        infl_maybe_done(f);
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
}

static void cb_notify(const struct camera3_callback_ops *ops, const camera3_notify_msg_t *msg)
{
    pthread_mutex_lock(&g_lock);
    if (msg->type == CAMERA3_MSG_SHUTTER) {
        struct inflight *f = infl_find(msg->message.shutter.frame_number);
        if (f)
            f->ts = (int64_t)msg->message.shutter.timestamp;
        struct sc_shutter s = { { SC_SHUTTER, sizeof(s) }, f ? f->tag : -1, msg->message.shutter.frame_number,
            (int64_t)msg->message.shutter.timestamp };
        send_msg(&s, sizeof(s), NULL, 0);
    } else if (msg->type == CAMERA3_MSG_ERROR) {
        const camera3_error_msg_t *e = &msg->message.error;
        struct inflight *f = infl_find(e->frame_number);
        int sid = -1;
        for (int i = 0; i < g_nst; i++)
            if (e->error_stream == &g_st[i].st)
                sid = g_st[i].cfg.sid;
        struct sc_error E = { { SC_ERROR, sizeof(E) }, f ? f->tag : -1, e->frame_number, e->error_code, sid };
        LOGK("HAL error %d frame %u stream %d", e->error_code, e->frame_number, sid);
        send_msg(&E, sizeof(E), NULL, 0);
        if (f && (e->error_code == CAMERA3_MSG_ERROR_REQUEST || e->error_code == CAMERA3_MSG_ERROR_RESULT)) {
            f->got_result = 1;
            f->failed = 1;
            infl_maybe_done(f);
        }
    }
    pthread_mutex_unlock(&g_lock);
}

static camera3_callback_ops_t g_cb_ops = { cb_process_capture_result, cb_notify };

/* metadata helpers */
static void meta_set(camera_metadata_t **m, uint32_t tag, const void *data, size_t count)
{
    camera_metadata_entry_t e;
    if (find_camera_metadata_entry(*m, tag, &e) == 0) {
        if (update_camera_metadata_entry(*m, e.index, data, count, NULL) == 0)
            return;
    } else if (add_camera_metadata_entry(*m, tag, data, count) == 0) {
        return;
    }
    /* grow */
    size_t ec = get_camera_metadata_entry_capacity(*m) * 2 + 8;
    size_t dc = get_camera_metadata_data_capacity(*m) * 2 + count * 8 + 64;
    camera_metadata_t *n = allocate_camera_metadata(ec, dc);
    if (!n)
        return;
    append_camera_metadata(n, *m);
    free_camera_metadata(*m);
    *m = n;
    if (find_camera_metadata_entry(*m, tag, &e) == 0)
        update_camera_metadata_entry(*m, e.index, data, count, NULL);
    else
        add_camera_metadata_entry(*m, tag, data, count);
}

static void meta_u8(camera_metadata_t **m, uint32_t tag, uint8_t v) { meta_set(m, tag, &v, 1); }
static void meta_i32(camera_metadata_t **m, uint32_t tag, int32_t v) { meta_set(m, tag, &v, 1); }

static int has_u8(const uint8_t *a, int n, uint8_t v)
{
    for (int i = 0; i < n; i++)
        if (a[i] == v)
            return 1;
    return 0;
}

static camera_metadata_t *build_settings(const struct sc_ctrl *c, int shot)
{
    const camera_metadata_t *t = shot ? g_tmpl_still : (g_video_mode ? g_tmpl_video : g_tmpl_preview);
    if (!t)
        t = g_tmpl_preview;
    camera_metadata_t *m = allocate_camera_metadata(get_camera_metadata_entry_count(t) + 32,
        get_camera_metadata_data_count(t) + 1024);
    append_camera_metadata(m, t);
    const struct sc_caminfo *I = &g_info[g_dev_idx];

    /* zoom: crop region (and the zoom ratio when the HAL has it) */
    float z = c->zoom < 1.0f ? 1.0f : c->zoom;
    float zmax = I->max_digital_zoom > 1.0f ? I->max_digital_zoom : 1.0f;
    if (z > zmax)
        z = zmax;
    if (I->active[2] > 0) {
        int32_t w = (int32_t)(I->active[2] / z), h = (int32_t)(I->active[3] / z);
        int32_t crop[4] = { (I->active[2] - w) / 2, (I->active[3] - h) / 2, w, h };
        meta_set(&m, ANDROID_SCALER_CROP_REGION, crop, 4);
    }
    if (c->ae_comp >= I->ae_comp_min && c->ae_comp <= I->ae_comp_max)
        meta_i32(&m, ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, c->ae_comp);
    if (c->fps_max > 0) {
        int32_t r[2] = { c->fps_min, c->fps_max };
        /* use a range the HAL advertises */
        int best = -1;
        for (int i = 0; i < I->n_fps; i++)
            if (I->fps[i][1] == c->fps_max && (best < 0 || abs(I->fps[i][0] - c->fps_min) < abs(I->fps[best][0] - c->fps_min)))
                best = i;
        if (best >= 0) {
            r[0] = I->fps[best][0];
            r[1] = I->fps[best][1];
            meta_set(&m, ANDROID_CONTROL_AE_TARGET_FPS_RANGE, r, 2);
        }
    }
    /* flash */
    uint8_t ae_mode = ANDROID_CONTROL_AE_MODE_ON, flash = ANDROID_FLASH_MODE_OFF;
    if (I->flash) {
        switch (c->flash_mode) {
        case 1: ae_mode = ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH; break;
        case 2: ae_mode = ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH; break;
        case 3: flash = ANDROID_FLASH_MODE_TORCH; break;
        default: break;
        }
        if (!has_u8(I->ae_modes, I->n_ae, ae_mode))
            ae_mode = ANDROID_CONTROL_AE_MODE_ON;
    }
    meta_u8(&m, ANDROID_CONTROL_AE_MODE, ae_mode);
    meta_u8(&m, ANDROID_FLASH_MODE, flash);
    meta_u8(&m, ANDROID_CONTROL_AE_LOCK, c->ae_lock ? ANDROID_CONTROL_AE_LOCK_ON : ANDROID_CONTROL_AE_LOCK_OFF);
    meta_u8(&m, ANDROID_CONTROL_AWB_LOCK, c->awb_lock ? ANDROID_CONTROL_AWB_LOCK_ON : ANDROID_CONTROL_AWB_LOCK_OFF);
    if (c->awb_mode && has_u8(I->awb_modes, I->n_awb, c->awb_mode))
        meta_u8(&m, ANDROID_CONTROL_AWB_MODE, c->awb_mode);
    /* focus */
    if (I->n_af > 1) {
        uint8_t af = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        if (c->af_mode == 1)
            af = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        else if (c->af_mode == 2)
            af = ANDROID_CONTROL_AF_MODE_AUTO;
        else if (c->af_mode == 3)
            af = ANDROID_CONTROL_AF_MODE_OFF;
        if (g_video_mode && af == ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE)
            af = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        if (!has_u8(I->af_modes, I->n_af, af))
            af = I->af_modes[I->n_af - 1];
        meta_u8(&m, ANDROID_CONTROL_AF_MODE, af);
        meta_u8(&m, ANDROID_CONTROL_AF_TRIGGER, c->af_trigger == 1 ? ANDROID_CONTROL_AF_TRIGGER_START :
            c->af_trigger == 2 ? ANDROID_CONTROL_AF_TRIGGER_CANCEL : ANDROID_CONTROL_AF_TRIGGER_IDLE);
        if (c->af_region[4] > 0 && I->max_regions_af > 0)
            meta_set(&m, ANDROID_CONTROL_AF_REGIONS, c->af_region, 5);
    }
    if (c->ae_region[4] > 0 && I->max_regions_ae > 0)
        meta_set(&m, ANDROID_CONTROL_AE_REGIONS, c->ae_region, 5);
    if (c->ae_precapture)
        meta_u8(&m, ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START);
    if (I->n_stab > 1)
        meta_u8(&m, ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
            c->video_stab ? ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON : ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF);
    if (I->n_face > 1)
        meta_u8(&m, ANDROID_STATISTICS_FACE_DETECT_MODE,
            c->face_detect ? I->face_modes[I->n_face - 1] : ANDROID_STATISTICS_FACE_DETECT_MODE_OFF);
    if (c->scene_mode > 0 && has_u8(I->scene_modes, I->n_scene, (uint8_t)c->scene_mode)) {
        meta_u8(&m, ANDROID_CONTROL_MODE, ANDROID_CONTROL_MODE_USE_SCENE_MODE);
        meta_u8(&m, ANDROID_CONTROL_SCENE_MODE, (uint8_t)c->scene_mode);
    }
    if (shot) {
        meta_i32(&m, ANDROID_JPEG_ORIENTATION, c->jpeg_orientation);
        meta_u8(&m, ANDROID_JPEG_QUALITY, c->jpeg_quality ? c->jpeg_quality : 95);
        if (I->n_thumb > 1) {
            int32_t th[2] = { I->thumb[I->n_thumb - 1][0], I->thumb[I->n_thumb - 1][1] };
            meta_set(&m, ANDROID_JPEG_THUMBNAIL_SIZE, th, 2);
        }
        if (c->has_gps) {
            meta_set(&m, ANDROID_JPEG_GPS_COORDINATES, c->gps, 3);
            int64_t ts = c->gps_ts;
            meta_set(&m, ANDROID_JPEG_GPS_TIMESTAMP, &ts, 1);
            static const uint8_t method[] = "GPS";
            meta_set(&m, ANDROID_JPEG_GPS_PROCESSING_METHOD, method, sizeof(method));
        }
    }
    return m;
}

static int stream_index_by_mask_bit(int bit)
{
    return bit < g_nst ? bit : -1;
}

/* the request loop */
static void *request_thread(void *arg)
{
    pthread_mutex_lock(&g_lock);
    while (g_req_thread_run) {
        struct sc_request req;
        int shot = 0;
        if (g_nshots > 0) {
            req = g_shots[0];
            shot = 1;
        } else if (g_repeat_on) {
            req = g_repeat;
        } else {
            pthread_cond_wait(&g_cond, &g_lock);
            continue;
        }
        if (!g_configured || g_ninfl >= MAX_INFLIGHT - 1) {
            struct timespec to;
            clock_gettime(CLOCK_REALTIME, &to);
            to.tv_nsec += 20000000;
            if (to.tv_nsec >= 1000000000) { to.tv_sec++; to.tv_nsec -= 1000000000; }
            pthread_cond_timedwait(&g_cond, &g_lock, &to);
            continue;
        }
        /* one free buffer per stream of the mask */
        camera3_stream_buffer_t outs[SC_MAX_STREAMS];
        int n = 0, missing = 0;
        int picks[SC_MAX_STREAMS][2];
        for (int bit = 0; bit < SC_MAX_STREAMS; bit++) {
            if (!(req.mask & (1u << bit)))
                continue;
            int si = stream_index_by_mask_bit(bit);
            if (si < 0)
                continue;
            struct sstream *S = &g_st[si];
            int bi = -1;
            for (int b = 0; b < S->nbuf; b++)
                if (S->b[b].state == B_FREE) {
                    bi = b;
                    break;
                }
            if (bi < 0) {
                missing = 1;
                break;
            }
            picks[n][0] = si;
            picks[n][1] = bi;
            n++;
        }
        if (missing || n == 0) {
            struct timespec to;
            clock_gettime(CLOCK_REALTIME, &to);
            to.tv_nsec += 30000000;
            if (to.tv_nsec >= 1000000000) { to.tv_sec++; to.tv_nsec -= 1000000000; }
            pthread_cond_timedwait(&g_cond, &g_lock, &to);
            if (n == 0 && shot) {
                LOGK("shot %d: no stream in mask 0x%x", req.tag, req.mask);
                struct sc_capture_done d = { { SC_CAPTURE_DONE, sizeof(d) }, req.tag, 0, 0 };
                send_msg(&d, sizeof(d), NULL, 0);
                memmove(&g_shots[0], &g_shots[1], sizeof(g_shots[0]) * (--g_nshots));
            }
            continue;
        }
        for (int i = 0; i < n; i++) {
            struct sstream *S = &g_st[picks[i][0]];
            S->b[picks[i][1]].state = B_HAL;
            outs[i].stream = &S->st;
            outs[i].buffer = &S->b[picks[i][1]].h;
            outs[i].status = CAMERA3_BUFFER_STATUS_OK;
            outs[i].acquire_fence = -1;
            outs[i].release_fence = -1;
        }
        if (shot)
            memmove(&g_shots[0], &g_shots[1], sizeof(g_shots[0]) * (--g_nshots));
        else {
            /* one-shot triggers only go out once */
            g_repeat.c.af_trigger = 0;
            g_repeat.c.ae_precapture = 0;
        }
        uint32_t fn = g_frame_no++;
        struct inflight *f = &g_infl[g_ninfl++];
        memset(f, 0, sizeof(*f));
        f->frame = fn;
        f->tag = req.tag;
        f->is_shot = shot;
        f->pending_bufs = n;
        camera_metadata_t *settings = build_settings(&req.c, shot);
        camera3_capture_request_t cr;
        memset(&cr, 0, sizeof(cr));
        cr.frame_number = fn;
        cr.settings = settings;
        cr.num_output_buffers = (uint32_t)n;
        cr.output_buffers = outs;
        pthread_mutex_unlock(&g_lock);
        int r = g_dev->ops->process_capture_request(g_dev, &cr);
        pthread_mutex_lock(&g_lock);
        free_camera_metadata(settings);
        if (r) {
            LOGK("process_capture_request(%u) = %d", fn, r);
            for (int i = 0; i < n; i++)
                g_st[picks[i][0]].b[picks[i][1]].state = B_FREE;
            struct inflight *ff = infl_find(fn);
            if (ff) {
                if (ff->is_shot) {
                    struct sc_capture_done d = { { SC_CAPTURE_DONE, sizeof(d) }, ff->tag, fn, 0 };
                    send_msg(&d, sizeof(d), NULL, 0);
                }
                *ff = g_infl[--g_ninfl];
            }
            struct timespec to;
            clock_gettime(CLOCK_REALTIME, &to);
            to.tv_sec += 0;
            to.tv_nsec += 100000000;
            if (to.tv_nsec >= 1000000000) { to.tv_sec++; to.tv_nsec -= 1000000000; }
            pthread_cond_timedwait(&g_cond, &g_lock, &to);
        } else if (shot || fn < 3 || (fn % 300) == 0) {
            LOGI("request %u tag %d shot %d bufs %d", fn, req.tag, shot, n);
        }
    }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

static void stop_requests(void)
{
    pthread_mutex_lock(&g_lock);
    g_repeat_on = 0;
    g_nshots = 0;
    int run = g_req_thread_run;
    g_req_thread_run = 0;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
    if (run)
        pthread_join(g_req_thread, NULL);
    if (g_dev && g_dev->ops->flush)
        g_dev->ops->flush(g_dev);
    /* wait for the in-flight requests to drain */
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < 100 && g_ninfl > 0; i++) {
        struct timespec to;
        clock_gettime(CLOCK_REALTIME, &to);
        to.tv_nsec += 20000000;
        if (to.tv_nsec >= 1000000000) { to.tv_sec++; to.tv_nsec -= 1000000000; }
        pthread_cond_timedwait(&g_cond, &g_lock, &to);
    }
    if (g_ninfl)
        LOGK("%d requests still in flight after flush", g_ninfl);
    g_ninfl = 0;
    pthread_mutex_unlock(&g_lock);
}

static void start_request_thread(void)
{
    pthread_mutex_lock(&g_lock);
    if (!g_req_thread_run) {
        g_req_thread_run = 1;
        pthread_create(&g_req_thread, NULL, request_thread, NULL);
    }
    pthread_mutex_unlock(&g_lock);
}

static void free_streams(void)
{
    for (int s = 0; s < g_nst; s++) {
        for (int b = 0; b < g_st[s].nbuf; b++)
            if (g_st[s].b[b].h)
                gr_release(g_gr, g_st[s].b[b].h);
        memset(&g_st[s], 0, sizeof(g_st[s]));
    }
    g_nst = 0;
    g_configured = 0;
}

static void dev_close(void)
{
    if (!g_dev)
        return;
    stop_requests();
    g_dev->common.close(&g_dev->common);
    g_dev = NULL;
    free_streams();
    free_camera_metadata(g_tmpl_preview);
    free_camera_metadata(g_tmpl_still);
    free_camera_metadata(g_tmpl_video);
    g_tmpl_preview = g_tmpl_still = g_tmpl_video = NULL;
    LOGK("camera %d closed", g_dev_idx);
    g_dev_idx = -1;
}

static int dev_open(int idx)
{
    if (g_dev)
        dev_close();
    if (idx < 0 || idx >= g_ncam)
        return -EINVAL;
    char name[8];
    snprintf(name, sizeof(name), "%d", g_hal_ids[idx]);
    hw_device_t *d = NULL;
    int r = g_mod->common.methods->open(&g_mod->common, name, &d);
    if (r || !d) {
        LOGK("open camera %s: %d", name, r);
        return r ? r : -ENODEV;
    }
    g_dev = (camera3_device_t *)d;
    r = g_dev->ops->initialize(g_dev, &g_cb_ops);
    if (r) {
        LOGK("initialize camera %s: %d", name, r);
        g_dev->common.close(&g_dev->common);
        g_dev = NULL;
        return r;
    }
    g_dev_idx = idx;
    const camera_metadata_t *t;
    t = g_dev->ops->construct_default_request_settings(g_dev, CAMERA3_TEMPLATE_PREVIEW);
    g_tmpl_preview = t ? clone_camera_metadata(t) : allocate_camera_metadata(16, 256);
    t = g_dev->ops->construct_default_request_settings(g_dev, CAMERA3_TEMPLATE_STILL_CAPTURE);
    g_tmpl_still = t ? clone_camera_metadata(t) : clone_camera_metadata(g_tmpl_preview);
    t = g_dev->ops->construct_default_request_settings(g_dev, CAMERA3_TEMPLATE_VIDEO_RECORD);
    g_tmpl_video = t ? clone_camera_metadata(t) : clone_camera_metadata(g_tmpl_preview);
    g_frame_no = 0;
    LOGK("camera %s open (device api 0x%x)", name, d->version);
    return 0;
}

static int alloc_buffer(struct sstream *S, int b)
{
    gralloc1_buffer_descriptor_t desc;
    if (gr_create_desc(g_gr, &desc) != GRALLOC1_ERROR_NONE)
        return -1;
    int w = (int)S->st.width, h = (int)S->st.height;
    gr_set_dim(g_gr, desc, (uint32_t)w, (uint32_t)h);
    gr_set_format(g_gr, desc, S->st.format);
    gr_set_prod(g_gr, desc, GRALLOC1_PRODUCER_USAGE_CAMERA | (S->st.usage & 0xffffffffull));
    uint64_t cons = GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN;
    if (S->cfg.kind == SC_KIND_VIDEO)
        cons |= GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER;
    gr_set_cons(g_gr, desc, cons);
    buffer_handle_t h0 = NULL;
    int r = gr_alloc(g_gr, 1, &desc, &h0);
    gr_destroy_desc(g_gr, desc);
    if ((r != GRALLOC1_ERROR_NONE && r != GRALLOC1_ERROR_NOT_SHARED) || !h0) {
        LOGK("gralloc allocate %dx%d fmt 0x%x: %d", w, h, S->st.format, r);
        return -1;
    }
    S->b[b].h = h0;
    S->b[b].state = B_FREE;
    return 0;
}

static void buffer_layout(struct sstream *S, int b, struct sc_buflayout *L)
{
    memset(L, 0, sizeof(*L));
    const struct qti_handle_view *v = qti_view(S->b[b].h);
    if (!v)
        return;
    L->size = v->size;
    L->offset = v->offset;
    if (S->cfg.kind == SC_KIND_STILL)
        return;
    /* plane layout from lock_flex */
    struct android_flex_layout fl;
    memset(&fl, 0, sizeof(fl));
    gralloc1_rect_t rect = { 0, 0, (int32_t)S->st.width, (int32_t)S->st.height };
    struct android_flex_plane planes[4];
    fl.planes = planes;
    fl.num_planes = 4;
    int ok = 0;
    if (gr_lock_flex && gr_lock_flex(g_gr, S->b[b].h, GRALLOC1_PRODUCER_USAGE_NONE,
            GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN, &rect, &fl, -1) == GRALLOC1_ERROR_NONE) {
        uint8_t *y = NULL, *cb = NULL, *cr = NULL;
        int ys = 0, cs = 0, step = 2;
        for (uint32_t i = 0; i < fl.num_planes; i++) {
            if (fl.planes[i].component == FLEX_COMPONENT_Y) {
                y = fl.planes[i].top_left;
                ys = fl.planes[i].v_increment;
            } else if (fl.planes[i].component == FLEX_COMPONENT_Cb) {
                cb = fl.planes[i].top_left;
                cs = fl.planes[i].v_increment;
                step = fl.planes[i].h_increment;
            } else if (fl.planes[i].component == FLEX_COMPONENT_Cr) {
                cr = fl.planes[i].top_left;
            }
        }
        if (y && cb && cr) {
            uint8_t *base = y; /* y plane starts the buffer for every QTI YUV format */
            L->y_off = 0;
            L->y_stride = (uint32_t)ys;
            L->c_stride = (uint32_t)cs;
            L->c_step = (uint32_t)step;
            L->cb_first = cb < cr;
            L->c_off = (uint32_t)((cb < cr ? cb : cr) - base);
            L->v_off = (uint32_t)((cb < cr ? cr : cb) - base);
            ok = 1;
        }
        int fence = -1;
        if (gr_unlock)
            gr_unlock(g_gr, S->b[b].h, &fence);
        if (fence >= 0)
            close(fence);
    }
    if (!ok) {
        /* NV21 with the gralloc stride */
        uint32_t stride = (uint32_t)v->width;
        uint32_t sh = (uint32_t)v->height;
        L->y_stride = stride;
        L->c_stride = stride;
        L->c_step = 2;
        L->cb_first = 0;
        L->c_off = stride * sh;
        L->v_off = L->c_off + 1;
        LOGK("lock_flex failed, assuming NV21 stride %u height %u", stride, sh);
    }
}

static int dev_configure(const struct sc_config *c)
{
    if (!g_dev)
        return -ENODEV;
    stop_requests();
    free_streams();
    if (c->n <= 0)
        return 0;
    g_video_mode = c->video_mode;
    const struct sc_caminfo *I = &g_info[g_dev_idx];
    camera3_stream_t *ptrs[SC_MAX_STREAMS];
    g_nst = c->n > SC_MAX_STREAMS ? SC_MAX_STREAMS : c->n;
    for (int i = 0; i < g_nst; i++) {
        struct sstream *S = &g_st[i];
        memset(S, 0, sizeof(*S));
        S->used = 1;
        S->cfg = c->s[i];
        S->st.stream_type = CAMERA3_STREAM_OUTPUT;
        if (c->s[i].kind == SC_KIND_STILL) {
            S->st.format = HAL_PIXEL_FORMAT_BLOB;
            S->st.width = (uint32_t)c->s[i].w;
            S->st.height = (uint32_t)c->s[i].h;
            S->st.data_space = HAL_DATASPACE_V0_JFIF;
        } else {
            S->st.format = HAL_PIXEL_FORMAT_YCbCr_420_888;
            S->st.width = (uint32_t)c->s[i].w;
            S->st.height = (uint32_t)c->s[i].h;
            S->st.data_space = HAL_DATASPACE_V0_JFIF;
            if (c->s[i].kind == SC_KIND_VIDEO)
                S->st.usage = GRALLOC_USAGE_HW_VIDEO_ENCODER;
        }
        S->st.rotation = CAMERA3_STREAM_ROTATION_0;
        ptrs[i] = &S->st;
    }
    camera3_stream_configuration_t sc;
    memset(&sc, 0, sizeof(sc));
    sc.num_streams = (uint32_t)g_nst;
    sc.streams = ptrs;
    sc.operation_mode = CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE;
    int r = g_dev->ops->configure_streams(g_dev, &sc);
    if (r) {
        LOGK("configure_streams: %d", r);
        free_streams();
        return r;
    }
    for (int i = 0; i < g_nst; i++) {
        struct sstream *S = &g_st[i];
        if (S->cfg.kind == SC_KIND_STILL) {
            /* BLOB buffers are allocated as jpeg_max_size x 1 */
            int sz = I->jpeg_max_size > 0 ? I->jpeg_max_size : S->cfg.w * S->cfg.h * 3 / 2;
            S->blob_size = (uint32_t)sz;
            S->st.width = (uint32_t)sz;
            S->st.height = 1;
        }
        int n = (int)S->st.max_buffers + 2;
        if (n < 4)
            n = 4;
        if (S->cfg.kind == SC_KIND_STILL && n > 4)
            n = 4;
        if (n > SC_MAX_BUFS)
            n = SC_MAX_BUFS;
        S->nbuf = 0;
        for (int b = 0; b < n; b++) {
            if (alloc_buffer(S, b))
                break;
            S->nbuf++;
        }
        /* restore the configured size for the HAL (the BLOB stream keeps width = buffer size internally) */
        if (S->cfg.kind == SC_KIND_STILL) {
            S->st.width = (uint32_t)S->cfg.w;
            S->st.height = (uint32_t)S->cfg.h;
        }
        LOGK("stream %d kind %d %dx%d: HAL format 0x%x usage 0x%llx max_buffers %u, %d buffers", S->cfg.sid,
            S->cfg.kind, S->cfg.w, S->cfg.h, S->st.format, (unsigned long long)S->st.usage, S->st.max_buffers, S->nbuf);
        if (S->nbuf == 0) {
            free_streams();
            return -ENOMEM;
        }
    }
    g_configured = 1;
    return 0;
}

static void send_buffers(void)
{
    for (int i = 0; i < g_nst; i++) {
        struct sstream *S = &g_st[i];
        struct sc_buffers B;
        memset(&B, 0, sizeof(B));
        B.hdr.type = SC_BUFFERS;
        B.hdr.len = sizeof(B);
        B.sid = S->cfg.sid;
        B.kind = S->cfg.kind;
        B.w = S->cfg.w;
        B.h = S->cfg.h;
        B.n = S->nbuf;
        int fds[SC_MAX_BUFS];
        for (int b = 0; b < S->nbuf; b++) {
            const struct qti_handle_view *v = qti_view(S->b[b].h);
            fds[b] = v ? v->fd : S->b[b].h->data[0];
            buffer_layout(S, b, &B.l[b]);
            if (S->cfg.kind == SC_KIND_STILL && v)
                B.android_format = v->format;
            else if (v)
                B.android_format = v->format;
        }
        LOGK("stream %d: %d buffers, size %u, y stride %u, chroma off %u stride %u step %u cb_first %u, fmt 0x%x",
            B.sid, B.n, B.l[0].size, B.l[0].y_stride, B.l[0].c_off, B.l[0].c_stride, B.l[0].c_step, B.l[0].cb_first,
            B.android_format);
        send_msg(&B, sizeof(B), fds, S->nbuf);
    }
}

/* torch without an open device (module API 2.4) */
static int set_torch(int idx, int on)
{
    if (idx < 0 || idx >= g_ncam)
        return -EINVAL;
    if (g_mod->common.module_api_version < CAMERA_MODULE_API_VERSION_2_4 || !g_mod->set_torch_mode)
        return -ENOSYS;
    char name[8];
    snprintf(name, sizeof(name), "%d", g_hal_ids[idx]);
    return g_mod->set_torch_mode(name, on ? true : false);
}

/* ------------------------------------------------------------- server */
static void handle_client(int s)
{
    pthread_mutex_lock(&g_sock_lock);
    g_sock = s;
    pthread_mutex_unlock(&g_sock_lock);
    static char buf[65536];
    for (;;) {
        ssize_t n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
        if ((size_t)n < sizeof(struct sc_hdr))
            continue;
        struct sc_hdr *h = (struct sc_hdr *)buf;
        switch (h->type) {
        case SC_HELLO: {
            struct sc_info I;
            memset(&I, 0, sizeof(I));
            I.hdr.type = SC_INFO;
            I.hdr.len = sizeof(I);
            I.version = SC_PROTO_VERSION;
            I.n_cams = g_mod ? g_ncam : 0;
            I.status = g_mod ? 0 : -1;
            snprintf(I.msg, sizeof(I.msg), "%s", g_hal_err);
            send_msg(&I, sizeof(I), NULL, 0);
            for (int i = 0; g_mod && i < g_ncam; i++)
                send_msg(&g_info[i], sizeof(g_info[i]), NULL, 0);
            break;
        }
        case SC_OPEN: {
            struct sc_open *o = (struct sc_open *)buf;
            int r = g_mod ? dev_open(o->index) : -ENODEV;
            reply(SC_OPEN, r, r ? "open failed" : "ok");
            break;
        }
        case SC_CLOSE:
            dev_close();
            reply(SC_CLOSE, 0, "ok");
            break;
        case SC_CONFIG: {
            struct sc_config *c = (struct sc_config *)buf;
            int r = dev_configure(c);
            reply(SC_CONFIG, r, r ? "configure failed" : "ok");
            if (!r)
                send_buffers();
            break;
        }
        case SC_REPEAT:
        case SC_SETTINGS: {
            struct sc_request *q = (struct sc_request *)buf;
            pthread_mutex_lock(&g_lock);
            if (h->type == SC_REPEAT) {
                g_repeat = *q;
                g_repeat_on = q->mask != 0;
            } else {
                uint32_t mask = g_repeat.mask;
                int32_t tag = g_repeat.tag;
                g_repeat = *q;
                g_repeat.mask = mask;
                g_repeat.tag = tag;
            }
            pthread_cond_broadcast(&g_cond);
            pthread_mutex_unlock(&g_lock);
            if (h->type == SC_REPEAT)
                LOGK("repeat tag %d mask 0x%x", q->tag, q->mask);
            start_request_thread();
            break;
        }
        case SC_SHOT: {
            struct sc_request *q = (struct sc_request *)buf;
            pthread_mutex_lock(&g_lock);
            if (g_nshots < SHOTQ)
                g_shots[g_nshots++] = *q;
            pthread_cond_broadcast(&g_cond);
            pthread_mutex_unlock(&g_lock);
            LOGK("shot tag %d mask 0x%x orient %d", q->tag, q->mask, q->c.jpeg_orientation);
            start_request_thread();
            break;
        }
        case SC_RELEASE: {
            struct sc_release *rl = (struct sc_release *)buf;
            pthread_mutex_lock(&g_lock);
            for (int i = 0; i < g_nst; i++)
                if (g_st[i].cfg.sid == rl->sid && rl->idx >= 0 && rl->idx < g_st[i].nbuf &&
                    g_st[i].b[rl->idx].state == B_HOST)
                    g_st[i].b[rl->idx].state = B_FREE;
            pthread_cond_broadcast(&g_cond);
            pthread_mutex_unlock(&g_lock);
            break;
        }
        case SC_TORCH: {
            struct sc_torch *t = (struct sc_torch *)buf;
            int r = g_mod ? set_torch(t->index, t->on) : -ENODEV;
            reply(SC_TORCH, r, NULL);
            break;
        }
        case SC_FLUSH:
            stop_requests();
            pthread_mutex_lock(&g_lock);
            for (int i = 0; i < g_nst; i++)
                for (int b = 0; b < g_st[i].nbuf; b++)
                    if (g_st[i].b[b].state == B_HAL)
                        g_st[i].b[b].state = B_FREE;
            pthread_mutex_unlock(&g_lock);
            reply(SC_FLUSH, 0, NULL);
            break;
        default:
            LOGI("unknown message %u", h->type);
        }
    }
    LOGK("host disconnected");
    dev_close();
    pthread_mutex_lock(&g_sock_lock);
    g_sock = -1;
    pthread_mutex_unlock(&g_sock_lock);
    close(s);
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    setup_logging();
    LOGK("starting (pid %d)", getpid());
    if (gralloc_init())
        snprintf(g_hal_err, sizeof(g_hal_err), "gralloc unavailable");
    else if (hal_init())
        g_mod = NULL;
    if (argc > 1 && !strcmp(argv[1], "--probe")) {
        LOGK("probe done: %d cameras", g_ncam);
        return 0;
    }

    int ls = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    struct sockaddr_un a;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    memcpy(a.sun_path + 1, SC_SOCKET_NAME, strlen(SC_SOCKET_NAME));
    socklen_t al = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + strlen(SC_SOCKET_NAME));
    if (bind(ls, (struct sockaddr *)&a, al) || listen(ls, 2)) {
        LOGK("bind/listen: %s", strerror(errno));
        return 1;
    }
    LOGK("listening, %d cameras", g_ncam);
    for (;;) {
        int s = accept4(ls, NULL, NULL, SOCK_CLOEXEC);
        if (s < 0) {
            if (errno == EINTR)
                continue;
            sleep(1);
            continue;
        }
        int sz = 1 << 20;
        setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
        LOGK("host connected");
        handle_client(s);
    }
}
