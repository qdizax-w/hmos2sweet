/*
 * Minimal declarations of the graphic_surface classes the camera host calls.
 *
 * Only the public surface headers are included. The producer side is used
 * through ProducerSurface / SurfaceBufferImpl / BufferExtraDataImpl member
 * functions called with qualified (non-virtual) calls: they resolve to the
 * symbols exported by the phone's libsurface.z.so, whatever the vtable
 * layout of Huawei's build is. Structures passed by reference are padded so
 * that fields added by newer versions read as zero.
 *
 * License: Apache-2.0
 */
#ifndef SWEET_SURFACE_MIN_H
#define SWEET_SURFACE_MIN_H

#include <refbase.h>
#include <surface.h>
#include <surface_buffer.h>
#include <buffer_extra_data.h>
#include <string>

namespace OHOS {

class ProducerSurface : public Surface {
public:
    GSError RequestBuffer(sptr<SurfaceBuffer> &buffer, int32_t &fence, BufferRequestConfig &config);
    GSError FlushBuffer(sptr<SurfaceBuffer> &buffer, int32_t fence, BufferFlushConfig &config);
    GSError CancelBuffer(sptr<SurfaceBuffer> &buffer);
    GSError SetQueueSize(uint32_t queueSize);
    uint32_t GetQueueSize();
    GSError CleanCache(bool cleanAll);
};

class SurfaceBufferImpl : public SurfaceBuffer {
public:
    void *GetVirAddr();
    int32_t GetStride() const;
    uint32_t GetSize() const;
    int32_t GetWidth() const;
    int32_t GetHeight() const;
    int32_t GetFileDescriptor() const;
    GSError Map();
    GSError FlushCache();
    sptr<BufferExtraData> GetExtraData() const;
    BufferHandle *GetBufferHandle() const;
};

class BufferExtraDataImpl : public BufferExtraData {
public:
    GSError ExtraSet(const std::string &key, int32_t value);
    GSError ExtraSet(const std::string &key, int64_t value);
};

} // namespace OHOS

namespace sweetcam {

struct PaddedRequestConfig {
    OHOS::BufferRequestConfig c;
    char pad[64];
};

struct PaddedFlushConfig {
    OHOS::BufferFlushConfig c;
    char pad[64];
};

static inline OHOS::ProducerSurface *PS(const OHOS::sptr<OHOS::Surface> &s)
{
    return static_cast<OHOS::ProducerSurface *>(s.GetRefPtr());
}

static inline OHOS::SurfaceBufferImpl *SB(const OHOS::sptr<OHOS::SurfaceBuffer> &b)
{
    return static_cast<OHOS::SurfaceBufferImpl *>(b.GetRefPtr());
}

} // namespace sweetcam

#endif
