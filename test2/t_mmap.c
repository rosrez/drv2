#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <libgen.h>     /* basename() */

#include "macros.h"

void print_usage(char *progname) {
    printf("%s [nodename] [npages]", basename(progname));
    printf("\tMemory maps the 'nodename' device/module -- the mapping is 'npages' pages in size\n."
            "\tTries to re");
}

void compare(char *buf1, char *buf2, size_t size, char *testname) {
    printf("Comparing %s", testname);
    if ((memcmp(buf1, buf2, size)) == 0)  {
        printf(" success\n");
    } else {
        printf(" failure\n");
    }
}

int main(int argc, char **argv) {
    int fd, size, rc, i;
    char *area, *nodename = "/dev/mmap";
    char *readbuf;

    if (argc == 1)
        print_usage(argv[0]);

    nodename = argv[1];

    size = getpagesize();       /*  use one page by default */
    if (argc > 2)
        size = atoi(argv[2]);

    printf("Memory mapping node: %s of size %d bytes\n", nodename, size);

    if ((fd = open(nodename, O_RDWR)) < 0)
        serr_exit("can't open node %s", nodename);

    area = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (area == MAP_FAILED)
        serr_exit("error mapping");

    if ((readbuf = malloc(size)) == NULL)
        serr_exit("error allocating read buffer");

    /*  read the file/device */
    if ((read(fd, readbuf, size)) != size)
        serr_exit("read (1) failed");

    /*  compare read and mapped contents */
    compare(readbuf, area, size, "mmap/read (1)");

    /*  overwrite mapped area with ABCDEF... */
    for (i = 0; i < size; i++) {
        area[i] = (char) (i % 26 + 65);
    }
    
    /*  rewind the device file */
    lseek(fd, 0, SEEK_SET);

    /*  read again */
    if ((read(fd, readbuf, size)) != size)
        serr_exit("read (2) failed");

    /*  compare read and mapped contents */
    compare(readbuf, area, size, "mmap/read (2)");

    /*  can close the file now */
    close(fd);
    free(readbuf);

    /*  just cat out the file to see if it worked */
    rc = write(STDOUT_FILENO, area, size);
    if (rc != size)
        serr_exit("write() to stdout failed");

    write(STDOUT_FILENO, "\n", 1);
    return 0;
}
