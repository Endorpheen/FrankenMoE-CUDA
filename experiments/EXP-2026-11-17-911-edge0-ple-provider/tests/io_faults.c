// Test-only syscall substitution; never linked into a server/runtime bundle.
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

static _Thread_local int mode;
static _Thread_local int calls;

void e0_io_mode(int value) { mode = value; calls = 0; }
int e0_io_calls(void) { return calls; }

ssize_t e0_test_pread(int fd, void * dst, size_t count, off_t offset) {
    ++calls;
    if (mode == 1 && count > 7) count = 7;
    if ((mode == 2 && calls <= 3) || mode == 3) { errno = EINTR; return -1; }
    if (mode == 4 && calls > 1) return 0;
    if (mode == 5) { errno = EIO; return -1; }
    if (mode == 6) return (ssize_t) count + 1;
    return pread(fd, dst, count, offset);
}
