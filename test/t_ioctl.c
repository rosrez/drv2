#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "macros.h"
#include "ioctl.h"

#define DEV_NAME "/dev/ioctl"

void print_usage(char *progname)
{
    fprintf(stderr, "Usage %s [options]\n", progname);
    fprintf(stderr, "   -c[lear]\n");
    fprintf(stderr, "   -a[dd] text\n");
    fprintf(stderr, "   -g[et]\n");
    fprintf(stderr, "   -d[elete]\n");
    fprintf(stderr, "   -r[ewind]\n");
    fprintf(stderr, "   -n[ext]\n");
    fprintf(stderr, "   -q[uery next]\n");

    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    void *argptr;
    char resbuf[TXT_LEN];
    int opt, cmd, fd, has_next;

    /* allow only one option */
    
    cmd = 0;
    argptr = NULL;
    if ((opt = getopt(argc, argv, "ca:gdrnq")) != -1) {
        switch (opt) {
            case 'c':
                cmd = IOCTL_CLEAR;
                break;

            case 'a':
                cmd = IOCTL_ADD;
                argptr = optarg;
                break;

            case 'g':
                cmd = IOCTL_GET;
                argptr = &resbuf[0];
                break;

            case 'd':
                cmd = IOCTL_REMOVE;
                break;

            case 'r':
                cmd = IOCTL_REWIND;
                break;

            case 'n':
                cmd = IOCTL_NEXT;
                break;

            case 'q':
                cmd = IOCTL_HASNEXT;
                argptr = &has_next;
                break;

            default:
                fprintf(stderr, "Invalid option %c\n", opt);
                print_usage(argv[0]);
        }
    }

    if (cmd == 0)
        print_usage(argv[0]);       /* does not return */

    if ((fd = open(DEV_NAME, O_RDWR)) == -1)
        serr_exit("open() failed");

    if (ioctl(fd, cmd, (unsigned long *) argptr) == -1)
        serr_exit("ioclt() failed");

    switch (cmd) {
        case IOCTL_GET:
            printf("%s", resbuf);
            break;

        case IOCTL_HASNEXT:
            exit(has_next ? 0 : 2);
            break;

        default:
            break;
    }

    close(fd);
    return EXIT_SUCCESS;
}
