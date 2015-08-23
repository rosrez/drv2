#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

static void cpuid_brand() {
    int code;
    int32_t out[12];

    printf("\n\ncpuid instruction: multiple inputs =====\n");

    /*
     * CPUID stores result in EAX, EBX, ECX, EDX.
     * Move returned data to temporary storage.
     */

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

