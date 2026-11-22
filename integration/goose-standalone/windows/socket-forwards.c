#include "platform.h"

#include <limits.h>

typedef struct { unsigned char *data; int64_t len; } byte_slice;
typedef struct { int64_t *data; int64_t len; } int_slice;

static int64_t socket_id(SOCKET fd) {
    return fd == INVALID_SOCKET ? -1 : (int64_t)fd;
}

int64_t gx_close(int64_t fd) { return closesocket((SOCKET)fd); }
int64_t gx_read(int64_t fd, byte_slice buf, int64_t n) {
    if (n < 0 || n > buf.len || n > INT_MAX || (n && !buf.data)) return -1;
    return recv((SOCKET)fd, (char *)buf.data, (int)n, 0);
}
int64_t gx_write(int64_t fd, byte_slice buf, int64_t n) {
    if (n < 0 || n > buf.len || n > INT_MAX || (n && !buf.data)) return -1;
    return send((SOCKET)fd, (const char *)buf.data, (int)n, 0);
}
int64_t gx_accept(int64_t fd, byte_slice addr, int_slice alen) {
    if (!addr.data) return socket_id(accept((SOCKET)fd, NULL, NULL));
    if (!alen.data || alen.len < 1 || alen.data[0] < 0 ||
        alen.data[0] > addr.len || alen.data[0] > INT_MAX) return -1;
    int len = (int)alen.data[0];
    const SOCKET client = accept((SOCKET)fd, (struct sockaddr *)addr.data, &len);
    if (client != INVALID_SOCKET) alen.data[0] = len;
    return socket_id(client);
}
int64_t gx_socket(int64_t domain, int64_t kind, int64_t proto) {
    return socket_id(socket((int)domain, (int)kind, (int)proto));
}
int64_t gx_bind(int64_t fd, byte_slice addr, int64_t n) {
    if (!addr.data || n < 0 || n > addr.len || n > INT_MAX) return -1;
    return bind((SOCKET)fd, (const struct sockaddr *)addr.data, (int)n);
}
int64_t gx_listen(int64_t fd, int64_t backlog) {
    return listen((SOCKET)fd, (int)backlog);
}
int64_t gx_setsockopt(int64_t fd, int64_t level, int64_t name, byte_slice value, int64_t n) {
    if (level != 1 || name != 2 || n != 4 || value.len < 4 || !value.data) return -1;
    const BOOL exclusive = TRUE;
    return setsockopt((SOCKET)fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                      (const char *)&exclusive, sizeof exclusive);
}
int64_t hl_socket(int64_t domain, int64_t kind, int64_t proto) {
    return gx_socket(domain, kind, proto);
}
int64_t hl_bind(int64_t fd, uint64_t address, int64_t size) {
    return gx_bind(fd, (byte_slice){(void *)(uintptr_t)address, size}, size);
}
int64_t hl_listen(int64_t fd, int64_t backlog) { return gx_listen(fd, backlog); }
int64_t hl_accept(int64_t fd, uint64_t addr, uint64_t size) {
    if (size > INT_MAX) return -1;
    int64_t n = (int64_t)size;
    return gx_accept(fd, (byte_slice){(void *)(uintptr_t)addr, n}, (int_slice){&n, 1});
}
int64_t hl_getsockname(int64_t fd, uint64_t addr, uint64_t len_addr) {
    if (!addr || !len_addr) return -1;
    int64_t *length = (void *)(uintptr_t)len_addr;
    if (*length < 0 || *length > INT_MAX) return -1;
    int n = (int)*length;
    const int status = getsockname((SOCKET)fd, (struct sockaddr *)(uintptr_t)addr, &n);
    if (!status) *length = n;
    return status;
}
