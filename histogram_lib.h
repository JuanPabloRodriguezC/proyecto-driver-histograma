#ifndef HISTOGRAM_LIB_H
#define HISTOGRAM_LIB_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the histogram display system
 * @return 0 on success, -1 on failure
 */
int histogram_init(void);

/**
 * @brief Display a histogram with 256 intensity values grouped into 32 columns
 * @param histogram Array of 256 intensity counts
 * @return 0 on success, -1 on failure
 */
int histogram_display_grouped(const uint32_t histogram[256]);

/**
 * @brief Display a sliding window of the histogram (32 consecutive intensities)
 * @param histogram Array of 256 intensity counts
 * @param start_intensity Starting intensity (0-224, must be multiple of 8)
 * @return 0 on success, -1 on failure
 */
int histogram_display_window(const uint32_t histogram[256], int start_intensity);

/**
 * @brief Set a specific pixel on the display
 * @param x X coordinate (0-31)
 * @param y Y coordinate (0-7)
 * @param on true to turn on, false to turn off
 * @return 0 on success, -1 on failure
 */
int matrix_set_pixel(int x, int y, bool on);

/**
 * @brief Update the display with current framebuffer
 * @return 0 on success, -1 on failure
 */
int matrix_update(void);

/**
 * @brief Clear the entire display
 * @return 0 on success, -1 on failure
 */
int histogram_clear(void);

/**
 * @brief Set display brightness
 * @param level Brightness level (0-15)
 * @return 0 on success, -1 on failure
 */
int histogram_set_brightness(int level);

/**
 * @brief Display a test pattern
 * @return 0 on success, -1 on failure
 */
int histogram_test_pattern(void);

/**
 * @brief Cleanup and close the histogram display
 */
void histogram_cleanup(void);

#endif // HISTOGRAM_LIB_H
