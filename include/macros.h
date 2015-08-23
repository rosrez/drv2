#ifndef MACROS_H
#define MACROS_H

#define serr_exit(arg...) do { \
        fprintf(stderr, arg); \
        fprintf(stderr, " : "); \
        perror(NULL); \
        exit(EXIT_FAILURE); \
    } while (0)

#define err_exit(fmt, arg...) do { \
        fprintf(stderr, fmt, arg); \
        fprintf(stderr, "\n"); \
        exit(EXIT_FAILURE); \
    } while (0)

#endif
