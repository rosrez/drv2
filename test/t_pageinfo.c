#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>       /* open() and friends */
#include <fcntl.h>
#include <sys/mman.h>       /* mmap() */

#include "macros.h"

#include "pageinfo.h"
#include "pageinfo_ioctl.h"

#define DEVNAME "/dev/pageinfo"

#define PFLAG_PRESENT 0
#define PFLAG_GLOBAL 1
#define PFLAG_DIRTY 2
#define PFLAG_ACCESSED 3
#define PFLAG_WRITE 4
#define PFLAG_EXEC 5
#define PFLAG_HUGE 6
#define PFLAG_MAX 7

int data;
char *page_size[] = { "N/A ", "1 GB", "2 MB", "4 KB" };

unsigned long read_cr3() {
    unsigned long res;
    asm ("mov %%cr3, %0\n\t"
        : "=r" (res)
        :
        :);

    return res;
}

static char *pflag_val(int flag, int value) {

    static char *page_flags[PFLAG_MAX][2] = {
        {"pres ", "PRES "},
        {"glob ", "GLOB "},
        {"dirt ", "DIRT "},
        {"acc  ", "ACC  "},
        {"writ ", "WRIT "},
        {"exec ", "EXEC "},
        {"huge ", "HUGE "}
    };

    return page_flags[flag][value];
}

void parse_va(struct pageinfo *info)
{
    unsigned long va =  va;
    unsigned long flags = info->flags[info->level];

    printf("virtual address: %lx -- size: %s", info->va, page_size[info->level]);
    if (info->none) {
        printf(":: NO MAPPING\n");
        return;
    }

    printf("   ");

    printf(pflag_val(PFLAG_PRESENT, !!flags));

    /* zero flags serve as an indicator that a page is not present since
     * if page is not present, the remaining flags have no meaning. 
     * (note that we perform an explicit check for the NONE condition
     * (no mapping) above).
     */
    if (!flags) {
        return;
    }

    printf(pflag_val(PFLAG_GLOBAL, !!(flags & _PAGE_GLOBAL)));
    printf(pflag_val(PFLAG_DIRTY, !!(flags & _PAGE_DIRTY)));
    printf(pflag_val(PFLAG_ACCESSED, !!(flags & _PAGE_ACCESSED)));
    printf(pflag_val(PFLAG_WRITE, !!(flags & _PAGE_RW)));
    printf(pflag_val(PFLAG_EXEC, !(flags & _PAGE_NX)));     /* NOTE: _PAGE_NX stands for NON-executable! */
    printf(pflag_val(PFLAG_HUGE, !!(flags & _PAGE_PSE)));

    printf(":: flags: 0x%lx :: ", flags);

    printf("physical page address: %lx\n", info->pa_dir[info->level]); 
}

void print_faults(struct pageinfo *info)
{
    printf("<<<<< FAULTS START >>>>>\n");

    printf("FAULTS: prev_minor: %d\n", info->prev_minflt);
    printf("FAULTS: prev_major: %d\n", info->prev_majflt);
    printf("FAULTS: cur_minor:  %d\n", info->cur_minflt);
    printf("FAULTS: cur_major: %d\n", info->cur_majflt);

    printf("<<<< FAULTS END >>>>>\n");
}

void test_codeseg(int fd)
{
    struct pageinfo info;

    /* ------------- CODE -------------- */

    info.va = (unsigned long ) &parse_va;

    if (ioctl(fd, PINFO_GET_ADDR, &info) == -1)
        serr_exit("ioctl() failed -- data");

    printf("======== CODE SEGMENT ===========\n");
    parse_va(&info);
}

void test_dataseg(int fd)
{
    struct pageinfo info;

    info.va = (unsigned long ) &data;

    if (ioctl(fd, PINFO_GET_ADDR, &info) == -1)
        serr_exit("ioctl() failed -- data");

    printf("====== DATA SEGMENT =========\n");
    parse_va(&info);
    print_faults(&info);
}

void touch_mem(int fd, char *addr)
{
    struct pageinfo info;
    
    /* ------------- STACK -------------- */

    info.va = (unsigned long ) addr;

    if (ioctl(fd, PINFO_GET_ADDR, &info) == -1)
        serr_exit("ioctl() failed -- data");

    printf("------ INITIAL STATE ----------\n");
    parse_va(&info);
    print_faults(&info);

    /* ------------- STACK 2 -------------- */

    /* now do a write to stack and see the changes */
    __asm__ ("movb $0x41, %%al\n\t"
             "movb %%al, (%0)\n\t"
            : 
            : "r" (addr)
            : "%eax"); 

    printf("+++++ Written %c to %p ++++++\n", *addr, addr);

    if (ioctl(fd, PINFO_GET_ADDR, &info) == -1)
        serr_exit("ioctl() failed -- data");

    printf("------ ON WRITE ACCESS ----------\n");
    parse_va(&info);
    print_faults(&info);
}

void test_stackseg(int fd, int offset)
{
    int asize;
    char *stack;

    asize = getpagesize() * 2;
    stack = alloca(asize);

    printf("======== STACK SEGMENT ===========\n");
    // touch_mem(fd, &stack[offset]);
    touch_mem(fd, (char *) &asize);
}

void test_mmap(int fd, int offset)
{
    int mfd, size;
    char *mem;

    size = getpagesize() * 2;
    printf("size = %d\n", size);
    mfd = open("t_pageinfo.mmap", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (mfd == -1)
        serr_exit("open() failed");

    /* truncate the file to requested size, we will map the entire file */
    printf("mfd = %d\n", mfd);
    if (ftruncate(mfd, (off_t) size) == -1)
        serr_exit("truncate() failed");

    /* map the entire file */
    mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
    if (mem == MAP_FAILED)
        serr_exit("mmap() failed");

    close(mfd);

    printf("======== MMAP ===========\n");

    touch_mem(fd, &mem[offset]);
}

void print_regs()
{
    unsigned long cr3;

    cr3 = read_cr3();

    printf("========= CPU REGISTERS =========\n");
    printf("CR3: %lx\n", cr3);
}

int main(int argc, char *argv[])
{
    int fd;
    int soffs,opt;

    if ((fd = open(DEVNAME, O_RDWR)) == -1)
        serr_exit("open() failed");

    while ((opt = getopt(argc, argv, "cds:m:r")) != -1) {
        switch (opt) {
            case 'd':
                test_dataseg(fd);
                break;
            
            case 'c':
                test_codeseg(fd);
                break;

            case 's':
                soffs = atoi(optarg);
                test_stackseg(fd, soffs);
                break;

            case 'm':
                soffs = atoi(optarg);
                test_mmap(fd, soffs);
                break;

            case 'r':
                print_regs();
                break;
        }
    }


    close(fd);

    return EXIT_SUCCESS;
}
