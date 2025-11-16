#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int f = open("/proc/my_gpio_driver", O_RDWR);

    while (1)
    {
        write(f, "21 1", 4);
        sleep(1);
        write(f, "21 0", 4);
        sleep(1);
    }
}
