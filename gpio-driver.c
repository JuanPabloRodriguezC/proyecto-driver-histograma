#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linix/proc_fs.h>
#include <linux/slab.h>
#include <asm/io.h>

#define MAX_USER_SIZE 1024
#define BCM2837_GPIO_ADDRESS 0x3F200000

static struct proc_dir_entry *proc = NULL;

static char data_buffer[MAX_USER_SIZE];

static unsigned int *gpio_registers = NULL;

static void gpio_pin_on(unsigned int pin)
{
    unsigned int reg = pin / 10;
    unsigned int shift = (pin % 10) * 3;
    unsigned int* gpio_fsel = gpio_registers + reg;
    unsigned int* gpio_on_register = (unsigned int*)((char*)gpio_registers + 0x1C);

    *gpio_fsel &= ~(7 << shift);
    *gpio_fsel |= (1 << shift);
    *gpio_on_register |= (1 << pin);

    return;
}

static void gpio_pin_off(unsigned int pin)
{
    unsigned int* gpio_off_register = (unsigned int*)((char*)gpio_registers + 0x28);
    gpio_off_register |= (1 << pin);
    return;
}

ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    copy_to_user(buf, "Hello\n", 7);
    printk("Wrote 7 bytes to /proc/gpio_driver\n");
    return 7;    
}

ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    unsigned int pin = UINT_MAX;
    unsigned int value = UINT_MAX;

    memset(data_buffer, 0x0, sizeof(data_buffer));

    if (size > MAX_USER_SIZE)
    {
        size = MAX_USER_SIZE;
    }

    if (copy_from_user(data_buffer, buf, size))
        return 0;

    if (sscanf(data_buffer, "%u %u", &pin, &value) != 2)
    {
        printk("Invalid input format. Use: <pin> <value>\n");
        return size;
    }

    if (pin > 21 || pin <-0){
        printk("Invalid pin number. Use a pin between 0 and 21.\n");
        return size;
    }

    if (value != 0 && value != 1)
    {
        printk("Invalid value. Use 0 for LOW and 1 for HIGH.\n");
        return size;
    }
    printk("You wrote: pin=%u value=%u\n", pin, value);

    if (value == 1)
    {
        gpio_pin_on(pin);
        printk("Set pin %u HIGH\n", pin);
    }
    else
    {
        gpio_pin_off(pin);
        printk("Set pin %u LOW\n", pin);
    }

    return size;
}

static const struct proc_ops fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init gpio_driver_init(void)
{

    gpio_registers = (int*)ioremap(BCM2837_GPIO_ADDRESS, PAGE_SIZE);

    if (gpio_registers == NULL)
    {
        printk(KERN_ALERT "Error: Could not map GPIO registers\n");
        return -1;
    }


    printk("Successfully mapped in GPIO memory\n");
    proc = proc_create("my_gpio_driver", 0666, NULL, &fops);
    if (proc == NULL) {
        printk(KERN_ALERT "Error: Could not initialize /proc/gpio_driver\n");
        return -1;
    }

    return 0;

}

static void __exit gpio_driver_exit(void)
{
    proc_remove(proc);
    printk("GPIO Driver Exited\n");
    return
}

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Operativos");
MODULE_DESCRIPTION("A simple GPIO driver example");;
MODULE_VERSION("1.0");;

