#include <unistd.h>
#include <sys/socket.h>
typedef struct { unsigned char * data; long len; } sl_u8x;
typedef struct { long * data; long len; } sl_i64x;
long gx_close(long fd) { return close((int) fd); }
long gx_read(long fd, sl_u8x buf, long n) { return read((int) fd, buf.data, (size_t) n); }
long gx_write(long fd, const unsigned char * data, long len) { return send((int) fd, data, (size_t) len, MSG_NOSIGNAL); }
long gx_accept(long fd, sl_u8x addr, sl_i64x alen) {
    socklen_t l = (socklen_t) (alen.data ? alen.data[0] : 16);
    int c = accept((int) fd, (struct sockaddr *) addr.data, &l);
    if (alen.data) alen.data[0] = (long) l;
    return (long) c;
}

// The socket family. Two Goose modules in one unit need the same libc calls with
// different signatures (http-listen passes a raw scratch address, http-listener a
// slice), and one C symbol cannot carry two prototypes; each module therefore binds
// its own name. These forwards translate only - no decision lives here.
long gx_socket(long domain, long kind, long proto) { return socket((int) domain, (int) kind, (int) proto); }
long gx_bind(long fd, sl_u8x addr, long len) {
    return bind((int) fd, (const struct sockaddr *) addr.data, (socklen_t) len);
}
long gx_listen(long fd, long backlog) { return listen((int) fd, (int) backlog); }
long gx_setsockopt(long fd, long level, long name, sl_u8x val, long len) {
    return setsockopt((int) fd, (int) level, (int) name, val.data, (socklen_t) len);
}
long hl_socket(long domain, long kind, long proto) { return socket((int) domain, (int) kind, (int) proto); }
long hl_bind(long fd, unsigned long addr, long len) {
    return bind((int) fd, (const struct sockaddr *) addr, (socklen_t) len);
}
long hl_listen(long fd, long backlog) { return listen((int) fd, (int) backlog); }
long hl_accept(long fd, unsigned long addr, unsigned long len) {
    socklen_t l = (socklen_t) len;
    return (long) accept((int) fd, (struct sockaddr *) addr, &l);
}
long hl_getsockname(long fd, unsigned long addr, unsigned long len_addr) {
    return (long) getsockname((int) fd, (struct sockaddr *) addr, (socklen_t *) len_addr);
}
