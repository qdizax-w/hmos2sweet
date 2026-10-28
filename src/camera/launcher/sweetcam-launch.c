/*
 * sweetcam-launch: runs cambridge (the Android camera HAL bridge) inside a
 * private mount namespace that looks like a minimal Android system:
 *
 *   /system  <- /vendor/android/system   (bionic linker/libc + a few AOSP libs)
 *   /vendor  <- /vendor/android/vendor   (sweet's camera HAL, CamX, gralloc, ...)
 *   /apex/com.android.runtime <- /vendor/android/apex/com.android.runtime
 *   /dev /proc /sys /data      shared with HarmonyOS
 *   /vendor/dsp                <- /vendor/dsp (DSP skeletons, dsp partition)
 *   /mnt/vendor/persist        <- the persist partition, read only (camera calibration)
 *
 * It also writes a read-only Android property area (/dev/__properties__,
 * pre-split format) with the vendor build.prop, the androidboot.* kernel
 * arguments as ro.boot.*, and a few values the HAL expects.
 *
 * cambridge is restarted (with back-off) if it dies.
 *
 * Static binary (no dependency on either libc of the device).
 * License: Apache-2.0
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define AROOT "/vendor/android"
#define NEWROOT "/dev/.sweetcam_root"
#ifndef PROPS
#define PROPS "/dev/__properties__"
#endif

static void klog(const char *fmt, ...)
{
    char b[512];
    int n = snprintf(b, sizeof(b), "<6>sweetcam-launch: ");
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(b + n, sizeof(b) - n, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(b) - 2)
        n = sizeof(b) - 2;
    b[n++] = '\n';
    int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        write(fd, b, n);
        close(fd);
    }
}

static void mkdirs(const char *p, mode_t m)
{
    char t[256];
    snprintf(t, sizeof(t), "%s", p);
    for (char *s = t + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            mkdir(t, m);
            *s = '/';
        }
    mkdir(t, m);
}

/* ------------------------------------------------ property area (bionic) */
#define PA_SIZE (128 * 1024)
#define PA_HDR 128
#define PROP_VALUE_MAX 92

struct trie_node {
    uint32_t namelen;
    uint32_t prop, left, right, children;
    char name[];
};

static uint8_t *pa;          /* whole file */
static uint8_t *pd;          /* data area */
static uint32_t pused;

static uint32_t pa_alloc(size_t sz)
{
    sz = (sz + 3) & ~(size_t)3;
    if (pused + sz > PA_SIZE - PA_HDR)
        return 0;
    uint32_t o = pused;
    pused += (uint32_t)sz;
    return o;
}

#define NODE(o) ((struct trie_node *)(pd + (o)))

static int pcmp(const char *a, uint32_t al, const char *b, uint32_t bl)
{
    if (al < bl)
        return -1;
    if (al > bl)
        return 1;
    return strncmp(a, b, al);
}

static uint32_t find_or_add(uint32_t parent, const char *n, uint32_t len)
{
    uint32_t *ref = &NODE(parent)->children;
    for (;;) {
        if (!*ref) {
            uint32_t o = pa_alloc(sizeof(struct trie_node) + len + 1);
            if (!o)
                return 0;
            struct trie_node *t = NODE(o);
            t->namelen = len;
            memcpy(t->name, n, len);
            t->name[len] = 0;
            *ref = o;
            return o;
        }
        struct trie_node *c = NODE(*ref);
        int r = pcmp(n, len, c->name, c->namelen);
        if (r == 0)
            return *ref;
        ref = r < 0 ? &c->left : &c->right;
    }
}

static void prop_set(const char *name, const char *value)
{
    size_t vl = strlen(value), nl = strlen(name);
    if (vl >= PROP_VALUE_MAX || nl == 0 || nl > 200)
        return;
    uint32_t node = 0;
    const char *s = name;
    for (;;) {
        const char *dot = strchr(s, '.');
        uint32_t len = dot ? (uint32_t)(dot - s) : (uint32_t)strlen(s);
        if (len == 0)
            return;
        node = find_or_add(node, s, len);
        if (!node)
            return;
        if (!dot)
            break;
        s = dot + 1;
    }
    struct trie_node *t = NODE(node);
    uint32_t po = t->prop;
    if (!po) {
        po = pa_alloc(4 + PROP_VALUE_MAX + nl + 1);
        if (!po)
            return;
        t = NODE(node);
        memcpy(pd + po + 4 + PROP_VALUE_MAX, name, nl + 1);
        t->prop = po;
    }
    uint32_t serial = (uint32_t)vl << 24;
    memcpy(pd + po, &serial, 4);
    memset(pd + po + 4, 0, PROP_VALUE_MAX);
    memcpy(pd + po + 4, value, vl);
}

static void load_prop_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || !*p)
            continue;
        if (!strncmp(p, "import ", 7))
            continue;
        char *eq = strchr(p, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        char *e = p + strlen(p);
        while (e > p && (e[-1] == ' ' || e[-1] == '\t'))
            *--e = 0;
        prop_set(p, v);
    }
    fclose(f);
}

static void cmdline_props(void)
{
    static char buf[4096];
    int fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0)
        return;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return;
    buf[n] = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, " \n", &save); tok; tok = strtok_r(NULL, " \n", &save)) {
        if (strncmp(tok, "androidboot.", 12))
            continue;
        char *eq = strchr(tok, '=');
        if (!eq)
            continue;
        *eq = 0;
        char name[128];
        snprintf(name, sizeof(name), "ro.boot.%s", tok + 12);
        prop_set(name, eq + 1);
    }
}

static int write_props(void)
{
    pa = calloc(1, PA_SIZE);
    if (!pa)
        return -1;
    pd = pa + PA_HDR;
    pused = sizeof(struct trie_node); /* the root node, empty name */
    /* defaults first, files override */
    prop_set("ro.hardware", "qcom");
    prop_set("ro.board.platform", "sm6150");
    prop_set("ro.product.device", "sweet");
    prop_set("ro.product.vendor.device", "sweet");
    prop_set("ro.product.name", "sweet");
    prop_set("ro.build.product", "sweet");
    prop_set("ro.product.manufacturer", "Xiaomi");
    prop_set("ro.product.vendor.manufacturer", "Xiaomi");
    prop_set("ro.product.brand", "Redmi");
    prop_set("ro.product.model", "Redmi Note 10 Pro");
    prop_set("ro.build.type", "user");
    prop_set("ro.debuggable", "0");
    prop_set("ro.boot.hwc", "Global");
    load_prop_file(AROOT "/system/build.prop");
    load_prop_file(AROOT "/vendor/build.prop");
    load_prop_file(AROOT "/vendor/default.prop");
    cmdline_props();
    prop_set("ro.hardware.camera", "qcom");
    prop_set("ro.hardware.gralloc", "qcom");
    prop_set("vendor.camera.hmos", "1");
    /* user overrides: /data/vendor/camera/sweetcam.prop */
    load_prop_file("/data/vendor/camera/sweetcam.prop");

    uint32_t hdr[4] = { pused, 1, 0x504f5250, 0xfc6ed0ab };
    memcpy(pa, hdr, sizeof(hdr));
    unlink(PROPS ".tmp");
    int fd = open(PROPS ".tmp", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0444);
    if (fd < 0)
        return -1;
    ssize_t w = write(fd, pa, PA_SIZE);
    fchown(fd, 0, 0);
    fchmod(fd, 0444);
    close(fd);
    free(pa);
    if (w != PA_SIZE)
        return -1;
    if (rename(PROPS ".tmp", PROPS))
        return -1;
    return 0;
}

/* ------------------------------------------------------- namespace */
static int bind(const char *src, const char *dst, int rec)
{
    if (mount(src, dst, NULL, MS_BIND | (rec ? MS_REC : 0), NULL)) {
        klog("bind %s -> %s: %s", src, dst, strerror(errno));
        return -1;
    }
    return 0;
}

static int setup_root(void)
{
    if (unshare(CLONE_NEWNS)) {
        klog("unshare: %s", strerror(errno));
        return -1;
    }
    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
    mkdirs(NEWROOT, 0755);
    if (mount("tmpfs", NEWROOT, "tmpfs", MS_NOSUID, "mode=0755,size=1m")) {
        klog("tmpfs: %s", strerror(errno));
        return -1;
    }
    const char *dirs[] = { "system", "vendor", "apex/com.android.runtime", "dev", "proc", "sys", "data",
        "linkerconfig", "tmp", "mnt/vendor/persist", "odm", "product", "system_ext", "acct", "config" };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char p[256];
        snprintf(p, sizeof(p), NEWROOT "/%s", dirs[i]);
        mkdirs(p, 0755);
    }
    if (bind(AROOT "/system", NEWROOT "/system", 0) || bind(AROOT "/vendor", NEWROOT "/vendor", 0) ||
        bind(AROOT "/apex/com.android.runtime", NEWROOT "/apex/com.android.runtime", 0))
        return -1;
    bind("/vendor/dsp", NEWROOT "/vendor/dsp", 0);
    bind("/dev", NEWROOT "/dev", 1);
    bind("/proc", NEWROOT "/proc", 1);
    bind("/sys", NEWROOT "/sys", 1);
    bind("/data", NEWROOT "/data", 1);
    mount("tmpfs", NEWROOT "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777,size=16m");
    /* camera calibration data */
    /* /vendor/persist is mounted by the remoteproc patch; otherwise mount the partition read only */
    if (access("/vendor/persist/camera", F_OK) != 0 ||
        mount("/vendor/persist", NEWROOT "/mnt/vendor/persist", NULL, MS_BIND, NULL)) {
        const char *persist[] = { "/dev/block/by-name/persist", "/dev/block/bootdevice/by-name/persist" };
        for (size_t i = 0; i < 2; i++)
            if (!mount(persist[i], NEWROOT "/mnt/vendor/persist", "ext4", MS_RDONLY | MS_NOSUID | MS_NODEV, NULL))
                break;
    }
    if (chroot(NEWROOT) || chdir("/")) {
        klog("chroot: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static volatile sig_atomic_t g_stop;
static void on_term(int s) { g_stop = 1; }

int main(int argc, char **argv)
{
    const char *bin = "/system/bin/cambridge";
    mkdirs("/data/vendor/camera", 0770);
    mkdirs("/data/log/sweetcam", 0770);
    mkdirs("/data/misc/camera", 0770);
    if (write_props())
        klog("could not write the property area: %s", strerror(errno));
    signal(SIGTERM, on_term);
    int fails = 0;
    while (!g_stop) {
        time_t t0 = time(NULL);
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGTERM, SIG_DFL);
            prctl(PR_SET_PDEATHSIG, SIGKILL);
            if (setup_root())
                _exit(127);
            setenv("PATH", "/system/bin:/vendor/bin", 1);
            setenv("LD_LIBRARY_PATH", "/vendor/lib64:/vendor/lib64/hw:/vendor/lib64/egl:/system/lib64", 1);
            setenv("ANDROID_ROOT", "/system", 1);
            setenv("ANDROID_DATA", "/data", 1);
            setenv("ADSP_LIBRARY_PATH", "/vendor/dsp/cdsp;/vendor/lib/rfsa/adsp;/vendor/dsp", 1);
            setenv("TMPDIR", "/tmp", 1);
            char *const args[] = { (char *)bin, argc > 1 ? argv[1] : NULL, NULL };
            execv(bin, args);
            klog("exec %s: %s", bin, strerror(errno));
            _exit(127);
        }
        if (pid < 0) {
            sleep(5);
            continue;
        }
        int st = 0;
        while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
            if (g_stop)
                kill(pid, SIGTERM);
        if (g_stop)
            break;
        if (WIFSIGNALED(st))
            klog("cambridge killed by signal %d", WTERMSIG(st));
        else
            klog("cambridge exited %d", WEXITSTATUS(st));
        if (argc > 1 && !strcmp(argv[1], "--probe"))
            break;
        fails = time(NULL) - t0 < 30 ? fails + 1 : 0;
        int delay = fails < 3 ? 2 : fails < 6 ? 15 : 120;
        sleep(delay);
    }
    return 0;
}
