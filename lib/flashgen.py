"""Flash scripts written next to the generated images (Windows .bat and Linux .sh)."""
import os
import shutil

from .util import REPO, info

ANDROID_LOGICAL = ["system", "vendor", "product", "odm", "system_ext", "system_a", "vendor_a", "product_a",
                   "odm_a", "system_ext_a", "system_b", "vendor_b", "product_b", "odm_b", "system_ext_b"]
HMOS_LOGICAL = ["system", "vendor", "chip_prod", "sys_prod", "cust", "patch", "version", "preload"]


def _bat(lines):
    return "\r\n".join(lines) + "\r\n"


def _sh(lines):
    return "\n".join(lines) + "\n"


SUPER_DEV = "S=/dev/block/by-name/super; [ -e $S ] || S=/dev/block/bootdevice/by-name/super"


def _bat_flash(parts, sizes, create):
    """fastbootd part: (create +) resize + flash; partitions fastbootd cannot write are left to :lpwrite."""
    b = ["set LPW="]
    if create:
        for p in parts:
            b.append("fastboot create-logical-partition %s 0 || goto :err" % p)
    for p in parts:
        b.append("fastboot resize-logical-partition %s %d || goto :err" % (p, sizes[p]))
    for p in parts:
        b.append('fastboot flash %s "%%OUT%%%s.img" || set LPW=%%LPW%% %s' % (p, p, p))
    b.append('if not "%LPW%"=="" (call :lpwrite || goto :err) else (fastboot reboot bootloader || goto :err)')
    return b


BAT_LPWRITE = [
    ":lpwrite",
    "REM Some recoveries (LineageOS-based fastbootd) cannot write newly created logical partitions",
    "REM (\"No such file or directory\"): write them from the recovery, straight into super, with lpwrite.",
    "echo fastbootd could not write:%LPW% - writing them from the recovery",
    "fastboot reboot recovery || exit /b 1",
    "adb wait-for-recovery || exit /b 1",
    'adb push "%OUT%lpwrite" /tmp/lpwrite || exit /b 1',
    "adb shell chmod 755 /tmp/lpwrite || exit /b 1",
    "for %%p in (%LPW%) do (",
    "  echo Writing %%p ...",
    '  adb push "%OUT%%%p.img" /tmp/%%p.img || exit /b 1',
    '  adb shell "' + SUPER_DEV + '; /tmp/lpwrite $S %%p /tmp/%%p.img; r=$?; rm -f /tmp/%%p.img; exit $r" || exit /b 1',
    ")",
    "adb reboot bootloader || exit /b 1",
    "exit /b 0",
]


def _sh_flash(parts, sizes, create):
    s = ["LPW="]
    if create:
        for p in parts:
            s.append("fastboot create-logical-partition %s 0" % p)
    for p in parts:
        s.append("fastboot resize-logical-partition %s %d" % (p, sizes[p]))
    for p in parts:
        s.append('fastboot flash %s "$OUT/%s.img" || LPW="$LPW %s"' % (p, p, p))
    s += ['if [ -n "$LPW" ]; then',
          '  echo "fastbootd could not write:$LPW - writing them from the recovery"',
          "  fastboot reboot recovery; adb wait-for-recovery",
          '  adb push "$OUT/lpwrite" /tmp/lpwrite; adb shell chmod 755 /tmp/lpwrite',
          "  for p in $LPW; do",
          '    adb push "$OUT/$p.img" /tmp/$p.img',
          "    adb shell \"" + SUPER_DEV.replace("$", "\\$") +
          "; /tmp/lpwrite \\$S $p /tmp/$p.img; r=\\$?; rm -f /tmp/$p.img; exit \\$r\"",
          "  done",
          "  adb reboot bootloader",
          "else",
          "  fastboot reboot bootloader",
          "fi"]
    return s


def write(out, super_parts, summary):
    parts = [p for p in HMOS_LOGICAL if p in super_parts]
    sizes = {p: os.path.getsize(os.path.join(out, p + ".img")) for p in parts}
    lpw = os.path.join(REPO, "prebuilt", "tools", "lpwrite")
    if os.path.isfile(lpw):
        shutil.copyfile(lpw, os.path.join(out, "lpwrite"))

    # ---------------------------------------------------------------- clean install
    b = ["@echo off", "setlocal",
         "REM hmos2sweet - CLEAN INSTALL: erases Android/HarmonyOS and ALL user data.",
         "REM Phone in fastboot mode (Power + Volume Down), bootloader unlocked.",
         "set OUT=%~dp0",
         "echo This ERASES the phone (system, vendor, userdata). Back up first.",
         "set /p OK=Type YES to continue: ",
         'if /I not "%OK%"=="YES" exit /b 1',
         "fastboot reboot fastboot || goto :err"]
    for p in sorted(set(ANDROID_LOGICAL + HMOS_LOGICAL)):
        b.append("fastboot delete-logical-partition %s >nul 2>&1" % p)
    b += _bat_flash(parts, sizes, True)
    b += ["fastboot format:f2fs userdata || goto :err",
          "fastboot format:ext4 cache || goto :err",
          'fastboot flash dtbo "%OUT%dtbo.img" || goto :err',
          'fastboot flash boot "%OUT%boot.img" || goto :err',
          "fastboot reboot",
          "echo Done. First boot takes a few minutes.",
          "exit /b 0", ":err", "echo FAILED at the previous step.", "exit /b 1", ""] + BAT_LPWRITE
    open(os.path.join(out, "install_clean.bat"), "w", newline="").write(_bat(b))

    s = ["#!/bin/sh", "# hmos2sweet - CLEAN INSTALL: erases Android/HarmonyOS and ALL user data.",
         "# Phone in fastboot mode (Power + Volume Down), bootloader unlocked.",
         'set -e; OUT=$(dirname "$0")',
         'printf "This ERASES the phone. Type YES to continue: "; read OK; [ "$OK" = YES ] || exit 1',
         "fastboot reboot fastboot"]
    for p in sorted(set(ANDROID_LOGICAL + HMOS_LOGICAL)):
        s.append("fastboot delete-logical-partition %s >/dev/null 2>&1 || true" % p)
    s += _sh_flash(parts, sizes, True)
    s += ["fastboot format:f2fs userdata", "fastboot format:ext4 cache",
          'fastboot flash dtbo "$OUT/dtbo.img"', 'fastboot flash boot "$OUT/boot.img"', "fastboot reboot",
          'echo "Done. First boot takes a few minutes."']
    open(os.path.join(out, "install_clean.sh"), "w").write(_sh(s))

    # ---------------------------------------------------------------- update, keeps data
    b = ["@echo off", "setlocal",
         "REM hmos2sweet - UPDATE: reflashes the ROM, user data is KEPT.",
         "REM Only for a phone that already runs a hmos2sweet build. Phone in fastboot mode.",
         "set OUT=%~dp0", "fastboot reboot fastboot || goto :err"]
    b += _bat_flash(parts, sizes, False)
    b += ['fastboot flash dtbo "%OUT%dtbo.img" || goto :err',
          'fastboot flash boot "%OUT%boot.img" || goto :err',
          "fastboot reboot", "exit /b 0", ":err", "echo FAILED at the previous step.", "exit /b 1", ""] + BAT_LPWRITE
    open(os.path.join(out, "update_keep_data.bat"), "w", newline="").write(_bat(b))
    s = ["#!/bin/sh", "# hmos2sweet - UPDATE: reflashes the ROM, user data is KEPT.", 'set -e; OUT=$(dirname "$0")',
         "fastboot reboot fastboot"]
    s += _sh_flash(parts, sizes, False)
    s += ['fastboot flash dtbo "$OUT/dtbo.img"', 'fastboot flash boot "$OUT/boot.img"', "fastboot reboot"]
    open(os.path.join(out, "update_keep_data.sh"), "w").write(_sh(s))

    for f in ("get_logs.bat", "get_logs.sh"):
        shutil.copyfile(os.path.join(REPO, "flash", f), os.path.join(out, f))
    for f in os.listdir(out):
        if f.endswith(".sh"):
            os.chmod(os.path.join(out, f), 0o755)

    with open(os.path.join(out, "BUILD_INFO.txt"), "w") as fh:
        fh.write(summary)
    info("install_clean.(bat|sh), update_keep_data.(bat|sh), get_logs.(bat|sh), BUILD_INFO.txt")
