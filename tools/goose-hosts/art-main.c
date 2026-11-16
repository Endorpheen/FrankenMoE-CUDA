/* Host for goose/misc/generated-artifact-policy.goose.
 *
 * Product caller: tools/check-generated-files.py (BMOE_GOOSE_ART=1, the
 * default). The script keeps the git index access, the head truncation and the
 * message formatting; the policy decisions come from here.
 *
 *   GEN  <path> <head>            art_is_generated  -> 0|1
 *   LIM  <mb_milli>               art_limit         -> bytes
 *   GATE <all> <size> <limit>     art_gate          -> 0 none, 1 warn, 2 fail
 */
#include "art.h"
#include "host_io.h"

int main(void) {
    char *f[HIO_MAX_FIELDS];
    goose_init(0, NULL);
    while (hio_readline()) {
        int n = hio_split(hio_line, f, HIO_MAX_FIELDS);
        if (n <= 0) continue;
        if (strcmp(f[0], "GEN") == 0 && n == 3) {
            int64_t pn = hio_decode(f[1], hio_a, HIO_MAX_DECODED);
            int64_t hn = hio_decode(f[2], hio_b, HIO_MAX_DECODED);
            if (pn < 0 || hn < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            sl_u8 path = {hio_a, pn};
            sl_u8 head = {hio_b, hn};
            printf("OK %lld\n", (long long) art_is_generated(path, head));
        } else if (strcmp(f[0], "LIM") == 0 && n == 2) {
            printf("OK %lld\n", (long long) art_limit(hio_int(f[1])));
        } else if (strcmp(f[0], "GATE") == 0 && n == 4) {
            printf("OK %lld\n", (long long) art_gate(hio_int(f[1]), hio_int(f[2]), hio_int(f[3])));
        } else {
            printf("ERR unknown request\n");
        }
        fflush(stdout);
    }
    return 0;
}
