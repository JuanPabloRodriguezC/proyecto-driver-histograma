#include "histogram_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

void print_histogram_info(histogram_hw_config_t *config)
{
    printf("\n=== Hardware Configuration ===\n");
    printf("Matrices: %d\n", config->matrices);
    printf("Display size: %d x %d pixels\n", config->width, config->height);
    printf("Total pixels: %d\n", config->width * config->height);
    printf("=============================\n\n");
}

void create_sample_histogram(uint32_t histogram[256])
{
    int i;
    
    // Create a sample histogram with Gaussian-like distribution
    for (i = 0; i < 256; i++) {
        double x = (i - 128.0) / 64.0;
        histogram[i] = (uint32_t)(1000.0 * exp(-x * x));
    }
}

void create_bimodal_histogram(uint32_t histogram[256])
{
    int i;
    
    // Create a bimodal distribution
    for (i = 0; i < 256; i++) {
        double x1 = (i - 80.0) / 40.0;
        double x2 = (i - 180.0) / 40.0;
        histogram[i] = (uint32_t)(500.0 * (exp(-x1 * x1) + exp(-x2 * x2)));
    }
}

void create_uniform_histogram(uint32_t histogram[256])
{
    int i;
    
    // Create uniform distribution
    for (i = 0; i < 256; i++) {
        histogram[i] = 500 + (rand() % 200);
    }
}

void print_dimensioned_histogram(uint8_t *hist, int width)
{
    int i, j;
    
    printf("\nDimensioned histogram values:\n");
    for (i = 0; i < width; i++) {
        printf("[%2d]: %d ", i, hist[i]);
        for (j = 0; j < hist[i]; j++) {
            printf("█");
        }
        printf("\n");
    }
    printf("\n");
}

int main(void)
{
    histogram_hw_config_t config;
    uint32_t histogram[256];
    uint8_t *dimensioned;
    int result;
    
    printf("=== MAX7219 Histogram Library Test ===\n\n");
    
    // Initialize library
    printf("Initializing histogram display...\n");
    result = histogram_init();
    if (result < 0) {
        fprintf(stderr, "Failed to initialize histogram display\n");
        return 1;
    }
    printf("✓ Initialization successful\n");
    
    // Get hardware configuration
    result = histogram_get_hw_config(&config);
    if (result < 0) {
        fprintf(stderr, "Failed to get hardware configuration\n");
        histogram_cleanup();
        return 1;
    }
    print_histogram_info(&config);
    
    // Allocate dimensioned histogram
    dimensioned = malloc(config.width * sizeof(uint8_t));
    if (dimensioned == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        histogram_cleanup();
        return 1;
    }
    
    // Test 1: Clear display
    printf("Test 1: Clearing display...\n");
    histogram_clear();
    sleep(2);
    printf("✓ Display cleared\n\n");
    
    // Test 2: Test pattern
    printf("Test 2: Displaying test pattern...\n");
    histogram_test_pattern();
    sleep(3);
    printf("✓ Test pattern displayed\n\n");
    
    histogram_clear();
    sleep(1);
    
    // Test 3: Gaussian histogram
    printf("Test 3: Displaying Gaussian distribution...\n");
    create_sample_histogram(histogram);
    
    result = histogram_dimension(histogram, dimensioned, config.width, config.height);
    if (result < 0) {
        fprintf(stderr, "Failed to dimension histogram\n");
    } else {
        print_dimensioned_histogram(dimensioned, config.width);
        result = histogram_display(dimensioned, config.width);
        if (result < 0) {
            fprintf(stderr, "Failed to display histogram\n");
        } else {
            printf("✓ Gaussian histogram displayed\n");
        }
    }
    sleep(4);
    
    // Test 4: Bimodal histogram using auto function
    printf("\nTest 4: Displaying bimodal distribution (auto)...\n");
    create_bimodal_histogram(histogram);
    
    result = histogram_display_auto(histogram);
    if (result < 0) {
        fprintf(stderr, "Failed to auto-display histogram\n");
    } else {
        printf("✓ Bimodal histogram displayed\n");
    }
    sleep(4);
    
    // Test 5: Uniform histogram
    printf("\nTest 5: Displaying uniform distribution...\n");
    create_uniform_histogram(histogram);
    
    result = histogram_dimension(histogram, dimensioned, config.width, config.height);
    if (result == 0) {
        print_dimensioned_histogram(dimensioned, config.width);
        result = histogram_display(dimensioned, config.width);
        if (result == 0) {
            printf("✓ Uniform histogram displayed\n");
        }
    }
    sleep(4);
    
    // Test 6: Brightness control
    printf("\nTest 6: Testing brightness levels...\n");
    for (int brightness = 0; brightness <= 15; brightness += 5) {
        printf("  Setting brightness to %d/15...\n", brightness);
        histogram_set_brightness(brightness);
        sleep(1);
    }
    histogram_set_brightness(8);  // Return to medium
    printf("✓ Brightness test complete\n");
    sleep(2);
    
    // Test 7: Manual pixel control
    printf("\nTest 7: Manual pixel drawing...\n");
    histogram_clear();
    sleep(1);
    
    // Draw a pattern
    for (int x = 0; x < config.width; x++) {
        int y = (int)(config.height / 2 + (config.height / 3) * sin(x * 0.5));
        if (y >= 0 && y < config.height) {
            matrix_set_pixel(x, y, true);
        }
    }
    printf("✓ Sine wave pattern drawn\n");
    sleep(4);
    
    // Clean up
    printf("\nCleaning up...\n");
    histogram_clear();
    free(dimensioned);
    histogram_cleanup();
    
    printf("\n=== All tests completed successfully ===\n");
    
    return 0;
}