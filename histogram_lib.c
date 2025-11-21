#include "histogram_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DRIVER_PATH "/proc/max7219"
#define BUFFER_SIZE 4096

static int driver_fd = -1;

int histogram_init(void)
{
    driver_fd = open(DRIVER_PATH, O_RDWR);
    if (driver_fd < 0) {
        perror("Failed to open MAX7219 driver");
        return -1;
    }
    
    // Clear display on init
    histogram_clear();
    
    return 0;
}

void histogram_cleanup(void)
{
    if (driver_fd >= 0) {
        histogram_clear();
        close(driver_fd);
        driver_fd = -1;
    }
}

int histogram_display_grouped(const uint32_t histogram[256])
{
    char buffer[BUFFER_SIZE];
    ssize_t written;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    if (histogram == NULL) {
        fprintf(stderr, "Invalid histogram pointer\n");
        return -1;
    }
    
    // Prepare command with histogram data
    strcpy(buffer, "histogram ");
    memcpy(buffer + strlen(buffer), histogram, 256 * sizeof(uint32_t));
    
    written = write(driver_fd, buffer, strlen("histogram ") + 256 * sizeof(uint32_t));
    if (written < 0) {
        perror("Failed to write histogram data");
        return -1;
    }
    
    return 0;
}

int histogram_display_window(const uint32_t histogram[256], int start_intensity)
{
    uint32_t window[256] = {0};
    int i, col;
    uint32_t max_value = 0;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    if (histogram == NULL) {
        fprintf(stderr, "Invalid histogram pointer\n");
        return -1;
    }
    
    // Validate start_intensity
    if (start_intensity < 0 || start_intensity > 224 || start_intensity % 8 != 0) {
        fprintf(stderr, "Invalid start_intensity: must be 0-224 and multiple of 8\n");
        return -1;
    }
    
    // Extract window and scale to display range
    // We'll group 8 consecutive intensities per column (32 columns total)
    for (i = 0; i < 32; i++) {
        int base_intensity = start_intensity + (i * 8);
        for (col = 0; col < 8 && base_intensity + col < 256; col++) {
            window[i * 8 + col] = histogram[base_intensity + col];
        }
    }
    
    // Use the grouped display function
    return histogram_display_grouped(window);
}

int matrix_set_pixel(int x, int y, bool on)
{
    char buffer[64];
    ssize_t written;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    if (x < 0 || x >= 32 || y < 0 || y >= 8) {
        fprintf(stderr, "Invalid pixel coordinates: (%d, %d)\n", x, y);
        return -1;
    }
    
    snprintf(buffer, sizeof(buffer), "pixel %d %d %d", x, y, on ? 1 : 0);
    
    written = write(driver_fd, buffer, strlen(buffer));
    if (written < 0) {
        perror("Failed to set pixel");
        return -1;
    }
    
    return 0;
}

int matrix_update(void)
{
    // In this implementation, the driver updates automatically
    // This function is here for API compatibility
    return 0;
}

int histogram_clear(void)
{
    const char *cmd = "clear";
    ssize_t written;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    written = write(driver_fd, cmd, strlen(cmd));
    if (written < 0) {
        perror("Failed to clear display");
        return -1;
    }
    
    return 0;
}

int histogram_set_brightness(int level)
{
    char buffer[32];
    ssize_t written;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    if (level < 0 || level > 15) {
        fprintf(stderr, "Invalid brightness level: %d (must be 0-15)\n", level);
        return -1;
    }
    
    snprintf(buffer, sizeof(buffer), "intensity %d", level);
    
    written = write(driver_fd, buffer, strlen(buffer));
    if (written < 0) {
        perror("Failed to set brightness");
        return -1;
    }
    
    return 0;
}

int histogram_test_pattern(void)
{
    const char *cmd = "test";
    ssize_t written;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    written = write(driver_fd, cmd, strlen(cmd));
    if (written < 0) {
        perror("Failed to display test pattern");
        return -1;
    }
    
    return 0;
}
