# hmos2curtana

**HarmonyOS 5 (OpenHarmony) for the Redmi Note 9S (curtana).** A port of
[hmos2sweet](https://github.com/tsryzenstech/hmos2sweet) (Redmi Note 10 Pro) to curtana.

`hmos2curtana` takes a **HarmonyOS full OTA package** of the Huawei Pura 70 (`update_full_base.zip`) and turns it
into a ROM you can flash on the Redmi Note 9S. The userspace stays Huawei's. What changes is underneath it:

- the crDroid 4.14 sm6250 (miatoll) kernel with the patches OpenHarmony needs;
- a set of **selectable patches** that replace the Kirin drivers with Snapdragon 720G equivalents.

> **Experimental and UNTESTED on a real curtana.** hmos2sweet boots on sweet; this port has not booted on a
> Redmi Note 9S yet. Keep a way back: an unlocked bootloader, a working recovery and a curtana ROM zip.
> No Huawei or Xiaomi image is included in this repository. Personal use only.

## What changed from hmos2sweet

sweet (SM7150 "sdmmagpie") and curtana (SM7125 "atoll") are different SoCs, but both are Qualcomm 4.14 platforms
with the same GPU (Adreno 618), WiFi/BT chip (WCN3990), UFS controller, USB controller and coprocessor stack, so
most of hmos2sweet's userspace work carries over unchanged.

| Part | sweet (hmos2sweet) | curtana (hmos2curtana) |
|---|---|---|
| Kernel | LineageOS sm6150 | **crDroid sm6250 16.0** (it carries the EROFS backport; LineageOS sm6250 has no EROFS). All 18 OpenHarmony kernel patches ported (firmware paths re-done, vibrator patch moved to `drivers/misc/aw8624_haptic`) |
| DTB / DTBO | `xiaomi-sdmmagpie.dtb` + sweet overlay | `cust-atoll-ab.dtb` + the four miatoll overlays (bootloader picks curtana's) |
| Command line | `hardware=sweet`, earlycon `0x880000` | `hardware=curtana`, earlycon `0xa88000`, `swiotlb=1` |
| `super` | 8.5 GiB | **8 GiB**: size is checked, `preload` is dropped if needed |
| Display | 120 Hz AMOLED | **60 Hz LCD** (patch 13 rewritten), curtana cutout (patch 12) |
| Audio | Bolero + 2× Awinic aw882xx on PRIM_MI2S, low-latency PCM 13 | **WCD937x + TI TAS2562 on TERT_MI2S**, low-latency PCM 9, SmartPA feedback PCM 32, `mixer_paths_wcd937x.xml` (**audio HAL rebuilt**) |
| Thermal | sm6150 ADC zones | atoll `*-therm-usr` zones |
| Vibration | aw8624 | aw8624 units only (drv2624 units: no vibration yet) |
| NFC | PN553 | **none on curtana**: patch 45 off |
| IR blaster | ir-spi `/dev/lirc0` | **off**: no LIRC driver in the curtana kernel yet |
| Camera | CamX bridge (WIP) | **off**: blob list is sweet's |
| Setup wizard | kept | **removed by default** (WiFi not confirmed on curtana yet) |
| Host | Linux / WSL | Linux / WSL **or macOS** (case-sensitive volume, see below) |

Unchanged and reused as built by hmos2sweet: hmos-preinit, display composer/allocator, Mesa, qrtr/rmtfs/tqftpserv/
pd-mapper/sweet-rproc, hexagonrpcd, sensors HAL, Bluetooth, RIL, GNSS, user_auth/pin_auth, HUKS, USB, thermal HDI.
Their internal names (`sweet-rproc`, `/vendor/etc/sweet/...`) are baked into the binaries and were kept on purpose.

## Status on curtana

Nothing has been verified on the device yet. Expected, from the hardware overlap with sweet:

| Feature | Expectation |
|---|---|
| Boot, display, GPU, touch, keys | 🟡 should work if the kernel boots (same display/GPU stack) |
| WiFi, Bluetooth, GPS, telephony | 🟡 same WCN3990 / Qualcomm modem path as sweet |
| Audio | 🟡 new routing for the TAS2562, needs testing |
| Sensors | 🟡 same SSC path; configs come from the curtana ROM |
| Vibration | 🟡 aw8624 units |
| Camera, fingerprint, IR, NFC | 🔴 not available |

## Requirements

- **Linux / WSL**, or **macOS** with Python 3.8+. On macOS the work and output folders must be on a
  case-sensitive volume (the script checks it and prints the commands to create one).
- About **25 GB free** (OTA 5.7 GB + extracted images + output).
- The **HarmonyOS full OTA** of the Huawei Pura 70; `fetchfw` downloads it.
- **A curtana / miatoll ROM zip** with block images (`vendor.new.dat.br` + `vendor.transfer.list`), e.g.
  LineageOS or crDroid for miatoll, or its extracted `vendor.img`. `pip3 install brotli` for `.br`.
- **The curtana kernel and audio HAL built** (`prebuilt/kernel/`, patch 31): run the GitHub Actions workflow
  "build curtana prebuilts" in your fork and unzip its `curtana-prebuilts` artifact here, or run
  `build/build_kernel.sh` + `build/build_audio.sh` on Linux (docs/BUILDING.md).
- Phone: **unlocked bootloader**, `fastboot` and `adb` (platform-tools) in PATH, a recovery (OrangeFox/TWRP for
  miatoll) to read logs and to go back.

## Usage

```sh
cd hmos2curtana
tools/fetch_tools.sh                 # erofs tools (Linux or macOS) into tools/bin
tools/build_debugfs_macos.sh         # macOS only: debugfs (Linux: install e2fsprogs)
pip3 install brotli

# macOS only: a case-sensitive volume for the work and output folders
hdiutil create -size 60g -type SPARSE -fs 'Case-sensitive APFS' -volname hmos ~/hmos.sparseimage
hdiutil attach ~/hmos.sparseimage

python3 fetchfw.py --download-only --dest /Volumes/hmos       # or: get update_full_base.zip yourself
python3 hmos2curtana.py --ota /Volumes/hmos/update_full_base.zip --curtana-rom ~/lineage-miatoll.zip \
        --work /Volumes/hmos/work --out /Volumes/hmos/out
```

The patch menu, `--patches`, `--list` and `--no-setup-wizard` work as in hmos2sweet (below).

The patch list opens:

```
  Patches to apply  (number = toggle, a = all, n = none, d = defaults,
                     i<number> = details, Enter = continue, q = quit)

     [=] Core: kernel, preinit, fstab
   1 [x] Display: DRM/KMS composer + buffer allocator
   2 [x] GPU: Mesa freedreno (Adreno 618) for OpenGL ES
   ...
```

- a number (or several, separated by spaces) toggles a patch;
- `a` enables everything, `n` disables everything, `d` goes back to the defaults;
- `i3` shows the description of patch 3;
- Enter continues.

Dependencies are handled automatically: enabling audio also enables "Modem/ADSP bring-up", disabling the
latter also disables audio and WiFi.

Without the menu (scripts, CI):

```sh
python3 hmos2curtana.py --ota OTA.zip --curtana-rom ROM.zip --patches defaults            # defaults
python3 hmos2curtana.py --ota OTA.zip --curtana-rom ROM.zip --patches all,-60             # all but debug-tools
python3 hmos2curtana.py --ota OTA.zip --curtana-rom ROM.zip --patches defaults --no-setup-wizard
python3 hmos2curtana.py --list                                                          # patch list
```

### Setup wizard

HarmonyOS's first-boot setup needs a WiFi connection. WiFi is not confirmed on curtana yet, so the
**"Remove the setup wizard"** patch is on by default (`--keep-setup-wizard` keeps the wizard):

- it removes `HwStartupGuide` from `system.img` (the original files are saved in `out/removed/`);
- it marks the first boot as completed in the settings database (`device_provisioned`, `user_setup_complete`).

## Installation

The output folder contains:

| File | |
|---|---|
| `system.img`, `vendor.img`, `chip_prod.img`, `sys_prod.img`, `cust.img`, `patch.img` | logical partitions (in `super`) |
| `boot.img`, `dtbo.img` | kernel + HarmonyOS ramdisk with `hmos-preinit` |
| `install_clean.bat` / `.sh` | **first install**: wipes Android and ALL data, creates the HarmonyOS partitions |
| `update_keep_data.bat` / `.sh` | update from a previous hmos2curtana build, **data is kept** |
| `get_logs.bat` / `.sh` | from recovery: copies the kernel log, hilog and boot screenshots |
| `BUILD_INFO.txt` | OTA version and patches used |
| `removed/` | files taken out of the images (setup wizard) |

1. Phone in fastboot (Power + Volume down).
2. `install_clean.bat` (Windows) or `./install_clean.sh` (Linux). It asks you to type `YES`.
3. The first boot takes a few minutes.

To go back to Android, flash a curtana / miatoll ROM again (recovery, or a MIUI fastboot ROM with `flash_all`):
the ROM recreates `super`. **Back up your `persist` and `modem` partitions first** (TWRP/OrangeFox backup):
they hold your IMEI/calibration and are never written by hmos2curtana, but a backup is cheap.

### Power off and charging

With a charger or a PC connected, "power off" makes the PMIC start the phone again in charging mode
(`androidboot.mode=charger`). `hmos-preinit` recognises it and does not start HarmonyOS: the screen stays off
and the phone keeps charging. Unplugging the cable powers it off; holding the power key for 1.5 s boots
HarmonyOS.

### Logs

The ramdisk contains `hmos-preinit`, which saves to the `cache` partition on every boot:

- the kernel log;
- snapshots of hilog, processes, threads and services;
- a screenshot.

To read them: boot the recovery (Power + Volume up), then run `get_logs.bat` / `get_logs.sh`. See
[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

## Repository layout

```
hmos2curtana.py        converter (patch menu, OTA extraction, image build, flash scripts)
fetchfw.py             finds/downloads Pura 70 firmware and runs hmos2sweet.py (lib/fetchfw.py)
lib/                   Python modules: Huawei OTA, EROFS, boot.img, curtana ROM (sweetrom.py), patches, scripts, fetchfw
patches/NN-name/       one folder per patch: patch.json (+ hook.py) + files/ with the binaries
prebuilt/              kernel (Image.gz, dtb, dtbo.img) and hmos-preinit
src/                   sources of everything in patches/ and prebuilt/
  kernel/patches/        patches on crDroid android_kernel_xiaomi_sm6250 (16.0)
  preinit/               hmos-preinit (/init: maps super, logs, charger mode, then HarmonyOS /bin/init_early)
  display/               DRM/KMS composer + dma-buf allocator (OpenHarmony VDI)
  mesa/                  Mesa 26.1 patches: OHOS EGL platform, YUV import
  audio/                 audio VDI HAL on tinyalsa
  userauth/ huks/ iam/   user_auth stub without TEE, software HUKS, OpenHarmony REE user_auth/pin_auth
  remoteproc/            sweet-rproc + patches to rmtfs/tqftpserv/pd-mapper (linux-msm)
  sensors/               hexagonrpcd for the ADSP + sensors HAL (SSC over QRTR/QMI)
  bluetooth/             Bluetooth vendor library for WCN3990
  ril/ gnss/ ir/         RIL over QMI (calls, SMS, data, supplementary services, USSD, IMS), GNSS over QMI LOC, IR
  camera/                camera: Android HAL bridge (cambridge), namespace launcher, OpenHarmony camera host (WIP)
  oobe/ idle/ stubs/     sweet-oobedone (skip setup), placeholders for Kirin daemons, link stubs
build/                 scripts to rebuild the kernel and the binaries (see docs/BUILDING.md)
.github/workflows/     CI: builds the curtana kernel + audio HAL on Linux
tools/                 fetch_tools.sh (erofs-utils, Linux/macOS), build_debugfs_macos.sh, gen_patch_docs.py
flash/                 log scripts
docs/                  architecture, patches, building, known issues
```

Every patch is described in [docs/PATCHES.md](docs/PATCHES.md). How the port works is explained in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md); rebuilding the binaries in [docs/BUILDING.md](docs/BUILDING.md).

## Licenses

The scripts in this repository are Apache-2.0. The individual parts have different licenses:

- kernel and kernel patches, hmos-preinit: GPL-2.0;
- code derived from OpenHarmony: Apache-2.0;
- Mesa: MIT;
- tinyalsa and linux-msm: BSD-3-Clause;
- sqlite: public domain.

Details are in [NOTICE.md](NOTICE.md). HarmonyOS, Huawei, Xiaomi and Redmi are trademarks of their respective
owners. This project is not affiliated with any of them. hmos2curtana is a port of
[hmos2sweet](https://github.com/tsryzenstech/hmos2sweet); all the HarmonyOS bring-up work is theirs.
