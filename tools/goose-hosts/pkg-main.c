/* Host for goose/misc/release-plan.goose.
 *
 * Product callers: scripts/make-release.sh and tools/package-main-runtime.sh
 * (BMOE_GOOSE_PKG=1, the default). The scripts keep git, gh, stat, archive
 * creation and file hashing; tag acceptance, the asset class, both size
 * limits, the retention guards, the dist file list, SHA256SUMS and
 * manifest.json bytes come from here.
 *
 *   VERSION       <tag>                                  -> 0|1
 *   PRERELEASE    <tag>                                  -> 0|1
 *   BASENAME      <path>                                 -> hex
 *   RUNTIME_ASSET <path>                                 -> 0|1
 *   SELECTED      <name>                                 -> 0|1
 *   ASSET_LIMIT   <bytes>                                -> 0|1
 *   TREE_LIMIT    <bytes>                                -> 0|1
 *   KEEP          <has_runtime> <keep_old> <tag> <same> <name> -> 0 delete, 1 keep
 *   DIST                                                 -> hex, one name per line
 *   SUMS          <n> <hash> <name> ...                   -> hex of SHA256SUMS
 *   MANIFEST      <unit> <compiler> <modules> <c_bytes> <obj_bytes>
 *                 <defined> <extern> <h1> <h2> <h3> <h4>   -> hex of manifest.json
 *   COUNT         <build_script_text>                    -> module count
 *   VERSION_LINE  <goose_version_text>                    -> hex
 */
#include "pkg.h"
#include "host_io.h"

static sl_u8 one(const char *field, uint8_t *dst) {
    sl_u8 s = {dst, 0};
    s.len = hio_decode(field, dst, HIO_MAX_DECODED);
    return s;
}

int main(void) {
    char *f[HIO_MAX_FIELDS];
    goose_init(0, NULL);
    while (hio_readline()) {
        int n = hio_split(hio_line, f, HIO_MAX_FIELDS);
        if (n <= 0) continue;
        const char *c = f[0];
        if (strcmp(c, "VERSION") == 0 && n == 2) {
            sl_u8 s = one(f[1], hio_a);
            printf(s.len < 0 ? "ERR malformed field\n" : "OK %lld\n",
                   (long long) (s.len < 0 ? 0 : pkg_version_ok(s)));
        } else if (strcmp(c, "PRERELEASE") == 0 && n == 2) {
            sl_u8 s = one(f[1], hio_a);
            printf(s.len < 0 ? "ERR malformed field\n" : "OK %lld\n",
                   (long long) (s.len < 0 ? 0 : pkg_prerelease(s)));
        } else if (strcmp(c, "BASENAME") == 0 && n == 2) {
            sl_u8 s = one(f[1], hio_a), out = {hio_out, 0};
            if (s.len < 0) { printf("ERR malformed field\n"); continue; }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = pkg_basename(s, out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(c, "RUNTIME_ASSET") == 0 && n == 2) {
            sl_u8 s = one(f[1], hio_a);
            printf(s.len < 0 ? "ERR malformed field\n" : "OK %lld\n",
                   (long long) (s.len < 0 ? 0 : pkg_runtime_asset(s)));
        } else if (strcmp(c, "SELECTED") == 0 && n == 2) {
            sl_u8 s = one(f[1], hio_a);
            printf(s.len < 0 ? "ERR malformed field\n" : "OK %lld\n",
                   (long long) (s.len < 0 ? 0 : pkg_asset_selected(s)));
        } else if (strcmp(c, "ASSET_LIMIT") == 0 && n == 2) {
            printf("OK %lld\n", (long long) pkg_asset_over_limit(hio_int(f[1])));
        } else if (strcmp(c, "TREE_LIMIT") == 0 && n == 2) {
            printf("OK %lld\n", (long long) pkg_tree_over_limit(hio_int(f[1])));
        } else if (strcmp(c, "KEEP") == 0 && n == 6) {
            sl_u8 name = one(f[5], hio_a);
            if (name.len < 0) { printf("ERR malformed field\n"); continue; }
            printf("OK %lld\n", (long long) pkg_keep_asset(hio_int(f[1]), hio_int(f[2]),
                                                           hio_int(f[3]), hio_int(f[4]), name));
        } else if (strcmp(c, "DIST") == 0 && n == 1) {
            sl_u8 out = {hio_out, 0};
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = pkg_dist_files(out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else if (strcmp(c, "SUMS") == 0 && n >= 4 && (n - 2) % 2 == 0) {
            int64_t k = (n - 2) / 2, pairs = 0;
            int64_t ho[8], hl[8], no[8], nl[8];
            int64_t at_h = 0, at_n = 0;
            int bad = 0;
            sl_u8 hash = {hio_b, 0}, name = {hio_c, 0}, out = {hio_out, 0};
            for (pairs = 0; pairs < k && pairs < 8; pairs++) {
                int64_t a = hio_decode(f[2 + 2 * pairs], hio_b + at_h, HIO_MAX_DECODED - at_h);
                int64_t b = hio_decode(f[3 + 2 * pairs], hio_c + at_n, HIO_MAX_DECODED - at_n);
                if (a < 0 || b < 0) { bad = 1; break; }
                ho[pairs] = at_h; hl[pairs] = a;
                no[pairs] = at_n; nl[pairs] = b;
                at_h += a; at_n += b;
            }
            if (bad || pairs < 1) { printf("ERR malformed field\n"); continue; }
            hash.len = at_h;
            name.len = at_n;
            out.len = (int64_t) sizeof hio_out / 4;
            {
                sl_i64 sho = {ho, pairs}, shl = {hl, pairs}, sno = {no, pairs}, snl = {nl, pairs};
                int64_t r = pkg_sha256sums(hash, sho, shl, name, sno, snl, pairs, out);
                printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
            }
        } else if (strcmp(c, "MANIFEST") == 0 && n == 12) {
            int64_t ho[4], hl[4], at = 0;
            int bad = 0;
            sl_u8 unit = {hio_a, 0}, compiler = {hio_b, 0}, hash = {hio_c, 0}, out = {hio_out, 0};
            int64_t k = 0;
            for (k = 0; k < 4; k++) {
                int64_t a = hio_decode(f[8 + k], hio_c + at, HIO_MAX_DECODED - at);
                if (a < 0) { bad = 1; break; }
                ho[k] = at; hl[k] = a;
                at += a;
            }
            if (bad) { printf("ERR malformed field\n"); continue; }
            unit.len = hio_decode(f[1], hio_a, HIO_MAX_DECODED);
            compiler.len = hio_decode(f[2], hio_b, HIO_MAX_DECODED);
            if (unit.len < 0 || compiler.len < 0) { printf("ERR malformed field\n"); continue; }
            hash.len = at;
            out.len = (int64_t) sizeof hio_out / 4;
            {
                sl_i64 sh = {ho, 4}, sl = {hl, 4};
                int64_t r = pkg_manifest_json(unit, compiler, hio_int(f[3]), hio_int(f[4]),
                                              hio_int(f[5]), hio_int(f[6]), hio_int(f[7]),
                                              hash, sh, sl, out);
                printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
            }
        } else if (strcmp(c, "COUNT") == 0 && n == 2) {
            sl_u8 text = one(f[1], hio_a);
            if (text.len < 0) { printf("ERR malformed field\n"); continue; }
            printf("OK %lld\n", (long long) pkg_module_count(text));
        } else if (strcmp(c, "VERSION_LINE") == 0 && n == 2) {
            sl_u8 text = one(f[1], hio_a), out = {hio_out, 0};
            if (text.len < 0) { printf("ERR malformed field\n"); continue; }
            out.len = (int64_t) sizeof hio_out / 4;
            int64_t r = pkg_version_line(text, out);
            printf("OK %s\n", hio_encode(hio_out, r, (char *) hio_line));
        } else {
            printf("ERR unknown request\n");
        }
        fflush(stdout);
    }
    return 0;
}
