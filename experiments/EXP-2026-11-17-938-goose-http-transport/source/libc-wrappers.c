#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
long gx_close(long fd) { return close((int) fd); }
long gx_read(long fd, unsigned char * buf, long n) { return read((int) fd, buf, (size_t) n); }
long gx_write(long fd, const unsigned char * buf, long n) { return write((int) fd, buf, (size_t) n); }
