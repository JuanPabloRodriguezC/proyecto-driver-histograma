#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linix/proc_fs.h>
#include <linux/slab.h>

#define MAX_USER_SIZE 1024

static struct proc_dir_entry *proc = NULL;

static char data_buffer[MAX_USER_SIZE];

ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    copy_to_user(buf, "Hello\n", 7);
    printk("Wrote 7 bytes to /proc/gpio_driver\n");
    return 7;    
}

ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    memset(data_buffer, 0x0, sizeof(data_buffer));

    if (size > MAX_USER_SIZE)
    {
        size = MAX_USER_SIZE;
    }

    copy_from_user(data_buffer, buf, size);

    printk("Read %zu bytes from /proc/gpio_driver\n", size);
    return size;
}

static const struct proc_ops fops = {
    .proc_read = NULL,
    .proc_write = NULL,
};

static int __init gpio_driver_init(void)
{
    printk("GPIO Driver Initialized\n");
    proc = proc_create("gpio_driver", 0666, NULL, &fops);
    if (proc == NULL) {
        printk(KERN_ALERT "Error: Could not initialize /proc/gpio_driver\n");
        return -ENOMEM;
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
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple GPIO driver example");;
MODULE_VERSION("1.0");;

