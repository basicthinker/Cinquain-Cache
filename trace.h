#ifndef TRACE_H_

#ifdef __KERNEL__

// just ignore tracing in kernel mode
#define trace

#else // user mode

#include <stdio.h>

#define trace(fmt, ...) { \
    fprintf(stderr, "%s:%d, in %s(): ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
}

#endif // __KERNEL__

#endif // TRACE_H_

