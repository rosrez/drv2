#ifndef MACROS_H
#define MACROS_H

#define serr_exit(arg...) do { fprintf(stderr, "%s: ", arg); perror(NULL); exit(EXIT_FAILURE); } while (0)

#define err_exit(arg...) do { fprintf(stderr, "%s\n", arg); exit(EXIT_FAILURE); } while (0)


#endif
