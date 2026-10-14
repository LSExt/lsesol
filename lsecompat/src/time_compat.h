#ifndef _TIME_COMPAT_H
#define _TIME_COMPAT_H

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* POSIX.1b Clock Identifiers if missing */
#ifndef CLOCK_REALTIME
#  define CLOCK_REALTIME 0
#endif

#ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC 1
#endif

/* Fallback timezone structure for legacy Solaris 2.5.1 */
#ifndef _TIMEZONE_T_DEFINED
#define _TIMEZONE_T_DEFINED
struct __timezone_type {
    struct __timezone_type *next;
    char tz_is_set;
    char *tzname_copy[2];
    char abbrs[1];
};
typedef struct __timezone_type *timezone_t;
#endif


/* Fallback typedef for Y2038 32-bit timestamp compatibility */
#ifndef time32_t
typedef int32_t time32_t;
#endif

/* 
 * If system headers didn't declare prototypes, declare them here.
 * Weak binding in time_compat.c will supply implementation without symbol conflicts.
 */
int utimes(const char *path, const struct timeval times[2]);

#ifdef __cplusplus
}
#endif

#endif /* _TIME_COMPAT_H */
