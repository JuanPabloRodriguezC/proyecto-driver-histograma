#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "histogram_lib.h"
#include <math.h>
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Function to generate a sample histogram
void generate_sample_histogram(uint32_t histogram[256], const char *type)
{
    int i;
    srand(time(NULL));
    
    memset(histogram, 0, 256 * sizeof(uint32_t));
    
    if (strcmp(type, "uniform") == 0) {
        // Uniform distribution
        for (i = 0; i < 256; i++) {
            histogram[i] = 100 + (rand() % 50);
        }
    }
    else if (strcmp(type, "gaussian") == 0) {
        // Gaussian-like distribution centered at 128
        for (i = 0; i < 256; i++) {
            int diff = i - 128;
            histogram[i] = 1000 * exp(-(diff * diff) / 2048.0);
        }
    }
    else if (strcmp(type, "bimodal") == 0) {
        // Bimodal distribution (peaks at 64 and 192)
        for (i = 0; i < 256; i++) {
            int diff1 = i - 64;
            int diff2 = i - 192;
            histogram[i] = 500 * exp(-(diff1 * diff1) / 1024.0) +
                          500 * exp(-(diff2 * diff2) / 1024.0);
        }
    }
    else if (strcmp(type, "increasing") == 0) {
        // Linearly increasing
        for (i = 0; i < 256; i++) {
            histogram[i] = i * 4;
        }
    }
    else if (strcmp(type, "decreasing") == 0) {
        // Linearly decreasing
        for (i = 0; i < 256; i++) {
            histogram[i] = (255 - i) * 4;
        }
    }
    else {
        // Random
        for (i = 0; i < 256; i++) {
            histogram[i] = rand() % 1000;
        }
    }
}

void print_menu(void)
{
    printf("\n=== MAX7219 Histogram Display Test ===\n");
    printf("1. Display uniform histogram\n");
    printf("2. Display gaussian histogram\n");
    printf("3. Display bimodal histogram\n");
    printf("4. Display increasing histogram\n");
    printf("5. Display decreasing histogram\n");
    printf("6. Display random histogram\n");
    printf("7. Display windowed view (scroll)\n");
    printf("8. Test pattern\n");
    printf("9. Clear display\n");
    printf("10. Set brightness\n");
    printf("11. Draw custom pattern\n");
    printf("0. Exit\n");
    printf("Choice: ");
}

void test_windowed_display(void)
{
    uint32_t histogram[256];
    int start;
    
    printf("Generating sample data...\n");
    generate_sample_histogram(histogram, "gaussian");
    
    printf("Scrolling through histogram windows...\n");
    for (start = 0; start <= 224; start += 32) {
        printf("Window starting at intensity %d\n", start);
        if (histogram_display_window(histogram, start) < 0) {
            fprintf(stderr, "Failed to display window\n");
            return;
        }
        sleep(2);
    }
}

void test_brightness(void)
{
    int level;
    
    printf("Enter brightness level (0-15): ");
    if (scanf("%d", &level) != 1) {
        fprintf(stderr, "Invalid input\n");
        return;
    }
    
    if (histogram_set_brightness(level) < 0) {
        fprintf(stderr, "Failed to set brightness\n");
    } else {
        printf("Brightness set to %d\n", level);
    }
}

void test_custom_pattern(void)
{
    int x, y;
    
    printf("Drawing smiley face...\n");
    histogram_clear();
    
    // Eyes
    matrix_set_pixel(8, 2, true);
    matrix_set_pixel(9, 2, true);
    matrix_set_pixel(22, 2, true);
    matrix_set_pixel(23, 2, true);
    
    // Mouth
    for (x = 10; x <= 21; x++) {
        if (x >= 10 && x <= 12) y = 5;
        else if (x >= 13 && x <= 18) y = 6;
        else y = 5;
        matrix_set_pixel(x, y, true);
    }
    
    matrix_update();
    printf("Custom pattern displayed!\n");
}

int main(void)
{
    uint32_t histogram[256];
    int choice;
    int running = 1;
    
    printf("Initializing MAX7219 Histogram Display...\n");
    
    if (histogram_init() < 0) {
        fprintf(stderr, "Failed to initialize histogram display\n");
        fprintf(stderr, "Make sure the driver is loaded: sudo insmod max7219_driver.ko\n");
        return 1;
    }
    
    printf("Display initialized successfully!\n");
    printf("Hardware: 4x MAX7219 8x8 LED Matrices (32x8 resolution)\n");
    
    while (running) {
        print_menu();
        
        if (scanf("%d", &choice) != 1) {
            fprintf(stderr, "Invalid input\n");
            while (getchar() != '\n');
            continue;
        }
        
        switch (choice) {
            case 1:
                printf("Displaying uniform histogram...\n");
                generate_sample_histogram(histogram, "uniform");
                histogram_display_grouped(histogram);
                break;
                
            case 2:
                printf("Displaying gaussian histogram...\n");
                generate_sample_histogram(histogram, "gaussian");
                histogram_display_grouped(histogram);
                break;
                
            case 3:
                printf("Displaying bimodal histogram...\n");
                generate_sample_histogram(histogram, "bimodal");
                histogram_display_grouped(histogram);
                break;
                
            case 4:
                printf("Displaying increasing histogram...\n");
                generate_sample_histogram(histogram, "increasing");
                histogram_display_grouped(histogram);
                break;
                
            case 5:
                printf("Displaying decreasing histogram...\n");
                generate_sample_histogram(histogram, "decreasing");
                histogram_display_grouped(histogram);
                break;
                
            case 6:
                printf("Displaying random histogram...\n");
                generate_sample_histogram(histogram, "random");
                histogram_display_grouped(histogram);
                break;
                
            case 7:
                test_windowed_display();
                break;
                
            case 8:
                printf("Displaying test pattern...\n");
                histogram_test_pattern();
                break;
                
            case 9:
                printf("Clearing display...\n");
                histogram_clear();
                break;
                
            case 10:
                test_brightness();
                break;
                
            case 11:
                test_custom_pattern();
                break;
                
            case 0:
                printf("Exiting...\n");
                running = 0;
                break;
                
            default:
                printf("Invalid choice\n");
                break;
        }
    }
    
    histogram_cleanup();
    printf("Cleanup complete. Goodbye!\n");
    
    return 0;
}
