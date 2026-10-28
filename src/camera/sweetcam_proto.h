/*
 * sweetcam protocol: between the HarmonyOS camera host (sweet_camera_host,
 * musl) and the Android camera HAL bridge (cambridge, bionic, running in an
 * Android mount namespace). Unix SOCK_SEQPACKET socket in the abstract
 * namespace, little endian, plain C structs (both sides are aarch64 clang).
 *
 * License: Apache-2.0
 */
#ifndef SWEETCAM_PROTO_H
#define SWEETCAM_PROTO_H

#include <stdint.h>

#define SC_SOCKET_NAME "sweetcam"       /* abstract: "\0sweetcam" */
#define SC_PROTO_VERSION 3

#define SC_MAX_CAMS 8
#define SC_MAX_SIZES 40
#define SC_MAX_STREAMS 4
#define SC_MAX_BUFS 10
#define SC_MAX_FACES 10

enum sc_msg_type {
    /* host -> bridge */
    SC_HELLO = 1,       /* -> SC_INFO, then one SC_CAMINFO per camera */
    SC_OPEN,            /* sc_open -> SC_REPLY */
    SC_CLOSE,           /* -> SC_REPLY */
    SC_CONFIG,          /* sc_config -> SC_REPLY, then one SC_BUFFERS per stream (with fds) */
    SC_REPEAT,          /* sc_request (mask 0 = stop) */
    SC_SETTINGS,        /* sc_request: new controls for the repeating request */
    SC_SHOT,            /* sc_request: single capture */
    SC_RELEASE,         /* sc_release: the host is done with a buffer */
    SC_TORCH,           /* sc_torch -> SC_REPLY */
    SC_FLUSH,           /* -> SC_REPLY */

    /* bridge -> host */
    SC_INFO = 100,
    SC_CAMINFO,
    SC_REPLY,
    SC_BUFFERS,
    SC_FRAME,
    SC_SHUTTER,
    SC_RESULT,
    SC_ERROR,
    SC_CAPTURE_DONE,    /* all buffers of a single capture delivered (or failed) */
};

struct sc_hdr {
    uint32_t type;
    uint32_t len;       /* total message length, header included */
};

struct sc_size {
    int32_t w, h;
    int64_t min_duration;   /* ns */
};

struct sc_caminfo {
    struct sc_hdr hdr;
    int32_t index;          /* our index (0..n-1) */
    int32_t hal_id;         /* Android camera id */
    int32_t facing;         /* 0 back, 1 front, 2 external */
    int32_t orientation;    /* sensor orientation, degrees */
    int32_t hw_level;       /* android.info.supportedHardwareLevel */
    int32_t n_yuv;
    struct sc_size yuv[SC_MAX_SIZES];
    int32_t n_jpeg;
    struct sc_size jpeg[SC_MAX_SIZES];
    int32_t n_fps;
    int32_t fps[16][2];
    int32_t ae_comp_min, ae_comp_max;
    int32_t ae_comp_step_num, ae_comp_step_den;
    uint8_t flash;
    uint8_t n_af, af_modes[8];
    uint8_t n_awb, awb_modes[12];
    uint8_t n_ae, ae_modes[8];
    uint8_t n_stab, stab_modes[4];
    uint8_t n_ois, ois_modes[4];
    uint8_t n_face, face_modes[4];
    uint8_t n_scene, scene_modes[24];
    uint8_t pad0;
    int32_t max_faces;
    float max_digital_zoom;
    float zoom_ratio_min, zoom_ratio_max;  /* android.control.zoomRatioRange (0 if absent) */
    int32_t active[4];      /* x, y, w, h */
    int32_t pixel_array[2];
    float phys_size[2];     /* mm */
    float focal_length;     /* mm, first */
    float aperture;
    float min_focus_dist;
    int32_t iso_range[2];
    int64_t exposure_range[2];
    int32_t jpeg_max_size;
    int32_t max_regions_ae, max_regions_awb, max_regions_af;
    int32_t partial_results;
    int32_t n_thumb;
    int32_t thumb[8][2];
    int32_t logical;        /* logical multi-camera */
};

struct sc_info {
    struct sc_hdr hdr;
    int32_t version;
    int32_t n_cams;
    int32_t status;         /* 0 ok, <0 the Android HAL could not be loaded */
    char msg[128];
};

struct sc_open {
    struct sc_hdr hdr;
    int32_t index;
};

struct sc_reply {
    struct sc_hdr hdr;
    int32_t for_type;
    int32_t status;         /* 0 ok, negative errno */
    char msg[96];
};

enum sc_stream_kind {
    SC_KIND_PREVIEW = 0,    /* YUV 4:2:0 */
    SC_KIND_VIDEO = 1,      /* YUV 4:2:0, video record usage */
    SC_KIND_STILL = 2,      /* JPEG (BLOB) */
    SC_KIND_YUV_STILL = 3,  /* YUV 4:2:0 still */
};

struct sc_stream_cfg {
    int32_t sid;            /* host stream id */
    int32_t kind;
    int32_t w, h;
};

struct sc_config {
    struct sc_hdr hdr;
    int32_t n;              /* 0 = unconfigure */
    int32_t video_mode;     /* 1: recording session */
    struct sc_stream_cfg s[SC_MAX_STREAMS];
};

struct sc_buflayout {
    uint32_t size;          /* mmap length */
    uint32_t offset;        /* mmap offset */
    uint32_t y_off, y_stride;
    uint32_t c_off, c_stride;   /* first chroma byte (Cb for NV12, Cr for NV21) */
    uint32_t c_step;            /* 2 = semi planar */
    uint32_t cb_first;          /* 1: NV12 order, 0: NV21 */
    uint32_t v_off;             /* planar only (c_step == 1): second chroma plane */
};

struct sc_buffers {
    struct sc_hdr hdr;
    int32_t sid;
    int32_t kind;
    int32_t w, h;
    int32_t n;              /* fds carried: one per buffer, in order */
    int32_t android_format;
    struct sc_buflayout l[SC_MAX_BUFS];
};

struct sc_ctrl {
    float zoom;             /* ratio, 1.0 = none */
    int32_t ae_comp;        /* index */
    int32_t fps_min, fps_max;
    int32_t jpeg_orientation;
    uint8_t jpeg_quality;
    uint8_t flash_mode;     /* 0 off, 1 auto, 2 always (single), 3 torch */
    uint8_t af_mode;        /* 0 continuous picture, 1 continuous video, 2 auto (trigger), 3 off */
    uint8_t af_trigger;     /* 0 idle, 1 start, 2 cancel */
    uint8_t ae_precapture;  /* 0 idle, 1 start */
    uint8_t ae_lock, awb_lock;
    uint8_t awb_mode;       /* android awb mode, 1 = auto */
    uint8_t video_stab;
    uint8_t face_detect;
    uint8_t has_gps;
    uint8_t mirror;
    int32_t af_region[5];   /* x0, y0, x1, y1, weight; weight 0 = none (active array coordinates) */
    int32_t ae_region[5];
    double gps[3];
    int64_t gps_ts;
    int32_t scene_mode;     /* -1 = none */
    int32_t exposure_mode;  /* 0 auto, 1 locked */
};

struct sc_request {
    struct sc_hdr hdr;
    int32_t tag;            /* host capture id */
    uint32_t mask;          /* bit = host stream index in the last config (0..3) */
    struct sc_ctrl c;
};

struct sc_release {
    struct sc_hdr hdr;
    int32_t sid;
    int32_t idx;
};

struct sc_torch {
    struct sc_hdr hdr;
    int32_t index;
    int32_t on;
};

struct sc_frame {
    struct sc_hdr hdr;
    int32_t sid;
    int32_t idx;
    int32_t tag;
    int32_t status;         /* 0 ok, else the buffer content is invalid */
    uint32_t frame;
    uint32_t data_size;     /* JPEG: real size, else 0 */
    int64_t ts;
};

struct sc_shutter {
    struct sc_hdr hdr;
    int32_t tag;
    uint32_t frame;
    int64_t ts;
};

struct sc_face {
    int32_t rect[4];        /* x0, y0, x1, y1 */
    int32_t score;
};

struct sc_result {
    struct sc_hdr hdr;
    int32_t tag;
    uint32_t frame;
    int64_t ts;
    uint32_t have;          /* bit 0: 3A states, 1: exposure, 2: faces, 3: lens */
    uint8_t af_state, ae_state, awb_state, flash_state;
    uint8_t af_mode, ae_mode, awb_mode, n_faces;
    int64_t exposure_ns;
    int32_t iso;
    float focus_distance;
    float zoom;
    int32_t crop[4];
    struct sc_face faces[SC_MAX_FACES];
};

struct sc_error {
    struct sc_hdr hdr;
    int32_t tag;
    uint32_t frame;
    int32_t code;           /* 1 device, 2 request, 3 result, 4 buffer */
    int32_t sid;
};

struct sc_capture_done {
    struct sc_hdr hdr;
    int32_t tag;
    uint32_t frame;
    int32_t ok;
};

#endif
