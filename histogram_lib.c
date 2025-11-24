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
static histogram_hw_config_t hw_config = {0};

int histogram_init(void)
{
    driver_fd = open(DRIVER_PATH, O_RDWR);
    if (driver_fd < 0) {
        perror("Failed to open MAX7219 driver");
        return -1;
    }
    
    // Read hardware configuration
    if (histogram_get_hw_config(&hw_config) < 0) {
        close(driver_fd);
        driver_fd = -1;
        return -1;
    }
    
    printf("Hardware detected: %d matrices, %dx%d resolution\n",
           hw_config.matrices, hw_config.width, hw_config.height);
    
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

int histogram_get_hw_config(histogram_hw_config_t *config)
{
    char buffer[256];
    ssize_t bytes_read;
    int fd;
    
    if (config == NULL) {
        fprintf(stderr, "Invalid config pointer\n");
        return -1;
    }
    
    // Open driver for reading configuration
    // Note: we open a new fd here because /proc files don't support lseek
    fd = open(DRIVER_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open driver for reading config");
        return -1;
    }
    
    // Read from driver
    memset(buffer, 0, sizeof(buffer));
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    
    // Close this temporary fd
    close(fd);
    
    if (bytes_read < 0) {
        perror("Failed to read hardware config");
        return -1;
    }
    
    if (bytes_read == 0) {
        fprintf(stderr, "No data read from driver\n");
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    
    // Debug: print what we read
    printf("Read from driver (%zd bytes): %s", bytes_read, buffer);
    
    // Parse configuration: "matrices=4\nwidth=32\nheight=8\n"
    if (sscanf(buffer, "matrices=%d\nwidth=%d\nheight=%d",
               &config->matrices, &config->width, &config->height) != 3) {
        fprintf(stderr, "Failed to parse hardware configuration\n");
        fprintf(stderr, "Buffer content: '%s'\n", buffer);
        return -1;
    }
    
    return 0;
}

int histogram_dimension(const uint32_t input_histogram[256], 
                       uint8_t *output_histogram,
                       int hw_width,
                       int hw_height)
{
    int i, col;
    uint32_t max_value = 0;
    uint32_t *grouped;
    int bucket_size;
    
    if (input_histogram == NULL || output_histogram == NULL) {
        fprintf(stderr, "Invalid histogram pointers\n");
        return -1;
    }
    
    if (hw_width <= 0 || hw_width > 256 || hw_height <= 0) {
        fprintf(stderr, "Invalid hardware dimensions: %dx%d\n", hw_width, hw_height);
        return -1;
    }
    
    // Allocate temporary grouped array
    grouped = calloc(hw_width, sizeof(uint32_t));
    if (grouped == NULL) {
        perror("Failed to allocate memory");
        return -1;
    }
    
    // Calculate bucket size (how many intensities per column)
    bucket_size = 256 / hw_width;
    if (bucket_size == 0) bucket_size = 1;
    
    // Group intensities into buckets
    for (i = 0; i < 256; i++) {
        col = i / bucket_size;
        if (col >= hw_width) col = hw_width - 1;
        grouped[col] += input_histogram[i];
    }
    
    // Find maximum value for scaling
    for (col = 0; col < hw_width; col++) {
        if (grouped[col] > max_value)
            max_value = grouped[col];
    }
    
    // Scale to hardware height
    for (col = 0; col < hw_width; col++) {
        if (max_value == 0) {
            output_histogram[col] = 0;
        } else {
            uint32_t scaled = (grouped[col] * (uint32_t)hw_height) / max_value;
            if (scaled > (uint32_t)hw_height) scaled = (uint32_t)hw_height;
            output_histogram[col] = (uint8_t)scaled;
        }
    }
    
    free(grouped);
    return 0;
}

int histogram_display(const uint8_t *histogram, int width)
{
    char buffer[BUFFER_SIZE];
    ssize_t written;
    size_t total_size;
    uint8_t *rotated;
    int i;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    if (histogram == NULL) {
        fprintf(stderr, "Invalid histogram pointer\n");
        return -1;
    }
    
    // Verify width matches hardware
    if (width != hw_config.width) {
        fprintf(stderr, "Histogram width (%d) doesn't match hardware width (%d)\n",
                width, hw_config.width);
        return -1;
    }
    
    // Allocate buffer for rotated histogram
    // After 90° CW rotation, the display shows the histogram rotated
    // Width becomes height, so we need width entries still
    rotated = malloc(width * sizeof(uint8_t));
    if (rotated == NULL) {
        perror("Failed to allocate memory for rotation");
        return -1;
    }
    
    // Rotate 90 degrees clockwise
    // Original histogram: histogram[0..31] with heights 0..8
    // Displayed as vertical bars from bottom to top
    //
    // After 90° CW rotation:
    // - What was column 0 (leftmost) becomes row 0 (bottom)
    // - What was column 31 (rightmost) becomes row 31 (top)
    // - Vertical bars become horizontal bars extending right
    //
    // So: rotated[i] = histogram[width - 1 - i]
    // This reverses the order (rightmost becomes bottom)
    
    for (i = 0; i < width; i++) {
        rotated[i] = histogram[width - 1 - i];
    }
    
    // Prepare command: "histogram " followed by width bytes
    strcpy(buffer, "histogram ");
    memcpy(buffer + strlen(buffer), rotated, width);
    
    total_size = strlen("histogram ") + width;
    
    written = write(driver_fd, buffer, total_size);
    
    free(rotated);
    
    if (written < 0) {
        perror("Failed to write histogram data");
        return -1;
    }
    
    if ((size_t)written != total_size) {
        fprintf(stderr, "Incomplete write: %zd of %zu bytes\n", written, total_size);
        return -1;
    }
    
    return 0;
}

int histogram_display_auto(const uint32_t histogram[256])
{
    uint8_t *dimensioned;
    int result;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    if (histogram == NULL) {
        fprintf(stderr, "Invalid histogram pointer\n");
        return -1;
    }
    
    // Allocate space for dimensioned histogram
    dimensioned = malloc(hw_config.width * sizeof(uint8_t));
    if (dimensioned == NULL) {
        perror("Failed to allocate memory");
        return -1;
    }
    
    // Dimension the histogram
    result = histogram_dimension(histogram, dimensioned, 
                                 hw_config.width, hw_config.height);
    if (result < 0) {
        free(dimensioned);
        return -1;
    }
    
    // Display it
    result = histogram_display(dimensioned, hw_config.width);
    
    free(dimensioned);
    return result;
}

int matrix_set_pixel(int x, int y, bool on)
{
    char buffer[64];
    ssize_t written;
    int rotated_x, rotated_y;
    
    if (driver_fd < 0) {
        fprintf(stderr, "Driver not initialized\n");
        return -1;
    }
    
    if (x < 0 || x >= hw_config.width || y < 0 || y >= hw_config.height) {
        fprintf(stderr, "Invalid pixel coordinates: (%d, %d)\n", x, y);
        return -1;
    }
    
    // Rotate 90 degrees clockwise
    // Formula for 90° CW rotation:
    // new_x = old_y
    // new_y = (width - 1) - old_x
    rotated_x = y;
    rotated_y = (hw_config.width - 1) - x;
    
    snprintf(buffer, sizeof(buffer), "pixel %d %d %d", rotated_x, rotated_y, on ? 1 : 0);
    
    written = write(driver_fd, buffer, strlen(buffer));
    if (written < 0) {
        perror("Failed to set pixel");
        return -1;
    }
    
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