#ifndef VDPRINTF_COMPAT_H
#define VDPRINTF_COMPAT_H

#include <sys/types.h>

#include <stdarg.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

int vdprintf(int fd, const char *format, va_list ap);
int dprintf(int fd, const char *format, ...);

#ifdef __cplusplus
}
#endif


#endif /* VDPRINTF_COMPAT_H */
