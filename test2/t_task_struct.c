#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <alloca.h>

#define err_exit(message)   do { \
                                perror(message); \
                                exit(EXIT_FAILURE); \
                            } while(0)
#define DEFAULT_DEVNAME "/dev/task"

#define STACK_CHUNK (1024 - 16)
#define HEAP_CHUNK (10 * 1024)

char *mem;
char buf[1024];
int dev;

#define rsp(var) asm("movq %%rsp, %0" : "=r" (var))

void grab_stack(int iterations) {
    if (iterations == 0) {
        register long __rsp;
        rsp(__rsp);
        printf("Increased stack bottom is: %lx\n", __rsp);

        if (read(dev, buf, 1) == -1)
            err_exit("grab_stack(): read() failed");
        return;
    }

    mem = alloca(STACK_CHUNK);
    memset(mem, 33, STACK_CHUNK);
    grab_stack(iterations - 1);
}

void grab_heap(size_t size) {
    char *mem[1000];
    int i;
    
    printf("brk() before heap increase equals: %p\n", sbrk(0));

    /*  this will probably result in an call sbrk() at most */
    for (i = 0; i < sizeof(mem)/sizeof(mem[0]); i++) {
        mem[i] = malloc(size);
        /*  so let's actually access the memory */
        memset(mem[i], 175, size);
    }
    
    printf("brk() after heap increase equals: %p\n", sbrk(0));

    if (read(dev, buf, 1) == -1)
        err_exit("grab_memory(): read() failed");

    for (i = 0; i < sizeof(mem)/sizeof(mem[0]); i++) {
        free(mem[i]);
    }
}

void message(char *msg) {
    printf("%s. Press ENTER\n", msg);
    fgetc(stdin);
}

int main(int argc, char *argv[]) {
    char *devname = DEFAULT_DEVNAME;

    if (argc > 1) 
        devname = argv[1];

    printf("My pid is: %d\n", (int) getpid());
    
    dev = open(devname, O_RDONLY);
    if (dev == -1)
        err_exit("open() failed");

    /* (1) initial state */
    if (read(dev, buf, 1) == -1)
        err_exit("main(): read() failed");

    message("captured initial state\n"); 

    /* (2) now grab 'a lot' of stack  */
    long __rsp;
    rsp(__rsp);
    printf("Initial stack bottom is: %lx\n", __rsp);

    grab_stack(1000);
    
    message("captured state after stack increase\n");

    /* (3) */
    grab_heap(HEAP_CHUNK);

    message("captured state after heap increase\n");

    close(dev);

    printf("Done.\n");
    return EXIT_SUCCESS;
}
