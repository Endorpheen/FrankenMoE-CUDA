#include <stdio.h>
#include <stdint.h>
#include "listener.h"
#include <string.h>
void goose_init(int, char **);
int64_t ht_open(int64_t, sl_i64);
int64_t ht_read_request(int64_t, sl_u8, sl_i64);
int64_t ht_keep_alive(sl_u8);
int64_t ht_response_head(int64_t, int64_t, int64_t, sl_u8);
int64_t ht_write_all(int64_t, sl_u8);
int64_t ht_close(int64_t);
int64_t ht_accept(int64_t, sl_u8, sl_i64, sl_i64);
int64_t ht_path_span(sl_u8, sl_i64);
int main(int argc, char ** argv) {
    goose_init(0, 0);
    int64_t opts[1] = {0};
    int64_t fd = ht_open(argc > 1 ? atoll(argv[1]) : 0, (sl_i64){opts, 1});
    if (fd < 0) { printf("OPEN_fail=%lld\n", (long long) fd); return 1; }
    printf("LISTENING fd=%lld opts=%lld\n", (long long) fd, (long long) opts[0]);
    fflush(stdout);
    static unsigned char addr[16];
    int64_t alen[1] = {16};
    int64_t ainfo[2] = {0,0};
    int64_t conn = ht_accept(fd, (sl_u8){addr, 16}, (sl_i64){alen, 1}, (sl_i64){ainfo, 2});
    printf("ACCEPT conn=%lld\n", (long long) conn);
    fflush(stdout);
    if (conn < 0) { ht_close(fd); return 1; }
    static unsigned char buf[65536], head[512], body[256];
    for (int req = 0; req < 2; ++req) {
        int64_t info[4] = {0,0,0,0};
        int64_t n = ht_read_request(conn, (sl_u8){buf, sizeof buf}, (sl_i64){info, 4});
        printf("REQ n=%lld method=%lld hend=%lld path_off=%lld path_len=%lld\n",
               (long long) n, (long long) info[1], (long long) info[0],
               (long long) info[2], (long long) info[3]);
        if (n <= 0) break;
        const char * p = "{\"ok\":true}";
        int64_t blen = (int64_t) strlen(p);
        int64_t hl = ht_response_head(200, blen, ht_keep_alive((sl_u8){buf, n}), (sl_u8){head, sizeof head});
        printf("HEAD len=%lld:\n%.*s", (long long) hl, (int) hl, head);
        ht_write_all(conn, (sl_u8){head, hl});
        ht_write_all(conn, (sl_u8){(unsigned char *) p, blen});
        memcpy(body, p, blen);
        fflush(stdout);
        break;
    }
    ht_close(conn);
    ht_close(fd);
    return 0;
}
