#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <stdint.h>

// RAW
uint8_t *load_raw_image(const char *path, int *width, int *height);
int save_raw_image(const char *path, int width, int height, const uint8_t *data);

// JPEG gris
uint8_t *load_jpeg_as_gray(const char *path, int *width, int *height);
int save_gray_as_jpeg(const char *path, int width, int height,
                      const uint8_t *data, int quality);

#endif // IMAGE_UTILS_H
