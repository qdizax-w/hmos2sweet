/*
 * libconsumerir.so for sweet: Huawei's consumer IR HDI service
 * (libconsumer_ir_service_1.0.z.so) dlopens this library and calls
 * consumerir_open(); the returned device is a table of four functions.
 * sweet has an IR LED on SPI (ir-spi, LIRC): /dev/lirc0.
 *
 * Copyright (c) 2026 hmos2sweet. Apache-2.0.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define LIRC_SET_SEND_CARRIER _IOW('i', 0x00000013, uint32_t)
#define LIRC_SET_SEND_DUTY_CYCLE _IOW('i', 0x00000015, uint32_t)

struct freq_range {
    int min;
    int max;
};

struct consumerir_device {
    int (*transmit)(struct consumerir_device *dev, int carrier_freq, const int *pattern, size_t count);
    int (*get_num_carrier_freqs)(struct consumerir_device *dev);
    int (*get_carrier_freqs)(struct consumerir_device *dev, size_t count, struct freq_range *ranges);
    void *reserved[6];
};

static const struct freq_range g_ranges[] = {{30000, 60000}};

static int ir_transmit(struct consumerir_device *dev, int carrier_freq, const int *pattern, size_t count)
{
    (void)dev;
    if (pattern == NULL || count == 0 || carrier_freq <= 0) {
        return -EINVAL;
    }
    int fd = open("/dev/lirc0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }
    uint32_t freq = (uint32_t)carrier_freq;
    ioctl(fd, LIRC_SET_SEND_CARRIER, &freq);
    uint32_t duty = 33;
    ioctl(fd, LIRC_SET_SEND_DUTY_CYCLE, &duty);
    /* LIRC wants an odd number of pulse/space durations (ends on a pulse) */
    size_t n = (count & 1) ? count : count - 1;
    if (n == 0) {
        close(fd);
        return -EINVAL;
    }
    uint32_t *buf = malloc(n * sizeof(uint32_t));
    if (buf == NULL) {
        close(fd);
        return -ENOMEM;
    }
    for (size_t i = 0; i < n; i++) {
        buf[i] = pattern[i] > 0 ? (uint32_t)pattern[i] : 1;
    }
    ssize_t w = write(fd, buf, n * sizeof(uint32_t));
    int err = w < 0 ? -errno : 0;
    free(buf);
    close(fd);
    return err;
}

static int ir_get_num_carrier_freqs(struct consumerir_device *dev)
{
    (void)dev;
    return (int)(sizeof(g_ranges) / sizeof(g_ranges[0]));
}

static int ir_get_carrier_freqs(struct consumerir_device *dev, size_t count, struct freq_range *ranges)
{
    (void)dev;
    size_t n = sizeof(g_ranges) / sizeof(g_ranges[0]);
    if (ranges == NULL) {
        return (int)n;
    }
    if (count > n) {
        count = n;
    }
    memcpy(ranges, g_ranges, count * sizeof(*ranges));
    return (int)count;
}

__attribute__((visibility("default"))) int consumerir_open(struct consumerir_device **out)
{
    if (out == NULL) {
        return -EINVAL;
    }
    struct consumerir_device *dev = calloc(1, sizeof(*dev));
    if (dev == NULL) {
        return -ENOMEM;
    }
    dev->transmit = ir_transmit;
    dev->get_num_carrier_freqs = ir_get_num_carrier_freqs;
    dev->get_carrier_freqs = ir_get_carrier_freqs;
    *out = dev;
    return 0;
}

__attribute__((visibility("default"))) int consumerir_close(struct consumerir_device *dev)
{
    free(dev);
    return 0;
}
