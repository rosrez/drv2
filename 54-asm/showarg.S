.intel_syntax noprefix

# DATA SEGMENT  -------------------------

    .section .data

format: .asciz "%s\n"

# CODE SEGMENT (ENTRY POINT) ------------

    .section .text

    .global main
    .type main, @function

# We are being called from libc: main(int argc, char *argv[]),
# therefore the C ABI applies: rdi = argc, rsi = argv

main:
.equ    .argv, 0

    push    rbp
    mov     rbp, rsp
    sub     rsp, 16             # make room for local variables

    mov     rcx, rsi            # argv
    mov     rsi, [rcx]          # argv[0]

more:
    mov     [rsp + .argv], rcx
    lea     rdi, [format]
    xor     eax, eax            # zero floating-point arguments
    call    printf

    mov     rcx, [rsp + .argv]
    add     rcx, 8
    mov     rsi, [rcx]
    test    rsi, rsi
    jnz     more

    xor     eax, eax            # return 0;
    leave
    ret
