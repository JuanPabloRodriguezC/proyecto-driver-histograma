#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "histogram_lib.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void compute_histogram(unsigned char *image_data, int width, int height, int channels, int histogram[256]) {
    // Initialize histogram to zero
    memset(histogram, 0, 256 * sizeof(int));
    
    // Count pixel intensities
    int total_pixels = width * height;
    for (int i = 0; i < total_pixels; i++) {
        // Calculate grayscale value from RGB
        int pixel_index = i * channels;
        int gray_value;
        
        if (channels >= 3) {
            // Convert RGB to grayscale using luminance formula
            int r = image_data[pixel_index];
            int g = image_data[pixel_index + 1];
            int b = image_data[pixel_index + 2];
            gray_value = (int)(0.299 * r + 0.587 * g + 0.114 * b);
        } else {
            // Grayscale image
            gray_value = image_data[pixel_index];
        }
        
        histogram[gray_value]++;
    }
}

void print_histogram(int histogram[256]) {
    printf("Histogram (Intensity: Count):\n");
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            printf("%3d: %d\n", i, histogram[i]);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <image_file>\n", argv[0]);
        return 1;
    }


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

    histogram_clear();
    sleep(1);
    
    int width, height, channels;
    unsigned char *image_data = stbi_load(argv[1], &width, &height, &channels, 0);
    
    if (image_data == NULL) {
        printf("Error: Could not load image '%s'\n", argv[1]);
        return 1;
    }
    
    printf("Image loaded: %dx%d, %d channels\n", width, height, channels);
    
    // Create histogram array
    int histogram[256];
    
    // Compute histogram
    compute_histogram(image_data, width, height, channels, histogram);



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

    // Clean up
    stbi_image_free(image_data);
    
    return 0;
}