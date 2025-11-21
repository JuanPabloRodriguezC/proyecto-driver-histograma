#ifndef HISTO_IOCTL_H
#define HISTO_IOCTL_H

#include <linux/ioctl.h>

#define HISTO_IOC_MAGIC 'H'

// Encender/apagar LED por indice
#define HISTO_IOC_LED_ON _IOW(HISTO_IOC_MAGIC, 0x01, int)
#define HISTO_IOC_LED_OFF _IOW(HISTO_IOC_MAGIC, 0x02, int)

// Apaga todo
#define HISTO_IOC_CLEAR _IO(HISTO_IOC_MAGIC, 0x03)

// Escribe histograma
// Espera un puntero a buffer
#define HISTO_IOC_BINS _IOW(HISTO_IOC_MAGIC, 0x10, void *)

// Lee estado del dispositivo
#define HISTO_IOC_STATUS _IOR(HISTO_IOC_MAGIC, 0x20, unsigned int)

#endif // HISTO_IOCTL_H