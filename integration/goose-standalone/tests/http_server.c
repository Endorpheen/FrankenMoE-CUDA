// Standalone server loop: the Goose listener plus the Goose router.
//
// Every decision in this loop is Goose's: the socket lifecycle, the request
// line, the path span, which route it is, and which status that route answers.
// The host supplies buffers and calls into the unit; it does not choose routes.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unit.h"

#define MAX_REQ 65536

static const char * body_for(int64_t route) {
    switch (route) {
        case 1:  return "{\"status\":\"ok\"}";
        case 2:  return "# metrics\n";
        case 3:  return "{\"model\":\"apex\"}";
        case 5:  return "{\"data\":[]}";
        case 8:  return "{}";
        default: return "{\"error\":{\"message\":\"unknown route\",\"code\":404}}";
    }
}

// route-table.goose uses 0 for "no route matched"; anything positive is a hit.
static int64_t status_for(int64_t route) {
    return route > 0 ? 200 : 404;
}

static int serve(int64_t conn) {
    static unsigned char req[MAX_REQ];
    static unsigned char head[512];
    static unsigned char path[1024];

    int64_t info[4] = {0, 0, 0, 0};
    const int64_t n = ht_read_request(conn, (sl_u8){ req, MAX_REQ }, (sl_i64){ info, 4 });
    if (n <= 0) {
        return -1;
    }
    const int64_t method = info[1];
    const int64_t off = info[2];
    const int64_t len = info[3];
    if (off < 0 || len < 0 || off + len > (int64_t) sizeof(path)) {
        return -1;
    }
    memcpy(path, req + off, (size_t) len);

    const int64_t route = route_id(method, (sl_u8){ path, len });
    const char * body = body_for(route);
    const int64_t blen = (int64_t) strlen(body);
    const int64_t keep = ht_keep_alive((sl_u8){ req, n });

    const int64_t hl = ht_response_head(status_for(route), blen, keep, (sl_u8){ head, sizeof(head) });
    if (hl < 0) {
        return -1;
    }
    if (ht_write_all(conn, (sl_u8){ head, hl }) != 0) {
        return -1;
    }
    if (ht_write_all(conn, (sl_u8){ (unsigned char *) body, blen }) != 0) {
        return -1;
    }
    printf("REQ method=%lld path=%.*s route=%lld keep=%lld\n",
           (long long) method, (int) len, path, (long long) route, (long long) keep);
    return keep ? 0 : 1;
}

int main(int argc, char ** argv) {
    goose_init(0, NULL);
    const int64_t port = argc > 1 ? atoll(argv[1]) : 0;
    const int64_t max_req = argc > 2 ? atoll(argv[2]) : 1;

    int64_t opts[1] = { 0 };
    const int64_t lfd = ht_open(port, (sl_i64){ opts, 1 });
    if (lfd < 0) {
        printf("SERVER open failed step=%lld\n", (long long) (-lfd));
        return 1;
    }
    printf("SERVER listening fd=%lld port=%lld\n", (long long) lfd, (long long) port);
    fflush(stdout);

    static unsigned char addr[16];
    int64_t alen[1] = { 16 };
    int64_t ainfo[2] = { 0, 0 };

    int64_t served = 0;
    while (served < max_req) {
        const int64_t conn = ht_accept(lfd, (sl_u8){ addr, 16 }, (sl_i64){ alen, 1 }, (sl_i64){ ainfo, 2 });
        if (conn < 0) {
            printf("SERVER accept failed\n");
            break;
        }
        served++;
        serve(conn);
        ht_close(conn);
    }
    ht_close(lfd);
    printf("SERVER done served=%lld\n", (long long) served);
    return 0;
}
