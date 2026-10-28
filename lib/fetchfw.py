"""fetchfw: find, check and download HarmonyOS full OTA packages for the Huawei Pura 70.

Huawei publishes full packages on its CDN as

    http://update.dbankcdn.com/download/data/pub_13/HWHOTA_hota_900_9/<xx>/v3/<id>/full/update_full_base.zip

next to a filelist.xml (size + sha256 of every file of the package). The <id> part is random, so packages
cannot be enumerated: candidates come from
  * the catalog shipped with hmos2sweet (lib/fetchfw_catalog.json) and an optional user catalog,
  * the Huawei update query service (best effort, --online),
  * URLs given on the command line (--url).

Every candidate is checked remotely before it is offered, without downloading it: filelist.xml is read
for size/sha256, and the zip central directory plus the tiny packageinfo.mbn / VERSION.mbn members are
read with HTTP range requests. Only packages whose device model is a Pura 70 (ADY-*) are accepted.
"""
import hashlib
import json
import os
import re
import struct
import sys
import time
import urllib.error
import urllib.request
import zlib
import xml.etree.ElementTree as ET

PURA70_PREFIXES = ("ADY-",)          # Huawei Pura 70 (ADY-AL00, ADY-AL10, ...)
PURA70_MODELS = ("ADY-AL00", "ADY-AL10")
UA = "Mozilla/5.0 (Linux; HarmonyOS) hmos2sweet-fetchfw"
CATALOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fetchfw_catalog.json")
USER_CATALOG = os.path.join(os.path.expanduser("~"), ".hmos2sweet_fw.json")
# shared list maintained in the repository: new packages show up for everybody without updating the tool
REMOTE_CATALOGS = (
    "https://raw.githubusercontent.com/tsryzenstech/hmos2sweet/main/lib/fetchfw_catalog.json",
)
TIMEOUT = 30


class FwError(Exception):
    pass


# ------------------------------------------------------------------ http
def _req(url, rng=None, data=None, headers=None, method=None):
    h = {"User-Agent": UA}
    if rng:
        h["Range"] = "bytes=%d-%d" % rng
    if headers:
        h.update(headers)
    return urllib.request.Request(url, data=data, headers=h, method=method)


def http_get(url, rng=None, limit=8 << 20):
    try:
        with urllib.request.urlopen(_req(url, rng), timeout=TIMEOUT) as r:
            if rng and r.status != 206:
                raise FwError("%s: the server ignores range requests" % url)
            return r.read(limit)
    except urllib.error.HTTPError as e:
        raise FwError("%s: HTTP %d" % (url, e.code))
    except (urllib.error.URLError, OSError) as e:
        raise FwError("%s: %s" % (url, getattr(e, "reason", e)))


def http_size(url):
    try:
        with urllib.request.urlopen(_req(url, (0, 0)), timeout=TIMEOUT) as r:
            cr = r.headers.get("Content-Range", "")
            m = re.search(r"/(\d+)$", cr)
            if m:
                return int(m.group(1))
            return int(r.headers.get("Content-Length", "0"))
    except urllib.error.HTTPError as e:
        raise FwError("%s: HTTP %d" % (url, e.code))
    except (urllib.error.URLError, OSError) as e:
        raise FwError("%s: %s" % (url, getattr(e, "reason", e)))


# ------------------------------------------------------------- urls
def normalize(url):
    """Accept the package folder, filelist.xml or update_full_base.zip URL; return (base, zip, filelist)."""
    url = url.strip()
    m = re.match(r"^(https?://.+?/v\d+/[^/]+/)(full/.*)?$", url)
    if not m:
        if url.endswith(".zip"):
            base = url.rsplit("/", 1)[0] + "/"
            return base, url, base + "filelist.xml"
        raise FwError("not a Huawei package URL: %s" % url)
    base = m.group(1)
    full = base + "full/"
    zipurl = url if url.endswith(".zip") else full + "update_full_base.zip"
    return base, zipurl, full + "filelist.xml"


def parse_filelist(xml):
    root = ET.fromstring(xml)
    files = {}
    for f in root.iter("file"):
        name = (f.findtext("spath") or "").strip()
        files[name] = {"sha256": (f.findtext("sha256") or "").strip().lower(),
                       "size": int((f.findtext("size") or "0").strip() or 0)}
    return files


# --------------------------------------------------- remote zip reading
def _find_eocd(tail):
    i = tail.rfind(b"PK\x05\x06")
    if i < 0:
        raise FwError("no zip end of central directory")
    return i


def remote_zip_members(url, size):
    """Read the central directory of a remote (zip64) archive. Returns {name: (method, csize, usize, lho)}."""
    tail_len = min(size, 1 << 16)
    tail = http_get(url, (size - tail_len, size - 1))
    e = _find_eocd(tail)
    _, _, _, _, n, cd_size, cd_off, _ = struct.unpack("<IHHHHIIH", tail[e:e + 22])
    if cd_off == 0xFFFFFFFF or cd_size == 0xFFFFFFFF or n == 0xFFFF:
        loc = tail.rfind(b"PK\x06\x07", 0, e)
        if loc < 0:
            raise FwError("zip64 locator missing")
        _, _, z64off, _ = struct.unpack("<IIQI", tail[loc:loc + 20])
        rec = http_get(url, (z64off, z64off + 55))
        if rec[:4] != b"PK\x06\x06":
            raise FwError("bad zip64 end record")
        n, _, cd_size, cd_off = struct.unpack("<QQQQ", rec[24:56])
    cd = http_get(url, (cd_off, cd_off + cd_size - 1), limit=cd_size + 16)
    out = {}
    p = 0
    for _ in range(n):
        if cd[p:p + 4] != b"PK\x01\x02":
            break
        (method, _, _, _, csize, usize, nl, xl, cl, _, _, _, lho) = struct.unpack(
            "<HHHIIIHHHHHII", cd[p + 10:p + 46])
        name = cd[p + 46:p + 46 + nl].decode("utf-8", "replace")
        extra = cd[p + 46 + nl:p + 46 + nl + xl]
        q = 0
        while q + 4 <= len(extra):
            hid, hl = struct.unpack("<HH", extra[q:q + 4])
            if hid == 1:
                vals = extra[q + 4:q + 4 + hl]
                k = 0
                if usize == 0xFFFFFFFF:
                    usize = struct.unpack("<Q", vals[k:k + 8])[0]; k += 8
                if csize == 0xFFFFFFFF:
                    csize = struct.unpack("<Q", vals[k:k + 8])[0]; k += 8
                if lho == 0xFFFFFFFF:
                    lho = struct.unpack("<Q", vals[k:k + 8])[0]; k += 8
            q += 4 + hl
        out[name] = (method, csize, usize, lho)
        p += 46 + nl + xl + cl
    return out


def remote_zip_read(url, member):
    method, csize, usize, lho = member
    if usize > (1 << 20):
        raise FwError("member too big to read remotely")
    hdr = http_get(url, (lho, lho + 29))
    if hdr[:4] != b"PK\x03\x04":
        raise FwError("bad local header")
    nl, xl = struct.unpack("<HH", hdr[26:30])
    start = lho + 30 + nl + xl
    data = http_get(url, (start, start + csize - 1)) if csize else b""
    if method == 0:
        return data
    if method == 8:
        return zlib.decompress(data, -15)
    raise FwError("unsupported compression %d" % method)


def kv(text):
    d = {}
    for line in text.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            d[k.strip()] = v.strip()
    return d


def probe(url, expect_size=None):
    """Identify a remote update_full_base.zip. Returns a dict, raises FwError if it is not usable."""
    size = expect_size or http_size(url)
    members = remote_zip_members(url, size)
    if "update.bin" not in members:
        raise FwError("no update.bin inside: not a HarmonyOS full package")
    info = {"size": size, "zip": url}
    if "packageinfo.mbn" in members:
        pk = kv(remote_zip_read(url, members["packageinfo.mbn"]).decode("utf-8", "replace"))
        info["model"] = pk.get("deviceModel", "")
        info["versionCode"] = pk.get("versionCode", "")
        info["packageType"] = pk.get("packageType", "")
    if "VERSION.mbn" in members:
        info["version"] = remote_zip_read(url, members["VERSION.mbn"]).decode("utf-8", "replace").strip()
    if "update.tag" in members:
        tag = kv(remote_zip_read(url, members["update.tag"]).decode("utf-8", "replace"))
        info["dataType"] = tag.get("PKG_DATA_TYPE", "")
    return info


def is_pura70(info):
    model = info.get("model", "")
    ver = info.get("version", "")
    return model.startswith(PURA70_PREFIXES) or ver.startswith(PURA70_PREFIXES)


def check_candidate(url, log=print):
    base, zipurl, flurl = normalize(url)
    fl = {}
    try:
        fl = parse_filelist(http_get(flurl))
    except (FwError, ET.ParseError) as e:
        log("  filelist.xml not readable (%s), checking the zip directly" % e)
    meta = fl.get("update_full_base.zip") or {}
    info = probe(zipurl, meta.get("size") or None)
    info["sha256"] = meta.get("sha256", "")
    info["base"] = base
    if info.get("packageType", "BASE").upper() != "BASE" or info.get("dataType", "FULL").upper() != "FULL":
        raise FwError("not a full BASE package (%s/%s)" % (info.get("packageType"), info.get("dataType")))
    if not is_pura70(info):
        raise FwError("device %s (%s) is not a Huawei Pura 70" % (info.get("model", "?"), info.get("version", "?")))
    return info


# ------------------------------------------------------------- sources
def load_catalogs():
    entries = []
    for path in (CATALOG, USER_CATALOG):
        try:
            with open(path, encoding="utf-8") as f:
                data = json.load(f)
            for e in data.get("packages", []):
                e = dict(e)
                e["source"] = "catalog" if path == CATALOG else "user catalog"
                entries.append(e)
        except FileNotFoundError:
            pass
        except (OSError, ValueError) as e:
            print("warning: %s: %s" % (path, e))
    return entries


def load_remote_catalogs(log=print):
    entries = []
    for url in REMOTE_CATALOGS:
        try:
            data = json.loads(http_get(url, limit=4 << 20).decode("utf-8"))
        except (FwError, ValueError) as e:
            log("  %s: %s" % (url, e))
            continue
        for e in data.get("packages", []):
            e = dict(e)
            e["source"] = "online catalog"
            entries.append(e)
    return entries


# public pages that list Huawei full packages: every update_full_base.zip URL found there is checked, the
# Pura 70 ones are offered
LINK_PAGES = (
    "https://raw.githubusercontent.com/SimomYung/unpack_huawei_package/main/README.md",
    "https://raw.githubusercontent.com/SimomYung/unpack_huawei_package/master/README.md",
)
PKG_URL_RE = re.compile(r"https?://update\.dbankcdn\.com/[A-Za-z0-9_./%-]+?/v\d+/[A-Za-z0-9_-]+/full/update_full_base\.zip")


def scan_link_pages(log=print):
    found = []
    for url in LINK_PAGES:
        try:
            txt = http_get(url, limit=4 << 20).decode("utf-8", "replace")
        except FwError:
            continue
        for u in PKG_URL_RE.findall(txt):
            if u not in found:
                found.append(u)
    if found:
        log("  %d package links found on public pages" % len(found))
    return [{"url": u, "source": "public list"} for u in found]


def remember(info):
    """Add a verified package to the user catalog so it is listed next time."""
    try:
        with open(USER_CATALOG, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError):
        data = {"packages": []}
    if any(normalize(p["url"])[1] == info["zip"] for p in data["packages"]):
        return
    data["packages"].append({"url": info["zip"], "model": info.get("model", ""), "version": info.get("version", ""),
                             "size": info.get("size", 0), "sha256": info.get("sha256", "")})
    try:
        with open(USER_CATALOG, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
    except OSError:
        pass


# Huawei update query service. Phones ask it for the packages newer than their version; with a very old
# version the answer is the latest full package for the model. The protocol is not documented: this is
# best effort, every URL it returns is verified like any other candidate.
QUERY_ENDPOINTS = (
    "https://query.hicloud.com/sp_ard_common/v2/Check.action?latest=true",
    "https://query.hicloud.com/sp_ard_common/v2/onestopCheck.action?verType=true",
    # HarmonyOS 5 update engine (libdupdate_engine_service): /hota/version-query/v1/dynamiclane/versioncheck
    "https://update.platform.dbankcloud.com/hota/version-query/v1/dynamiclane/versioncheck",
    "https://update.platform.dbankcloud.cn/hota/version-query/v1/dynamiclane/versioncheck",
)


def query_online(log=print):
    found = []
    for model in PURA70_MODELS:
        prod = model.split("-")[0]
        for ver in ("%s-LGRP3-CHN 5.0.0.100(SP1C00E100R1P1)" % prod, "%s-LGRP3-CHN 2.0.0.1" % prod):
            bodies = (
                {"rules": {"FirmWare": ver, "DeviceName": model, "PackageType": "full", "Language": "zh-cn",
                           "OS": "HarmonyOS", "C_version": "C00", "D_version": "D000", "IMEI": "",
                           "deviceId": "", "saleinfo": "CN"}},
                {"deviceName": model, "deviceModel": model, "versionPackageType": "full", "language": "zh-cn",
                 "currentVersionList": [{"versionNumber": ver, "componentType": "TCPU"}],
                 "osVersion": "HarmonyOS 5.0.0", "vendorCountry": "all-cn", "deviceType": "phone"},
                {"commonRules": {"deviceName": model, "language": "zh-cn", "OS": "HarmonyOS",
                                 "PackageType": "full", "udid": "", "deviceId": "", "saleinfo": "CN",
                                 "vendorCountry": "all-cn"},
                 "components": [{"name": "TCPU", "version": ver, "versionPackageType": "1"}]},
            )
            for ep in QUERY_ENDPOINTS:
                for body in bodies:
                    try:
                        req = _req(ep, data=json.dumps(body).encode(),
                                   headers={"Content-Type": "application/json;charset=UTF-8"}, method="POST")
                        with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
                            txt = r.read(1 << 20).decode("utf-8", "replace")
                    except (urllib.error.URLError, OSError, ValueError):
                        continue
                    for u in re.findall(r'https?://[^"\s]+?/v\d+/[A-Za-z0-9_\-]+/', txt):
                        if u not in found:
                            found.append(u)
                            log("  online: %s answered %s" % (model, u))
    return [{"url": u, "source": "online"} for u in found]


# ------------------------------------------------------------- download
def download(info, dest_dir, log=print):
    os.makedirs(dest_dir, exist_ok=True)
    tag = re.sub(r"[^A-Za-z0-9._-]+", "_", info.get("version") or info.get("versionCode") or "fw")
    path = os.path.join(dest_dir, "update_full_base_%s.zip" % tag)
    size = info["size"]
    have = os.path.getsize(path) if os.path.exists(path) else 0
    if have > size:
        os.remove(path)
        have = 0
    if have < size:
        log("downloading %s (%.2f GB) -> %s" % (info.get("version", ""), size / 1e9, path))
        t0, last = time.time(), 0
        with open(path, "ab") as out:
            while have < size:
                try:
                    with urllib.request.urlopen(_req(info["zip"], (have, size - 1)), timeout=TIMEOUT) as r:
                        while True:
                            chunk = r.read(1 << 20)
                            if not chunk:
                                break
                            out.write(chunk)
                            have += len(chunk)
                            now = time.time()
                            if now - last > 1:
                                last = now
                                spd = have / max(now - t0, 1e-3)
                                sys.stdout.write("\r  %5.1f%%  %.2f/%.2f GB  %.1f MB/s   " % (
                                    100.0 * have / size, have / 1e9, size / 1e9, spd / 1e6))
                                sys.stdout.flush()
                except (urllib.error.URLError, OSError) as e:
                    log("\n  connection lost (%s), resuming in 5 s" % getattr(e, "reason", e))
                    time.sleep(5)
        sys.stdout.write("\n")
    else:
        log("already downloaded: %s" % path)
    if info.get("sha256"):
        log("checking sha256 ...")
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(8 << 20), b""):
                h.update(chunk)
        if h.hexdigest().lower() != info["sha256"].lower():
            os.rename(path, path + ".bad")
            raise FwError("sha256 mismatch: the download is corrupted (kept as %s.bad)" % path)
        log("sha256 ok")
    return path
