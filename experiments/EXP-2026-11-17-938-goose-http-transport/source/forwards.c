#include <unistd.h>
#include <sys/socket.h>
typedef struct { unsigned char * data; long len; } sl_u8x;
typedef struct { long * data; long len; } sl_i64x;
long gx_close(long fd) { return close((int) fd); }
long gx_read(long fd, sl_u8x buf, long n) { return read((int) fd, buf.data, (size_t) n); }
long gx_write(long fd, const unsigned char * data, long len) { return write((int) fd, data, (size_t) len); }
long gx_accept(long fd, sl_u8x addr, sl_i64x alen) {
    socklen_t l = (socklen_t) (alen.data ? alen.data[0] : 16);
    int c = accept((int) fd, (struct sockaddr *) addr.data, &l);
    if (alen.data) alen.data[0] = (long) l;
    return (long) c;
}
