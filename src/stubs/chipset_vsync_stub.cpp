/*
 * libchipset_vsync.z.so replacement for sweet.
 *
 * render_service asks the camera HAL's "chipset_vsync_service" for a
 * hardware vsync source on every frame until it gets one
 * (RSMainThread::ConnectChipsetVsyncSer retries while InitChipsetVsyncImpl
 * returns -1). That service is part of Huawei's Kirin camera host and never
 * exists on sweet, so each frame cost a failed HDI service lookup through
 * hdf_devmgr (~30 per second). Report "connected" once and ignore the
 * vsync updates.
 */
namespace OHOS {
namespace Camera {
class ChipsetVsyncImpl {
public:
    static ChipsetVsyncImpl &Instance();
    int InitChipsetVsyncImpl();
    int SetVsyncImpl(long timestamp, long period);
};

static ChipsetVsyncImpl g_instance;

ChipsetVsyncImpl &ChipsetVsyncImpl::Instance()
{
    return g_instance;
}

int ChipsetVsyncImpl::InitChipsetVsyncImpl()
{
    return 0;
}

int ChipsetVsyncImpl::SetVsyncImpl(long, long)
{
    return 0;
}
} // namespace Camera
} // namespace OHOS
