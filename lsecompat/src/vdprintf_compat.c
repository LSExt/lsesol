/*
 * vdprintf_compat.c - POSIX.1-2008 vdprintf/dprintf implementation for lsecompat
 *
 * How to compile and run the standalone test suite on Solaris / SunOS:
 *   gcc -D_TEST_VDPRINTF_COMPAT -I/usr/local/lse/include -L/usr/local/lse/lib -llsecompat -o test_vdprintf vdprintf_compat.c
 *   ./test_vdprintf
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifndef STACK_BUF_SIZE
#define STACK_BUF_SIZE 1048
#endif

int vdprintf(int fd, const char *format, va_list ap) {
    char stack_buf[STACK_BUF_SIZE];
    char *heap_buf = NULL;
    char *write_ptr = stack_buf;
    int len;
    int written, total = 0;

    if (fd < 0 || !format) {
        errno = EINVAL;
        return -1;
    }

    len = vsnprintf(stack_buf, sizeof(stack_buf), format, ap);
    if (len < 0) {
        return -1;
    }

    if ((size_t)len >= sizeof(stack_buf)) {
        heap_buf = (char *)malloc((size_t)len + 1);
        if (heap_buf) {
            len = vsnprintf(heap_buf, (size_t)len + 1, format, ap);
            if (len < 0) {
                free(heap_buf);
                return -1;
            }
            write_ptr = heap_buf;
        } else {
            len = (int)sizeof(stack_buf) - 1;
        }
    }

    while (total < len) {
        written = write(fd, write_ptr + total, (size_t)(len - total));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            total = -1;
            break;
        }
        if (written == 0) {
            break;
        }
        total += written;
    }

    if (heap_buf) {
        free(heap_buf);
    }

    return total;
}

int dprintf(int fd, const char *format, ...) {
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = vdprintf(fd, format, ap);
    va_end(ap);

    return ret;
}

#ifdef _TEST_VDPRINTF_COMPAT

#include <assert.h>
#include <fcntl.h>

static int test_vd_wrapper(int fd, const char *fmt, ...) {
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vdprintf(fd, fmt, ap);
    va_end(ap);

    return ret;
}

int main(void) {
    int fd, ret;
    char readbuf[512];
    ssize_t bytes_read;

    printf("=== Running vdprintf assert test suite ===\n");

    /* TEST 1: Standard Formatting Length & Output */
    ret = test_vd_wrapper(STDOUT_FILENO, "[Test 1] Hello %s! Value: %d\n", "Solaris", 251);
    assert(ret > 0);

    /* TEST 2: Buffer Truncation & Dynamic Allocation Safety */
    {
        char huge_input[2048];
        memset(huge_input, 'X', sizeof(huge_input) - 1);
        huge_input[sizeof(huge_input) - 1] = '\0';

        ret = test_vd_wrapper(STDOUT_FILENO, "[Test 2] Large string: %s\n", huge_input);
        assert(ret > (int)sizeof(huge_input) - 1);
    }

    /* TEST 3: Argument Types (%p, %u, %ld) */
    ret = test_vd_wrapper(STDOUT_FILENO, "[Test 3] Pointer: %p, Unsigned: %u, Long: %ld\n",
                          (void *)main, 4294967295U, 123456789L);
    assert(ret > 0);

    /* TEST 4: Disk File Descriptor Write & Verification */
    fd = open("/tmp/vdprintf_test.tmp", O_RDWR | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);

    ret = test_vd_wrapper(fd, "PROFTPD_PID=%d\nSTATUS=%s\n", 5002, "OK");
    assert(ret == 27);

    lseek(fd, 0, SEEK_SET);
    bytes_read = read(fd, readbuf, sizeof(readbuf) - 1);
    assert(bytes_read == 27);

    readbuf[bytes_read] = '\0';
    assert(strcmp(readbuf, "PROFTPD_PID=5002\nSTATUS=OK\n") == 0);

    close(fd);
    unlink("/tmp/vdprintf_test.tmp");

    /* TEST 5: Invalid File Descriptor Error Handling (-1) */
    errno = 0;
    ret = test_vd_wrapper(-1, "Fail test: %d\n", 100);
    assert(ret == -1);
    assert(errno == EBADF || errno == EINVAL);

    /* TEST 6: Direct dprintf call (ProFTPD emulation) */
    ret = dprintf(STDOUT_FILENO, "[Test 6] proftpd[%d]: daemon started\n", 5002);
    assert(ret > 0);

    /* TEST 7: Empty String / Zero-Byte Write */
    ret = dprintf(STDOUT_FILENO, "");
    assert(ret == 0);

    /* TEST 8: NULL Format Pointer Handling */
    errno = 0;
    ret = vdprintf(STDOUT_FILENO, NULL, (va_list){0});
    assert(ret == -1);
    assert(errno == EINVAL);

    /* TEST 9: O_APPEND File Descriptor Writes */
    fd = open("/tmp/vdprintf_append.tmp", O_RDWR | O_CREAT | O_APPEND, 0644);
    assert(fd >= 0);

    ret = dprintf(fd, "LINE1\n");
    assert(ret == 6);
    ret = dprintf(fd, "LINE2\n");
    assert(ret == 6);

    lseek(fd, 0, SEEK_SET);
    bytes_read = read(fd, readbuf, sizeof(readbuf) - 1);
    assert(bytes_read == 12);
    readbuf[bytes_read] = '\0';
    assert(strcmp(readbuf, "LINE1\nLINE2\n") == 0);

    close(fd);
    unlink("/tmp/vdprintf_append.tmp");

    printf("=== All vdprintf assertions passed successfully! ===\n");
    return 0;
}

#endif /* _TEST_VDPRINTF_COMPAT */
