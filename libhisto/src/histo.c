#include "histo.h"
#include "histo_ioctl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct HistoContext
{
    char *device_path;
    int fd;
    int simulator; // 0 = real, 1 = simulado
    int collect_metrics;
    histo_metrics_t m;
};

// Obtiene el tiempo actual en microsegundos
int64_t now_us(void)
{
    struct timeval tv;
    struct tm *tm_info;
    time_t now_sec;
    int minute, second, decisecond;
    gettimeofday(&tv, NULL);

    now_sec = tv.tv_sec;
    tm_info = localtime(&now_sec);

    minute = tm_info->tm_min;
    second = tm_info->tm_sec;

    decisecond = tv.tv_usec / 100000;

    return (int64_t)(minute * 1000 + second * 10 + decisecond);
}

static char *histo_str_dup(const char *s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char *p = (char *)malloc(len);
    if (!p)
        return NULL;
    memcpy(p, s, len);
    return p;
}

histo_status_t histo_create(const histo_options_t *opts, HistoContext **out)
{
    if (!out)
        return HISTO_ERR_ARG;
    HistoContext *ctx = (HistoContext *)calloc(1, sizeof(*ctx));
    if (!ctx)
        return HISTO_ERR_NOMEM;

    const char *path = (opts && opts->device_path) ? opts->device_path : HISTO_DEFAULT_DEVICE;
    ctx->device_path = histo_str_dup(path);
    if (!ctx->device_path)
    {
        free(ctx);
        return HISTO_ERR_NOMEM;
    }

    ctx->fd = -1;
    ctx->simulator = (opts && opts->simulator) ? 1 : 0;
    ctx->collect_metrics = (opts && opts->collect_metrics) ? 1 : 0;
    *out = ctx;
    return HISTO_OK;
}

void histo_destroy(HistoContext *ctx)
{
    if (!ctx)
        return;
    histo_close(ctx);
    free(ctx->device_path);
    free(ctx);
}

histo_status_t histo_set_device_path(HistoContext *ctx, const char *device_path)
{
    if (!ctx || !device_path)
        return HISTO_ERR_ARG;
    if (ctx->fd != -1)
        return HISTO_ERR_STATE; // no se puede cambiar con el device abierto
    free(ctx->device_path);
    ctx->device_path = histo_str_dup(device_path);
    return ctx->device_path ? HISTO_OK : HISTO_ERR_NOMEM;
}

histo_status_t histo_open(HistoContext *ctx)
{
    if (!ctx)
        return HISTO_ERR_ARG;
    if (ctx->fd != -1)
        return HISTO_OK; // ya abierto

    if (ctx->simulator)
    {
        // Backend simulado no usa fd real
        ctx->fd = -1;
        return HISTO_OK;
    }

    int fd = open(ctx->device_path, O_RDWR);
    if (fd < 0)
    {
        perror("open device");
        return HISTO_ERR_OPEN;
    }
    ctx->fd = fd;
    return HISTO_OK;
}

void histo_close(HistoContext *ctx)
{
    if (!ctx)
        return;
    if (!ctx->simulator && ctx->fd != -1)
    {
        close(ctx->fd);
    }
    ctx->fd = -1;
}

int histo_fd(const HistoContext *ctx)
{
    return ctx ? ctx->fd : -1;
}

static histo_status_t do_ioctl(HistoContext *ctx, unsigned long req, void *arg)
{
    if (!ctx)
        return HISTO_ERR_ARG;
    uint64_t t0 = 0, t1 = 0;
    if (ctx->collect_metrics)
        t0 = now_us();

    int rc;
    if (ctx->simulator)
    {
        // Efectos simulados minimos
        rc = 0;
    }
    else
    {
        rc = ioctl(ctx->fd, req, arg);
    }

    if (ctx->collect_metrics)
    {
        t1 = now_us();
        ctx->m.last_ioctl_ns = (t1 - t0);
    }
    return (rc == 0) ? HISTO_OK : HISTO_ERR_IOCTL;
}

histo_status_t histo_led_on(HistoContext *ctx, uint8_t index)
{
    int idx = (int)index;
    return do_ioctl(ctx, HISTO_IOC_LED_ON, &idx);
}

histo_status_t histo_led_off(HistoContext *ctx, uint8_t index)
{
    int idx = (int)index;
    return do_ioctl(ctx, HISTO_IOC_LED_OFF, &idx);
}

histo_status_t histo_clear(HistoContext *ctx)
{
    return do_ioctl(ctx, HISTO_IOC_CLEAR, NULL);
}

histo_status_t histo_display_bins(HistoContext *ctx, const uint32_t *bins, size_t count)
{
    if (!ctx || !bins || count == 0 || count > HISTO_MAX_BINS)
        return HISTO_ERR_ARG;
    uint64_t t0 = 0, t1 = 0;
    if (ctx->collect_metrics)
        t0 = now_us();

    int rc;
    if (ctx->simulator)
    {
        // Simulacion validar entrada
        rc = 0;
    }
    else
    {
        // El driver espera el puntero de usuario
        rc = ioctl(ctx->fd, HISTO_IOC_BINS, (void *)bins);
    }

    if (ctx->collect_metrics)
    {
        t1 = now_us();
        ctx->m.last_write_ns = (t1 - t0);
    }
    return (rc == 0) ? HISTO_OK : HISTO_ERR_WRITE;
}

histo_status_t histo_read_status(HistoContext *ctx, uint32_t *out_flags)
{
    if (!ctx || !out_flags)
        return HISTO_ERR_ARG;
    uint64_t t0 = 0, t1 = 0;
    if (ctx->collect_metrics)
        t0 = now_us();

    int rc;
    if (ctx->simulator)
    {
        *out_flags = 0u; // ejemplo
        rc = 0;
    }
    else
    {
        unsigned int flags = 0;
        rc = ioctl(ctx->fd, HISTO_IOC_STATUS, &flags);
        if (rc == 0)
            *out_flags = flags;
    }

    if (ctx->collect_metrics)
    {
        t1 = now_us();
        ctx->m.last_read_ns = (t1 - t0);
    }
    return (rc == 0) ? HISTO_OK : HISTO_ERR_READ;
}

void histo_get_metrics(const HistoContext *ctx, histo_metrics_t *out)
{
    if (!ctx || !out)
        return;
    *out = ctx->m;
}