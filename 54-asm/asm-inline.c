#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define __NR_write 1  

char *text = "Test text";

static int sys_write(int fd, char *buf, size_t len) {
    long rc;

    printf("\nExecute a system call =====\n");

    /* 
     * OUTPUT:  rax - system call return value;
     * INPUT:   rax - system call number;
     *          rdi - file descriptor
     *          rsi - buffer
     *          rdx - length
     */

    asm("syscall;"
        : "=a" (rc)
        : "a" (__NR_write), "D" ((long) fd), "S" (buf), "d" (len));

    return rc;
}

static void cpuid_brand() {
    int code;
    int32_t out[12];

    printf("\n\ncpuid instruction: multiple inputs =====\n");

    code = 0x80000002;
    asm("cpuid"             // stores result in EAX, EBX, ECX, EDX
        : "=a"(out[0]),     // store result result values in the output array
          "=b"(out[1]),
          "=c"(out[2]),
          "=d"(out[3])
        : "a"(code)
        );

    code = 0x80000003;
    asm("cpuid"
        : "=a"(out[4]),
          "=b"(out[5]),
          "=c"(out[6]),
          "=d"(out[7])
        : "a"(code)
        );

    code = 0x80000004;
    asm("cpuid"
        : "=a"(out[8]),
          "=b"(out[9]),
          "=c"(out[10]),
          "=d"(out[11])
        : "a"(code)
        );

    printf("CPU brand: %.48s\n", (char *) out);
}

static void string_copy() {
    char dest[32];

    printf("\ncopy string (rdi, rsi, no output) =====\n");

    int l = strlen(text);

    asm("cld\n\t"
        "rep movsb\n\t"
        "xor %%al, %%al\n\t"
        "stosb"         /* null terminate */
        :               /* no output */
        : "D" (dest), "S" (text), "c" (l)
        : "cc" /* modified flags */
        );

    printf("copied string: '%s'\n", dest);
}

static void rdtscl_long() {
    uint64_t rdx, rax;
    uint64_t val64;

    printf("\nrdtsc (multiple outputs: edx:eax combined) =====\n");

    asm("rdtsc" : "=a" (rax), "=d" (rdx));

    val64 = (rdx << 32) | rax;

    printf("tsc = %lu\n", val64);
}

static void atomic_dec(uint64_t counter) {
    printf("\nAtomic decrement: must use a memory reference (with a lock prefix) =====\n");

    printf("value before decrement: %lu\n", counter);

    asm __volatile__ ("lock; decq %0"
        : "=m" (counter)
        : "m" (counter)
        : "cc"     /* may modify flags */
        );

    printf("value after decrement: %lu\n", counter);
}

int main(int argc, char *argv[]) {
    int rc;

    printf("Inline assembly samples\n");

    rc = sys_write(STDOUT_FILENO, text, strlen(text));
    cpuid_brand();
    string_copy();
    rdtscl_long();
    atomic_dec(12);

    return EXIT_SUCCESS;
}
