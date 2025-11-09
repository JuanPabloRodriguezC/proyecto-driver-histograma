#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int f = open("/proc/gpio_driver", O_RDWR);

    while (1)
    {
        write(f, "17 1", 4);
        sleep(5);
        write(f, "17 0", 4);
        sleep(5);
    }
}