.intel_syntax noprefix

#include <asm/unistd.h>
/*
The included file above includes either unistd_64.h or unistd_32.h, which, in turn,
contain NR_ defines for system call numbers. We cannot include 64 or 32 bit versions
directly, since this doesn't seem to be portable (even across Fedora and RedHat).

On Fedora, we can find the definitions for 64-bit calls in:
/usr/include/asm/unistd_64.h,

and include the file as:
#include <asm/unistd_64.h>
*/

    .section .data

greeting: .ascii "Hello from syscall\n"
greeting_end:
    .set GREETING_LEN, greeting_end - greeting

    .section .text

    .global _start
    .type   _start, @function

_start:
    mov     eax, __NR_write         # write(STDOUT_FILE, greeting, GREETING_LEN);
    mov     ebx, 1                  # stdout
    lea     ecx, [greeting]         # greeting
    mov     edx, GREETING_LEN       # number of bytes to write
    int     0x80

    mov     eax, __NR_exit          # exit(0);
    xor     ebx, ebx
    int     0x80
