# Known issues and logs

## Reading the logs

Every boot leaves in the `cache` partition (`/log/hmos` on HarmonyOS):

| File | Content |
|---|---|
| `kmsg-N.log` | kernel log |
| `boot-N/hilog-stream-*.txt` | hilog |
| `snapN-*.txt` | processes, threads, services, mounts, parameters, GPU/display state |
| `screen-*.jpeg` | screenshots |

HarmonyOS's own logs are in `/data/log` (hilog, faultlog). To download everything: recovery (Power + Volume up),
then `get_logs.bat` / `get_logs.sh`. `pstore/` holds the last kernel messages before a reboot.

Useful lines in kmsg:

- `sweet-rproc:` firmware partition, ADSP, modem, `wlan0`, sound card, mobile data;
- `sweet-oobedone:` setup keys written into the settings database;
- `sweet-sensors:` sensors HAL (SSC service found, SUID of every sensor, errors);
- `hmos: power off requested by` / `hmos: reboot("...") requested by`: who asked for a power off or a restart;
- `hmos-preinit: androidboot.mode=charger`: the phone started in off-mode charging;
- `cambridge:` / `sweetcam-launch:` camera bridge (work in progress).

Useful hilog tags:

- `SWEET_AUDIO`: audio HAL;
- `composer_host`: display;
- `C04201/foundation/DMS`: screens seen by the window manager;
- `USER_AUTH_HDI`, `PIN_AUTH_HDI`, `USER_AUTH_SA`: lock screen credentials.

## Known issues

| Symptom | Status |
|---|---|
| ADSP restarting every ~40 s (`sensor_process ... USER-PD DOG`) | needs patch 34 (hexagonrpcd); if it goes on, look for `hexagonrpcd` and `adsprpc` in kmsg |
| `wlan: /dev/wlan ON -> -22`, no `wlan0` | look for `sweet: board id` / `BDF download` (icnss) in kmsg: the kernel must send bdwlan to the firmware |
| Bluetooth does not turn on | `sweet-bt:` lines in kmsg: chip version, crbtfw/crnv files, HCI reset answer |
| Spontaneous reboots | run `get_logs` right after: `pstore/` and the `kmsg` of the previous boot show the cause; look for `crashed`, `critical`, `subsys-restart`, `Kernel panic`. Patch 15 removes the reboots for crashing services, kernel patch 0013 those for a slow ADSP/modem |
| "Power off" restarts the phone | with a cable plugged in this is off-mode charging: since this release the phone stays off and charges. Without a cable, look for `hmos: power off requested` in pstore |
| Black screen, no boot animation, back to fastboot | the kernel is the first thing to check on curtana: pstore after the reboot shows how far it got. The kernel is crDroid sm6250 + the OpenHarmony patches, built by `build/build_kernel.sh` |
| PIN asked at boot although none was set | fixed in iter42 (user_auth HDI built for the HarmonyOS 5.x interface). If it comes back: `USER_AUTH_HDI` in hilog, "VerifyAuthToken bad param" means the interface does not match |
| Sensors missing in apps | `sweet-sensors:` lines in kmsg: if it stays at "waiting for the SSC", the ADSP sensors domain did not start (see `hexagonrpcd`) |
| `tqftpserv: read/write <file> -> error` in kmsg | file the modem asks the TFTP server for: if `wlanmdsp.mbn` or a readwrite folder is missing, WiFi does not start |
| No sound on curtana | the audio HAL must be the curtana rebuild (patch 31 warns when it is hmos2sweet's). `sweet audio VDI (tinyalsa, curtana/atoll)` in hilog confirms it; `TAS2562 SmartPA on` / `speaker feedback pcm 32` show the amplifier state |
| No vibration | only curtana units with the aw8624 motor are supported; drv2624 units have no `/sys/class/leds/vibrator` |
| hdc does not see the phone | enable developer options and USB debugging; look for `a600000.dwc3` / `configfs` in kmsg |
| Setup stuck on the WiFi screen | enable "Remove the setup wizard" (`--no-setup-wizard`) |
| Camera | not on curtana: patch 47 is sweet's CamX bridge + blob list; it needs a curtana blob list |
| Fingerprint | not ported: the sensor is handled by a trusted app in the Qualcomm TEE (QSEE) |
| Proximity | not included (on miatoll it is part of the SSC/ADSP sensor set; untested) |
| HEVC video above 1920 px | limit in the media service, patch 40 is not enough |

## Black screen

- **Black from the start**: look for `can not pick a crtc` / `skip connector` (DRM connectors) in kmsg, then the
  `composer_host` lines in hilog.
- **Black after the boot animation**: if SceneBoard restarts in a loop with
  `GetDefaultDisplaySync: Get display info failed`, the window manager did not get the screen. Check that the
  composer created a single display with id 0.
- **Backlight on but no image after pressing power**: fixed (composer fence ownership). If it comes back,
  `threads.txt` in the logs shows the fd count of `composer_host` and `render_service`.

## Going back to Android

Flash a curtana / miatoll ROM (LineageOS, crDroid, MIUI fastboot ROM) from recovery or fastboot. The ROM recreates
Android's logical partitions in `super`.
