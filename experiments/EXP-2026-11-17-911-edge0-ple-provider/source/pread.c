// Temporary OS-ABI adapter debt: one syscall translation, no read policy.
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <unistd.h>
#include "runtime.h"

int64_t e0_pread_bytes(int64_t fd, sl_u8 dst, int64_t offset) {
    const ssize_t n = pread((int) fd, dst.data, (size_t) dst.len, (off_t) offset);
    return n < 0 ? -(int64_t) errno : (int64_t) n;
}
