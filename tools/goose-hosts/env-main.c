/* Host for goose/misc/build-env.goose.
 *
 * Product callers: scripts/build.sh (BMOE_GOOSE_ENV=1, the default) and
 * scripts/prepare_upstreams.sh. The scripts keep the file system, git and
 * cmake invocations; the defaults, the configure vector, the subdirectory mask
 * and the upstream pin checks come from here.
 *
 *   DIR      <root> <set> <val>                    -> hex of the build dir
 *   JOBS     <set> <val>                           -> hex of the -j value
 *   ARGS     <root> <build_dir> <cc_set> <cc> <cxx_set> <cxx>
 *            <cuda_set> <cuda> <arch_set> <arch>    -> hex, one token per line
 *   TYPE     <set> <val> <multi>                   -> hex of the build type
 *   SUBDIRS  <llama> <cli> <tests> <tools>         -> bit mask
 *   OPTIONS  <cmakelists_text>                     -> hex, NAME<TAB>DEFAULT lines
 *   PIN      <expected> <actual>                   -> 0|1
 *   INTEG    <reverse_ok> <markers>                -> 1|2|3
 *   LINK     <exists> <is_symlink>                 -> 0 ok, 1 guard fires
 *   MARKERS  <text> <snip>...                      -> 0|1, all snippets present
 *   UPSTREAM <prepare_upstreams_text>              -> hex, url<TAB>dir<TAB>sha<TAB>branch
 */
#include "env.h"
#include "host_io.h"

#define HIO_ARG_CAP 4096
static uint8_t hio_args_cc[HIO_ARG_CAP];
static uint8_t hio_args_cxx[HIO_ARG_CAP];
static uint8_t hio_args_cuda[HIO_ARG_CAP];
static uint8_t hio_args_arch[HIO_ARG_CAP];

static int64_t dec_cap(const char *field, uint8_t *dst, int64_t cap, sl_u8 *out) {
    int64_t n = hio_decode(field, dst, cap);
    if (n < 0) return -1;
    out->data = dst;
    out->len = n;
    return n;
}

static int64_t dec(int i, const char *field, uint8_t *dst, int64_t *len, sl_u8 *out) {
    int64_t n = dec_cap(field, dst, HIO_MAX_DECODED, out);
    if (n >= 0 && len != NULL) *len = n;
    return n;
}

int main(void) {
    char *f[HIO_MAX_FIELDS];
    goose_init(0, NULL);
    while (hio_readline()) {
        int n = hio_split(hio_line, f, HIO_MAX_FIELDS);
        if (n <= 0) continue;
        if (strcmp(f[0], "DIR") == 0 && n == 4) {
            sl_u8 root = {NULL, 0}, val = {NULL, 0}, out = {hio_out, 0};
            if (dec(0, f[1], hio_a, NULL, &root) < 0 || dec(0, f[3], hio_b, NULL, &val) < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = env_build_dir(root, hio_int(f[2]), val, out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(f[0], "JOBS") == 0 && n == 3) {
            sl_u8 val = {NULL, 0}, out = {hio_out, 0};
            if (dec(0, f[2], hio_a, NULL, &val) < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = env_jobs(hio_int(f[1]), val, out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(f[0], "ARGS") == 0 && n == 11) {
            sl_u8 root = {NULL, 0}, bd = {NULL, 0}, cc = {NULL, 0}, cxx = {NULL, 0};
            sl_u8 cuda = {NULL, 0}, arch = {NULL, 0}, out = {hio_out, 0};
            /* Each override keeps its own buffer: they are all live at the call,
             * and a compiler path is far below the tool limit. */
            if (dec(0, f[1], hio_a, NULL, &root) < 0 || dec(0, f[2], hio_b, NULL, &bd) < 0 ||
                dec_cap(f[4], hio_args_cc, HIO_ARG_CAP, &cc) < 0 ||
                dec_cap(f[6], hio_args_cxx, HIO_ARG_CAP, &cxx) < 0 ||
                dec_cap(f[8], hio_args_cuda, HIO_ARG_CAP, &cuda) < 0 ||
                dec_cap(f[10], hio_args_arch, HIO_ARG_CAP, &arch) < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = env_cmake_args(root, bd, hio_int(f[3]), cc, hio_int(f[5]), cxx,
                                       hio_int(f[7]), cuda, hio_int(f[9]), arch, out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(f[0], "TYPE") == 0 && n == 4) {
            sl_u8 val = {NULL, 0}, out = {hio_out, 0};
            if (dec(0, f[2], hio_a, NULL, &val) < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = env_build_type(hio_int(f[1]), val, hio_int(f[3]), out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(f[0], "SUBDIRS") == 0 && n == 5) {
            printf("OK %lld\n", (long long) env_subdirs(hio_int(f[1]), hio_int(f[2]),
                                                        hio_int(f[3]), hio_int(f[4])));
        } else if (strcmp(f[0], "OPTIONS") == 0 && n == 2) {
            sl_u8 text = {NULL, 0}, out = {hio_out, 0};
            if (dec(0, f[1], hio_a, NULL, &text) < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = env_option_table(text, out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(f[0], "PIN") == 0 && n == 3) {
            sl_u8 exp = {NULL, 0}, act = {NULL, 0};
            if (dec(0, f[1], hio_a, NULL, &exp) < 0 || dec(0, f[2], hio_b, NULL, &act) < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            printf("OK %lld\n", (long long) env_pin_ok(exp, act));
        } else if (strcmp(f[0], "INTEG") == 0 && n == 3) {
            printf("OK %lld\n", (long long) env_integration_state(hio_int(f[1]), hio_int(f[2])));
        } else if (strcmp(f[0], "LINK") == 0 && n == 3) {
            printf("OK %lld\n", (long long) env_link_guard(hio_int(f[1]), hio_int(f[2])));
        } else if (strcmp(f[0], "MARKERS") == 0 && n >= 3) {
            sl_u8 text = {NULL, 0};
            sl_u8 snip = {hio_b, 0};
            int64_t off[64], len[64];
            int64_t k = 0, at = 0;
            if (dec(0, f[1], hio_a, NULL, &text) < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            while (2 + k < n && k < 64) {
                int64_t l = hio_decode(f[2 + k], hio_b + at, HIO_MAX_DECODED - at);
                if (l < 0) { at = -1; break; }
                off[k] = at;
                len[k] = l;
                at += l;
                k++;
            }
            if (at < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            snip.len = at;
            {
                sl_i64 swo = {off, k}, swl = {len, k};
                printf("OK %lld\n", (long long) env_markers_all(text, snip, swo, swl, k));
            }
        } else if (strcmp(f[0], "UPSTREAM") == 0 && n == 2) {
            sl_u8 text = {NULL, 0}, out = {hio_out, 0};
            if (dec(0, f[1], hio_a, NULL, &text) < 0) {
                printf("ERR malformed field\n");
                continue;
            }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = env_upstream_pins(text, out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else {
            printf("ERR unknown request\n");
        }
        fflush(stdout);
    }
    return 0;
}
