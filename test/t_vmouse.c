#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define FILENAME "/sys/devices/platform/vmouse/coordinates"

#define err_return(rc, arg...) do { fprintf(stderr, arg); perror(" -- reason"); return rc; } while (0)

int main(int argc, char *argv[])
{
    int fd;
    int x, y;
    char buffer[10];

    /* Open the sysfs coordinnate node */
    fd = open(FILENAME, O_RDWR);
    if (fd == -1)
        err_return(EXIT_FAILURE, "open() failed"); 

    while (1) {
        /* Genenrate random relative coordinates */
        x = random() % 20;
        y = random() % 20;
        if (x % 2) x = -x; if (y % 2) y = -y;
        sprintf(buffer, "%d %d %d", x, y, 0);
        write(fd, buffer, strlen(buffer));
        fsync(fd);
        sleep(1);
    }

    close(fd);

    return EXIT_SUCCESS; 
}
