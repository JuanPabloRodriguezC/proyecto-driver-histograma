#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>

#define MAX_USER_SIZE 4096
#define BCM2837_GPIO_ADDRESS 0x3F200000

// SPI Pins
#define SPI_MOSI  10
#define SPI_CLK   11
#define SPI_CS    8

// MAX7219 Registers
#define MAX7219_REG_NOOP        0x00
#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_DIGIT1      0x02
#define MAX7219_REG_DIGIT2      0x03
#define MAX7219_REG_DIGIT3      0x04
#define MAX7219_REG_DIGIT4      0x05
#define MAX7219_REG_DIGIT5      0x06
#define MAX7219_REG_DIGIT6      0x07
#define MAX7219_REG_DIGIT7      0x08
#define MAX7219_REG_DECODEMODE  0x09
#define MAX7219_REG_INTENSITY   0x0A
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DISPLAYTEST 0x0F

#define NUM_MATRICES 4

static struct proc_dir_entry *proc_entry = NULL;
static char data_buffer[MAX_USER_SIZE];
static unsigned int *gpio_registers = NULL;

// Framebuffer for 4 matrices (32x8)
static uint8_t framebuffer[NUM_MATRICES][8];

static inline void gpio_set_output(unsigned int pin)
{
    unsigned int reg = pin / 10;
    unsigned int shift = (pin % 10) * 3;
    unsigned int *fsel = gpio_registers + reg;
    
    *fsel &= ~(7 << shift);  // Clear
    *fsel |= (1 << shift);   // Set as output (001)
}

static inline void gpio_set_high(unsigned int pin)
{
    unsigned int *set_reg = (unsigned int*)((char*)gpio_registers + 0x1C);
    *set_reg = (1 << pin);
}

static inline void gpio_set_low(unsigned int pin)
{
    unsigned int *clr_reg = (unsigned int*)((char*)gpio_registers + 0x28);
    *clr_reg = (1 << pin);
}


static void spi_init(void)
{
    gpio_set_output(SPI_MOSI);
    gpio_set_output(SPI_CLK);
    gpio_set_output(SPI_CS);
    
    gpio_set_low(SPI_MOSI);
    gpio_set_low(SPI_CLK);
    gpio_set_high(SPI_CS);
}

static void spi_transfer_byte(uint8_t data)
{
    int i;
    for (i = 7; i >= 0; i--) {
        gpio_set_low(SPI_CLK);
        
        if (data & (1 << i))
            gpio_set_high(SPI_MOSI);
        else
            gpio_set_low(SPI_MOSI);
        
        udelay(5);  // Increased delay for stability
        gpio_set_high(SPI_CLK);
        udelay(5);  // Increased delay for stability
    }
    gpio_set_low(SPI_CLK);
}

static void max7219_send(uint8_t reg, uint8_t data, int matrix_index)
{
    int i;
    
    gpio_set_low(SPI_CS);
    udelay(5);  // Increased CS setup time
    
    // Send to all matrices in cascade
    for (i = NUM_MATRICES - 1; i >= 0; i--) {
        if (i == matrix_index) {
            spi_transfer_byte(reg);
            spi_transfer_byte(data);
        } else {
            // Send NOOP to other matrices
            spi_transfer_byte(MAX7219_REG_NOOP);
            spi_transfer_byte(0x00);
        }
    }
    
    udelay(5);  // Increased CS hold time
    gpio_set_high(SPI_CS);
    udelay(5);  // Time for latch
}

static void max7219_broadcast(uint8_t reg, uint8_t data)
{
    int i;
    
    gpio_set_low(SPI_CS);
    udelay(5);
    
    // Send same command to all matrices
    for (i = 0; i < NUM_MATRICES; i++) {
        spi_transfer_byte(reg);
        spi_transfer_byte(data);
    }
    
    udelay(5);
    gpio_set_high(SPI_CS);
    udelay(5);
}

static void max7219_init(void)
{
    spi_init();
    
    // Initialize all matrices
    max7219_broadcast(MAX7219_REG_SHUTDOWN, 0x00);      // Shutdown mode
    max7219_broadcast(MAX7219_REG_DISPLAYTEST, 0x00);   // Normal operation
    max7219_broadcast(MAX7219_REG_DECODEMODE, 0x00);    // No decode (raw mode)
    max7219_broadcast(MAX7219_REG_SCANLIMIT, 0x07);     // Scan all 8 digits
    max7219_broadcast(MAX7219_REG_INTENSITY, 0x08);     // Medium brightness
    max7219_broadcast(MAX7219_REG_SHUTDOWN, 0x01);      // Normal operation
    
    // Clear all matrices
    memset(framebuffer, 0, sizeof(framebuffer));
}

static void max7219_clear(void)
{
    int row;
    
    memset(framebuffer, 0, sizeof(framebuffer));
    
    for (row = 0; row < 8; row++) {
        max7219_broadcast(MAX7219_REG_DIGIT0 + row, 0x00);
    }
}

static void max7219_update(void)
{
    int matrix, row;
    
    for (row = 0; row < 8; row++) {
        for (matrix = 0; matrix < NUM_MATRICES; matrix++) {
            max7219_send(MAX7219_REG_DIGIT0 + row, 
                        framebuffer[matrix][row], 
                        matrix);
        }
    }
}

static void max7219_set_pixel(int x, int y, bool on)
{
    int matrix, local_x, byte_index, bit_index;
    
    if (x < 0 || x >= 32 || y < 0 || y >= 8)
        return;
    
    // Determine which matrix (0-3) and local position
    matrix = x / 8;
    local_x = x % 8;
    
    byte_index = y;
    bit_index = 7 - local_x;  // MSB is leftmost pixel
    
    if (on)
        framebuffer[matrix][byte_index] |= (1 << bit_index);
    else
        framebuffer[matrix][byte_index] &= ~(1 << bit_index);
}

// Histogram display function
static void display_histogram_grouped(uint32_t *histogram)
{
    int i, col, height;
    uint32_t max_value = 0;
    uint32_t grouped[32];
    
    // Group 256 intensities into 32 columns
    memset(grouped, 0, sizeof(grouped));
    for (i = 0; i < 256; i++) {
        grouped[i / 8] += histogram[i];
    }
    
    // Find maximum for scaling
    for (i = 0; i < 32; i++) {
        if (grouped[i] > max_value)
            max_value = grouped[i];
    }
    
    // Clear display
    max7219_clear();
    
    if (max_value == 0)
        return;
    
    // Draw histogram
    for (col = 0; col < 32; col++) {
        height = (grouped[col] * 8) / max_value;
        if (height > 8) height = 8;
        
        for (i = 0; i < height; i++) {
            max7219_set_pixel(col, 7 - i, true);
        }
    }
    
    max7219_update();
}

// Proc file operations
static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    char msg[128];
    int len;
    
    if (*offset > 0)
        return 0;
    
    len = snprintf(msg, sizeof(msg), 
                   "MAX7219 Driver Ready\nMatrices: %d\nResolution: 32x8\n",
                   NUM_MATRICES);
    
    if (copy_to_user(buf, msg, len))
        return -EFAULT;
    
    *offset = len;
    return len;
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    char cmd[32];
    uint32_t histogram[256];
    int i;
    
    memset(data_buffer, 0, sizeof(data_buffer));
    
    if (size > MAX_USER_SIZE)
        size = MAX_USER_SIZE;
    
    if (copy_from_user(data_buffer, buf, size))
        return -EFAULT;
    
    // Parse command
    sscanf(data_buffer, "%31s", cmd);
    
    if (strcmp(cmd, "clear") == 0) {
        max7219_clear();
        printk(KERN_INFO "MAX7219: Display cleared\n");
    }
    else if (strcmp(cmd, "test") == 0) {
        // Test pattern
        for (i = 0; i < 32; i++) {
            max7219_set_pixel(i, i % 8, true);
        }
        max7219_update();
        printk(KERN_INFO "MAX7219: Test pattern displayed\n");
    }
    else if (strcmp(cmd, "histogram") == 0) {
        // Expect histogram data after command
        char *data_ptr = data_buffer + strlen(cmd) + 1;
        size_t expected_size = strlen(cmd) + 1 + sizeof(histogram);
        
        printk(KERN_DEBUG "MAX7219: Received %zu bytes, expected %zu bytes\n", 
               size, expected_size);
        
        if (size >= expected_size) {
            memcpy(histogram, data_ptr, sizeof(histogram));
            display_histogram_grouped(histogram);
            printk(KERN_INFO "MAX7219: Histogram displayed\n");
        } else {
            printk(KERN_WARNING "MAX7219: Invalid histogram data size (got %zu, need %zu)\n",
                   size, expected_size);
        }
    }
    else if (strcmp(cmd, "pixel") == 0) {
        int x, y, state;
        if (sscanf(data_buffer, "pixel %d %d %d", &x, &y, &state) == 3) {
            max7219_set_pixel(x, y, state != 0);
            max7219_update();
            printk(KERN_INFO "MAX7219: Pixel (%d,%d) = %d\n", x, y, state);
        }
    }
    else if (strcmp(cmd, "intensity") == 0) {
        int level;
        if (sscanf(data_buffer, "intensity %d", &level) == 1) {
            if (level >= 0 && level <= 15) {
                max7219_broadcast(MAX7219_REG_INTENSITY, level);
                printk(KERN_INFO "MAX7219: Intensity set to %d\n", level);
            }
        }
    }
    
    return size;
}

static const struct proc_ops fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init max7219_driver_init(void)
{
    // Map GPIO memory
    gpio_registers = (unsigned int*)ioremap(BCM2837_GPIO_ADDRESS, PAGE_SIZE);
    
    if (gpio_registers == NULL) {
        printk(KERN_ALERT "MAX7219: Failed to map GPIO memory\n");
        return -ENOMEM;
    }
    
    printk(KERN_INFO "MAX7219: Successfully mapped in GPIO memory\n");
    
    // Initialize hardware
    max7219_init();
    
    // Create proc entry
    proc_entry = proc_create("max7219", 0666, NULL, &fops);
    if (proc_entry == NULL) {
        printk(KERN_ALERT "MAX7219: Failed to create /proc/max7219\n");
        iounmap(gpio_registers);
        gpio_registers = NULL;
        return -ENOMEM;
    }
    
    printk(KERN_INFO "MAX7219: Driver loaded successfully\n");
    printk(KERN_INFO "MAX7219: SPI Pins - MOSI:%d CLK:%d CS:%d\n", 
           SPI_MOSI, SPI_CLK, SPI_CS);
    
    return 0;
}

static void __exit max7219_driver_exit(void)
{
    if (gpio_registers != NULL) {
        max7219_clear();
        max7219_broadcast(MAX7219_REG_SHUTDOWN, 0x00);
    }
    
    if (proc_entry != NULL) {
        proc_remove(proc_entry);
        proc_entry = NULL;
    }
    
    if (gpio_registers != NULL) {
        iounmap(gpio_registers);
        gpio_registers = NULL;
    }
    
    printk(KERN_INFO "MAX7219: Driver unloaded successfully\n");
}

module_init(max7219_driver_init);
module_exit(max7219_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Operativos - TEC");
MODULE_DESCRIPTION("MAX7219 LED Matrix Driver for Histogram Display");
MODULE_VERSION("1.0");