#!/usr/bin/env python3
"""Extract system.img and vendor.img from a HarmonyOS full OTA into <work>/ota (what make_sysroot.sh reads).

  python3 build/extract_ota_images.py update_full_base.zip <work folder>
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lib import hwota  # noqa: E402

ota, work = sys.argv[1], sys.argv[2]
out = os.path.join(work, "ota")
os.makedirs(out, exist_ok=True)
hwota.HwOta(ota).extract(["system", "vendor"], out)
