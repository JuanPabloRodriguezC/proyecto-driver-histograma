#ifndef HISTO_H
#define HISTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Ruta por defecto del device file expuesto por el driver
#define HISTO_DEFAULT_DEVICE "/dev/histodrv"

    typedef struct HistoContext HistoContext;
    typedef enum
    {
        HISTO_OK = 0,
        HISTO_ERR_ARG = -1,
        HISTO_ERR_OPEN = -2,
        HISTO_ERR_IOCTL = -3,
        HISTO_ERR_READ = -4,
        HISTO_ERR_WRITE = -5,
        HISTO_ERR_NOMEM = -6,
        HISTO_ERR_STATE = -7
    } histo_status_t;

#define HISTO_MAX_BINS 256

    // inicialización
    typedef struct
    {
        const char *device_path;
        bool simulator;       // true => usar backend simulado
        bool collect_metrics; // true => medir latencias de IOCTL/WRITE/READ
    } histo_options_t;

    typedef struct
    {
        uint64_t last_ioctl_ns;
        uint64_t last_write_ns;
        uint64_t last_read_ns;
    } histo_metrics_t;

    // API

    // Crea el contexto de la biblioteca
    histo_status_t histo_create(const histo_options_t *opts, HistoContext **out);

    // Libera el contexto
    void histo_destroy(HistoContext *ctx);

    // Cambia la ruta del device file en runtime si no esta abierto
    histo_status_t histo_set_device_path(HistoContext *ctx, const char *device_path);

    histo_status_t histo_open(HistoContext *ctx);

    void histo_close(HistoContext *ctx);

    // Devuelve el si descriptor de archivo aplica o -1
    int histo_fd(const HistoContext *ctx);

    // Escribe el histograma al hardware
    histo_status_t histo_display_bins(HistoContext *ctx, const uint32_t *bins, size_t count);

    histo_status_t histo_led_on(HistoContext *ctx, uint8_t index);
    histo_status_t histo_led_off(HistoContext *ctx, uint8_t index);
    histo_status_t histo_clear(HistoContext *ctx);

    histo_status_t histo_read_status(HistoContext *ctx, uint32_t *out_flags);

    // accede a metricas
    void histo_get_metrics(const HistoContext *ctx, histo_metrics_t *out);

#ifdef __cplusplus
}
#endif

#endif // HISTO_H