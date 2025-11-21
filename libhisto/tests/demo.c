#include "histo.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    HistoContext *ctx = NULL;
    histo_options_t opts = {
        .device_path = HISTO_DEFAULT_DEVICE,
        .simulator = true, // cambiar a false en RPi con driver cargado
        .collect_metrics = true};

    if (histo_create(&opts, &ctx) != HISTO_OK)
    {
        fprintf(stderr, "histo_create failed\n");
        return 1;
    }
    if (histo_open(ctx) != HISTO_OK)
    {
        fprintf(stderr, "histo_open failed\n");
        return 1;
    }

    // Demostración: encender LED 0, escribir bins, limpiar
    (void)histo_led_on(ctx, 0);

    uint32_t bins[HISTO_MAX_BINS];
    for (size_t i = 0; i < HISTO_MAX_BINS; ++i)
        bins[i] = (i % 32) * 4;
    (void)histo_display_bins(ctx, bins, HISTO_MAX_BINS);

    (void)histo_clear(ctx);

    histo_metrics_t m;
    histo_get_metrics(ctx, &m);
    printf("last_ioctl_ns=%llu last_write_ns=%llu last_read_ns=%llu\n",
           (unsigned long long)m.last_ioctl_ns,
           (unsigned long long)m.last_write_ns,
           (unsigned long long)m.last_read_ns);

    histo_destroy(ctx);
    return 0;
}