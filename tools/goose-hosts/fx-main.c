/* Host for goose/misc/fixture-plan.goose.
 *
 * Product caller: scripts/make-tiny-moe.py (BMOE_GOOSE_FX=1, the default). The
 * script keeps gguf, numpy and the writer; the tensor plan — every name, numpy
 * shape and per-layer seed, in emission order — the mode guards, the shard
 * count and the banner come from here.
 *
 *   ARCH      <name>                 -> arch id, -1 unknown
 *   GUARD     <arch> <split>         -> "0 -" or "1 <message hex>"
 *   SMALLFIRST <split>               -> 0|1
 *   COUNT     <arch>                 -> tensor count
 *   SHARDS    <n> <per_shard> <small_first> -> shard count
 *   TENSOR    <arch> <i>             -> hex of "<name> <shape> <seed>"
 *   METACOUNT <arch>                 -> metadata count
 *   META      <arch> <i>             -> hex of the metadata row
 *   BANNER    <arch> <out_path>      -> hex of the printed line
 */
#include "fx.h"
#include "host_io.h"

int main(void) {
    char *f[HIO_MAX_FIELDS];
    goose_init(0, NULL);
    while (hio_readline()) {
        int n = hio_split(hio_line, f, HIO_MAX_FIELDS);
        if (n <= 0) continue;
        const char *c = f[0];
        if (strcmp(c, "ARCH") == 0 && n == 2) {
            sl_u8 name = {hio_a, 0};
            name.len = hio_decode(f[1], hio_a, 1024);
            if (name.len < 0) { printf("ERR malformed field\n"); continue; }
            printf("OK %lld\n", (long long) fx_arch(name));
        } else if (strcmp(c, "GUARD") == 0 && n == 3) {
            /* fx_split_guard writes the message without returning its length, so
             * the buffer is pre-filled with a sentinel and the written prefix is
             * measured. This keeps the text owned by the module. */
            int64_t i = 0;
            char enc[2 * 4096];
            memset(hio_out, 0x7f, 4096);
            sl_u8 msg = {hio_out, 4096};
            int64_t r = fx_split_guard(hio_int(f[1]), hio_int(f[2]), msg);
            if (r == 0) {
                printf("OK 0 -\n");
                continue;
            }
            while (i < 4096 && hio_out[i] != 0x7f) i++;
            printf("OK 1 %s\n", hio_encode(hio_out, i, enc));
        } else if (strcmp(c, "SMALLFIRST") == 0 && n == 2) {
            printf("OK %lld\n", (long long) fx_small_first(hio_int(f[1])));
        } else if (strcmp(c, "COUNT") == 0 && n == 2) {
            printf("OK %lld\n", (long long) fx_tensor_count(hio_int(f[1])));
        } else if (strcmp(c, "SHARDS") == 0 && n == 4) {
            printf("OK %lld\n", (long long) fx_shards(hio_int(f[1]), hio_int(f[2]), hio_int(f[3])));
        } else if (strcmp(c, "TENSOR") == 0 && n == 3) {
            sl_u8 out = {hio_out, 0};
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = fx_tensor(hio_int(f[1]), hio_int(f[2]), out);
            if (r < 0) {
                printf("ERR out of range\n");
                continue;
            }
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(c, "METACOUNT") == 0 && n == 2) {
            printf("OK %lld\n", (long long) fx_meta_count(hio_int(f[1])));
        } else if (strcmp(c, "META") == 0 && n == 3) {
            sl_u8 out = {hio_out, 0};
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = fx_meta(hio_int(f[1]), hio_int(f[2]), out);
            if (r < 0) {
                printf("ERR out of range\n");
                continue;
            }
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(c, "BANNER") == 0 && n == 3) {
            sl_u8 path = {hio_b, 0}, out = {hio_out, 0};
            path.len = hio_decode(f[2], hio_b, HIO_MAX_DECODED);
            if (path.len < 0) { printf("ERR malformed field\n"); continue; }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = fx_banner(hio_int(f[1]), path, out);
            if (r < 0) {
                printf("ERR out of range\n");
                continue;
            }
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else {
            printf("ERR unknown request\n");
        }
        fflush(stdout);
    }
    return 0;
}
