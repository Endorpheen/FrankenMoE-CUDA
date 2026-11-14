// Smoke call for the P9-06 set (cli-protocol + thinking-control +
// parse-params): read one session-mode command line from stdin JSON.
#include "p9-06.h"

#include <stdio.h>
#include <string.h>

void goose_init_p9_06(int argc, char **argv);
int64_t cli_read_line(sl_u8 line, int64_t dflt_npredict, int64_t dflt_think,
                      sl_u8 prompt, sl_i64 st);

int run_p9_06(void) {
    static const char line[] =
        "{\"cmd\":\"generate\",\"id\":7,\"n_predict\":16,\"think\":true,"
        "\"prompt\":\"hello\"}";
    static uint8_t prompt[4096];
    static int64_t st[8];
    char *argv[] = {(char *)"smoke", NULL};

    goose_init_p9_06(1, argv);
    sl_u8 text = {(uint8_t *)line, (int64_t)(sizeof line - 1)};
    sl_u8 pbuf = {prompt, (int64_t)sizeof prompt};
    sl_i64 sst = {st, 8};
    int64_t rc = cli_read_line(text, 128, 0, pbuf, sst);

    /* st: 0 kind (0 generate, 1 cancel, 2 close, 3 skip), 1 id, 2 n_predict,
       3 think, 4 clear_kv, 5 prompt length */
    printf("p9-06:  cli_read_line rc=%lld kind=%lld id=%lld n_predict=%lld think=%lld prompt=%.*s\n",
           (long long)rc, (long long)st[0], (long long)st[1], (long long)st[2],
           (long long)st[3], (int)st[5], (char *)prompt);
    return (rc == 0 && st[0] == 0 && st[1] == 7 && st[2] == 16 && st[3] == 1 &&
            st[5] == 5) ? 0 : 1;
}
