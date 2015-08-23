.intel_syntax noprefix

#include <asm/unistd.h>

#ifndef __NR_exit
#define __NR_read 0
#define __NR_write 1
#define __NR_open 3
#define __NR_close 4
#define __NR_exit 60
#endif

.equ STDIN_FILENO, 0
.equ STDOUT_FILENO, 1

# system call invocation macros -------------------------

.macro syscall_exit return_val
    mov     edi, \return_val
    mov     eax, __NR_exit
    syscall
.endm

.macro syscall_write fd, buffer, nbytes
    mov     rdi, \fd
    lea     rsi, [\buffer]
    mov     edx, \nbytes
    mov     eax, __NR_write
    syscall
.endm

.macro syscall_read fd, buffer, nbytes
    mov     rdi, \fd
    lea     rsi, [\buffer]
    mov     edx, \nbytes
    mov     eax, __NR_read
    syscall
.endm

.macro syscall_open filename, flags, mode
    mov     rdi, \filename
    mov     rsi, \flags
    mov     rdx, \mode
    mov     eax, __NR_open
    syscall
.endm

.macro syscall_close fd
    mov     rdi, \fd
    mov     eax, __NR_close
    syscall
.endm

.macro strlen str
    mov     rdi, \str
    mov     rcx, -1
    xor     al, al
    cld
    repne   scasb
    mov     rax, -2
    sub     rax, rcx
.endm

    .section .data

# DATA SEGMENT  -------------------------

err_usage: .ascii "Usage: syscall_cat [filename]\n"
err_usage_end:
    .set ERR_USAGE_LEN, err_usage_end - err_usage

err_cannot_open_file: .ascii "Cannot open file: "
err_cannot_open_file_end:
    .set ERR_CANNOT_OPEN_FILE_LEN, err_cannot_open_file_end - err_cannot_open_file

err_read_file: .ascii "Error reading input file\n"
err_read_file_end:
    .set ERR_READ_FILE_LEN, err_read_file_end - err_read_file

err_write_file: .ascii "Error reading to file: "
err_write_file_end:
    .set ERR_WRITE_FILE_LEN, err_write_file_end - err_write_file

chr_newline: .ascii "\n"

# BSS SEGMENT -------------------------------

    .section .bss

.equ    BUF_SIZE,   4096
buffer: .space BUF_SIZE

# CODE SEGMENT (ENTRY POINT) ----------------

    .section .text

    .global _start
    .type   _start, @function

_start:

# stack offsets of local variables
.equ    .fdin, 0

    mov     rbp, rsp
    sub     rsp, 16

    # parse command-line arguments
    mov     eax, STDIN_FILENO        # default to stdin for input
    mov     rsi, [rbp + 16]          # make sure argv[1] is not null
    test    rsi, rsi
    jz     store_fd

    ## if user supplied a filename, open the file
    syscall_open [rbp + 16], 0, 0400       # user-read (S_IRUSR) permission
    cmp     rax, 0
    jge     store_fd

    syscall_write 2, err_cannot_open_file, ERR_CANNOT_OPEN_FILE_LEN
    mov     rsi, [rbp + 16]
    strlen  rsi                         # eax == strlen(filename)
    syscall_write 2, rsi, eax
    syscall_write 2, chr_newline, 1
    syscall_exit 2

store_fd:
    # store input file descriptor on stack
    mov     [rsp + .fdin], rax

copy_loop:
    # read up to buffer length bytes from fdin
    mov     rsi, [rsp + .fdin]
    syscall_read rsi, buffer, BUF_SIZE
    cmp     rax, 0
    je      close_file2     # read zero bytes => EOF
    jl      read_error

    # store the number of bytes read by read()
    mov     r12, rax

    syscall_write STDOUT_FILENO, buffer, r12d
    cmp     rax, r12
    jz      copy_loop

write_error:
    syscall_write 2, err_write_file, ERR_WRITE_FILE_LEN
    syscall_write 2, chr_newline, 1
    jmp close_file2

read_error:
    syscall_write 2, err_read_file, ERR_READ_FILE_LEN
    syscall_write 2, chr_newline, 1

close_file2:
    syscall_close STDOUT_FILENO

close_file1:
    syscall_close [rsp + .fdin]

exit:
    syscall_exit 0
