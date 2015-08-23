#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define MAX_DEVS 10
#define NUM_DEVS 4
#define DEVNODE_FMT "/dev/polld%d"

int main(int argc, char *argv[]) {
    int i, read_flg, rc;
    int fd[MAX_DEVS];
    char tmps[128];
    char readbuf[1024];

    char prog[] = {'|', '/', '-', '\\'};
    int p;    

    struct timeval tv;
    fd_set readfds;
    int max_fds;

    int num_devs = NUM_DEVS;
    char *devnode_fmt = DEVNODE_FMT;

    if (argc > 1)
        num_devs = atoi(argv[1]);
    if (argc > 2)
        devnode_fmt = argv[2];

    for (i = 0; i < num_devs; i++) {
        sprintf(tmps, devnode_fmt, i);
        fd[i] = open(tmps, O_RDONLY);
        if (fd[i] < 0) {
            fprintf(stderr, "File %s ", tmps);
            perror("Cannot open: ");
            return 1;
        }
    }
    printf("Opened all devices\n");

    p = 0;
    while (1) {
        printf("\r%c\t", prog[p]); 
        p = (p + 1) % 4;

        /* polling interval: 1/8 of a second */
        tv.tv_sec = 0;
        tv.tv_usec = 125000;

        max_fds = -1;
        FD_ZERO(&readfds);
        for (i = 0; i < num_devs; i++) {
            FD_SET(fd[i], &readfds);
            if (fd[i] > max_fds)
                max_fds = fd[i];
        }

        rc = select(max_fds + 1, &readfds, NULL, NULL, &tv);
        if (rc == -1) {
            perror("select() failed: ");
            return 2;
        }

        read_flg = 0;
        for (i = 0; i < num_devs; i++) {
            if (FD_ISSET(fd[i], &readfds)) {
                rc = read(fd[i], readbuf, 1023);
                if (rc == -1) {
                    perror("cannot read file %d: ");
                    return 3;
                }
                readbuf[rc] = '\0';
                printf("%15.15s\t", readbuf);
                read_flg++;
            } else {
                printf("\t\t");
            }
        }
        if (read_flg)
            printf("\n");
        fflush(stdout);
    }

    for (i = 0; i < num_devs; i++) {
        close(fd[i]);
    }

    return 0;
}
