#ifndef HISTOGRAM_LIB_H
#define HISTOGRAM_LIB_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Hardware configuration structure
 */
typedef struct {
    int matrices;
    int width;
    int height;
} histogram_hw_config_t;

/**
 * @brief Initialize the histogram display system
 * @return 0 on success, -1 on failure
 */
int histogram_init(void);

/**
 * @brief Get hardware configuration from driver
 * @param config Pointer to store hardware configuration
 * @return 0 on success, -1 on failure
 */
int histogram_get_hw_config(histogram_hw_config_t *config);

/**
 * @brief Create dimensioned histogram from 256-value histogram
 * 
 * Takes a full 256-value histogram and creates a dimensioned version
 * that fits the hardware display dimensions. Groups intensities into
 * buckets matching the display width and scales heights to fit display height.
 * 
 * @param input_histogram Input histogram with 256 intensity values
 * @param output_histogram Output array (must be allocated with size = hw_width)
 * @param hw_width Hardware display width (number of columns)
 * @param hw_height Hardware display height (maximum bar height)
 * @return 0 on success, -1 on failure
 */
int histogram_dimension(const uint32_t input_histogram[256], 
                       uint8_t *output_histogram,
                       int hw_width,
                       int hw_height);

/**
 * @brief Display a pre-dimensioned histogram on hardware
 * 
 * Sends a histogram that has already been dimensioned to match the
 * hardware specifications. The histogram should contain height values
 * (0 to hw_height) for each column.
 * 
 * NOTE: This function automatically applies a 90° clockwise rotation
 * to accommodate the physical orientation of the display hardware.
 * Users provide data in logical orientation, and the library handles
 * the transformation transparently.
 * 
 * @param histogram Dimensioned histogram array (size = hw_width)
 * @param width Width of the histogram (must match hardware width)
 * @return 0 on success, -1 on failure
 */
int histogram_display(const uint8_t *histogram, int width);

/**
 * @brief Display a 256-value histogram (convenience function)
 * 
 * Automatically dimensions and displays a full 256-value histogram.
 * This is a convenience function that combines histogram_dimension()
 * and histogram_display().
 * 
 * @param histogram Full 256-value histogram
 * @return 0 on success, -1 on failure
 */
int histogram_display_auto(const uint32_t histogram[256]);

/**
 * @brief Set a specific pixel on the display
 * 
 * NOTE: Coordinates are in logical orientation. This function automatically
 * applies a 90° clockwise rotation to match the physical display orientation.
 * Users work with logical coordinates (x=0 is left, y=0 is top), and the
 * library handles the physical transformation.
 * 
 * @param x X coordinate (0 to width-1) in logical orientation
 * @param y Y coordinate (0 to height-1) in logical orientation
 * @param on true to turn on, false to turn off
 * @return 0 on success, -1 on failure
 */
int matrix_set_pixel(int x, int y, bool on);


int histogram_test_pattern(void);

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
 * @brief Cleanup and close the histogram display
 */
void histogram_cleanup(void);

#endif // HISTOGRAM_LIB_H