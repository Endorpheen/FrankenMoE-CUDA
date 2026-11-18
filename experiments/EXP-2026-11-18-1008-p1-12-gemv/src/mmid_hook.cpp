// A/B seam for the tiled Q4_K MoE kernel.
// Patches the entry of ggml_compute_forward_mul_mat_id_tiled: Q4_K calls go to
// the delivered Goose object, everything else (and FG_MMID_GOOSE=0) goes through
// a trampoline that replays the saved prologue and continues the native body.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>

#include "ggml.h"
#include "ggml-cpu-impl.h"

typedef struct {
    const uint8_t * src0; int64_t ne00; int64_t ne01; int64_t nb01;
    const uint8_t * act;  int64_t nbw1;
    const int32_t * routes; int64_t cne1;
    int64_t nb0, nb1, nb2;
    float * dst;
    int32_t ith, nth;
} fg_mmid_tiled_q4_k_args;

extern "C" int32_t fg_cpu_simd_mmid_tiled_q4_k(const fg_mmid_tiled_q4_k_args * a, uint8_t * scratch);

static void * g_target;
static void * g_tramp;
static unsigned char g_saved[18];
static unsigned long g_goose, g_native, g_fallback;
static unsigned long g_n_notq4, g_n_cne, g_n_extra, g_n_ref, g_n_envoff;
static int g_enabled;

// The delivered kernel takes activation rows already gathered in route order.
// The runtime keeps the full [ne11 x ne12] Q8_K matrix in wdata, so the seam
// gathers (slot + batch*ne11) * nbw1 into a per-thread buffer, exactly the rows
// the native one_expert would read.
static thread_local uint8_t * g_actbuf = nullptr;
static thread_local size_t g_actcap = 0;
static bool gather_act(const uint8_t * wdata, int64_t nbw1, int64_t ne11,
                       const int32_t * routes, int64_t cne1, uint8_t ** out) {
    const size_t need = (size_t) cne1 * (size_t) nbw1;
    if (need > g_actcap) {
        uint8_t * p = (uint8_t *) realloc(g_actbuf, need);
        if (!p) return false;
        g_actbuf = p; g_actcap = need;
    }
    for (int64_t i = 0; i < cne1; ++i) {
        const int64_t i11 = (int64_t) routes[2*i + 0] % ne11;
        const int64_t i12 = (int64_t) routes[2*i + 1];
        memcpy(g_actbuf + (size_t) i * (size_t) nbw1, wdata + (size_t)(i11 + i12*ne11) * (size_t) nbw1, (size_t) nbw1);
    }
    *out = g_actbuf;
    return true;
}

extern "C" bool fg_mmid_hook_entry(const ggml_compute_params * p, ggml_tensor * dst,
                                   int64_t cur_a, int64_t cne1,
                                   const int32_t * routes, char * scratch) {
    typedef bool (*fn_t)(const ggml_compute_params *, ggml_tensor *, int64_t, int64_t, const int32_t *, char *);
    const ggml_tensor * src0 = dst->src[0];
    const ggml_tensor * src1 = dst->src[1];

    // The native dispatcher declines below 8 routed rows (tiled.cpp:1202) and
    // for repacked weights; those calls must stay on the stock vec_dot path,
    // otherwise the Goose tiled kernel would replace a different kernel.
    static const bool tiled_env_on = [] { const char * e = getenv("GGML_CPU_TILED_MM"); return !e || atoi(e) != 0; }();
    static const bool tiled_forced = [] { const char * e = getenv("GGML_CPU_TILED_MM_FORCE"); return e && atoi(e) == 1; }();
    bool use_goose = g_enabled && src0 && src1 &&
                     src0->type == GGML_TYPE_Q4_K && dst->type == GGML_TYPE_F32 &&
                     (src0->ne[0] % 256) == 0 && src0->ne[1] > 0 && cne1 > 0 &&
                     p && !p->use_ref && p->nth > 0 && p->ith >= 0 && p->ith < p->nth &&
                     tiled_env_on && src0->extra == NULL && (cne1 >= 8 || tiled_forced);
    if (!use_goose) {
        g_native++;
        if (!src0 || src0->type != GGML_TYPE_Q4_K) g_n_notq4++;
        else if (src0->extra != NULL) g_n_extra++;
        else if (p && p->use_ref) g_n_ref++;
        else if (!tiled_env_on) g_n_envoff++;
        else g_n_cne++;
        return ((fn_t) g_tramp)(p, dst, cur_a, cne1, routes, scratch);
    }

    fg_mmid_tiled_q4_k_args a{};
    a.src0   = (const uint8_t *) src0->data + cur_a * src0->nb[2];
    a.ne00   = src0->ne[0];
    a.ne01   = src0->ne[1];
    a.nb01   = (int64_t) src0->nb[1];
    const uint8_t * wdata = (const uint8_t *) (src1->type == GGML_TYPE_Q8_K ? src1->data : p->wdata);
    a.nbw1   = (src0->ne[0] / 256) * 292;
    uint8_t * gathered = nullptr;
    if (!gather_act(wdata, a.nbw1, src1->ne[1], routes, cne1, &gathered)) {
        g_fallback++;
        return ((fn_t) g_tramp)(p, dst, cur_a, cne1, routes, scratch);
    }
    a.act    = gathered;
    a.routes = routes;
    a.cne1   = cne1;
    a.nb0    = (int64_t) dst->nb[0];
    a.nb1    = (int64_t) dst->nb[1];
    a.nb2    = (int64_t) dst->nb[2];
    a.dst    = (float *) dst->data;
    a.ith    = p->ith;
    a.nth    = p->nth;

    static int checked = 0;
    const int64_t chk_groups = (a.ne01 + 63) / 64;
    const int64_t chk_r0 = (chk_groups * p->ith) / p->nth * 64;
    const int64_t chk_r1 = MIN(chk_groups * (p->ith + 1) / p->nth * 64, a.ne01);
    const bool do_check = getenv("FG_MMID_CHECK") && checked < 8 && cne1 <= 256 && chk_r1 > chk_r0;
    if (do_check) {
        checked++;
        int64_t max_slot = 0, max_batch = 0;
        for (int64_t i = 0; i < cne1; ++i) {
            if (routes[2*i]   > max_slot)  max_slot  = routes[2*i];
            if (routes[2*i+1] > max_batch) max_batch = routes[2*i+1];
        }
        const size_t shadow_sz = (size_t) (a.nb0 * a.ne01 + a.nb1 * (max_slot + 1) + a.nb2 * (max_batch + 1) + 4096);
        uint8_t * shadow = (uint8_t *) calloc(1, shadow_sz);
        fg_mmid_tiled_q4_k_args b = a;
        b.dst = (float *) shadow;
        const int32_t rcg = fg_cpu_simd_mmid_tiled_q4_k(&b, (uint8_t *) scratch);
        const bool rcn = ((fn_t) g_tramp)(p, dst, cur_a, cne1, routes, scratch);
        long diff = 0; double maxrel = 0.0; long first_diff = -1;
        if (rcg == 0) {
            const int64_t grow0 = chk_r0;
            const int64_t grow1 = chk_r1;
            for (int64_t r = grow0; r < grow1; ++r) {
                for (int64_t m = 0; m < cne1; ++m) {
                    const int64_t idx = r * a.nb0 + (int64_t) routes[2*m] * a.nb1 + (int64_t) routes[2*m+1] * a.nb2;
                    float vg, vn;
                    memcpy(&vg, shadow + idx, 4);
                    memcpy(&vn, ((uint8_t *) dst->data) + idx, 4);
                    if (memcmp(&vg, &vn, 4) != 0) {
                        diff++;
                        if (first_diff < 0) first_diff = r;
                        double d = fabs((double) vg - (double) vn);
                        double rel = d / (fabs((double) vn) > 1e-30 ? fabs((double) vn) : 1.0);
                        if (rel > maxrel) maxrel = rel;
                        if (diff <= 3) fprintf(stderr, "MMID_CHECK diff r=%lld m=%lld native=%.9g goose=%.9g\n",
                                               (long long) r, (long long) m, (double) vn, (double) vg);
                    }
                }
            }
            fprintf(stderr, "MMID_CHECK #%d ith=%d nth=%d first_diff_row=%ld rows=%lld..%lld total=%lld diff=%ld maxrel=%.3e ne00=%lld ne01=%lld cne1=%lld max_slot=%lld max_batch=%lld s3_len=%lld need=%lld\n", checked,
                    p->ith, p->nth, first_diff, (long long) grow0, (long long) grow1, (long long)((grow1-grow0) * cne1), diff, maxrel, (long long) a.ne00, (long long) a.ne01, (long long) cne1,
                    (long long) max_slot, (long long) max_batch,
                    (long long)(a.nb0*a.ne01 + a.nb1*cne1 + a.nb2*cne1),
                    (long long)((a.ne01-1)*a.nb0 + max_slot*a.nb1 + max_batch*a.nb2 + 4));
            if (diff > 0 && !getenv("FG_MMID_DUMPED")) {
                setenv("FG_MMID_DUMPED", "1", 1);
                FILE * f = fopen("/tmp/mmid_dump.bin", "wb");
                if (f) {
                    fwrite(&a.ne00, 8, 1, f); fwrite(&a.ne01, 8, 1, f); fwrite(&a.nb01, 8, 1, f);
                    fwrite(&a.nbw1, 8, 1, f); fwrite(&a.cne1, 8, 1, f);
                    fwrite(&a.nb0, 8, 1, f); fwrite(&a.nb1, 8, 1, f); fwrite(&a.nb2, 8, 1, f);
                    fwrite(&chk_r0, 8, 1, f); fwrite(&chk_r1, 8, 1, f);
                    fwrite(a.src0, (size_t)(a.nb01 * a.ne01), 1, f);
                    fwrite(a.act, (size_t)(a.nbw1 * cne1), 1, f);
                    fwrite(routes, sizeof(int32_t), (size_t)(2*cne1), f);
                    for (int64_t r = chk_r0; r < chk_r1; ++r)
                        for (int64_t m = 0; m < cne1; ++m) {
                            const int64_t idx = r*a.nb0 + (int64_t)routes[2*m]*a.nb1 + (int64_t)routes[2*m+1]*a.nb2;
                            fwrite(((uint8_t*)dst->data)+idx, 4, 1, f);
                            fwrite(shadow+idx, 4, 1, f);
                        }
                    fclose(f);
                    fprintf(stderr, "MMID_CHECK dumped /tmp/mmid_dump.bin\n");
                }
            }
            fprintf(stderr, "MMID_CHECK #%d cur_a=%lld src0[0..3]=%02x%02x%02x%02x act0=%02x%02x%02x%02x route0=(%d,%d)\n",
                    checked, (long long) cur_a,
                    a.src0[0], a.src0[1], a.src0[2], a.src0[3],
                    a.act[0], a.act[1], a.act[2], a.act[3],
                    routes[0], routes[1]);
        } else {
            fprintf(stderr, "MMID_CHECK goose_rc=%d\n", rcg);
        }
        free(shadow);
        g_goose++;
        return rcn;
    }

    const int32_t rc = fg_cpu_simd_mmid_tiled_q4_k(&a, (uint8_t *) scratch);
    if (rc != 0) {
        g_fallback++;
        fprintf(stderr, "MMID_GOOSE_RC %d ne00=%lld ne01=%lld cne1=%lld\n", rc,
                (long long) a.ne00, (long long) a.ne01, (long long) a.cne1);
        return ((fn_t) g_tramp)(p, dst, cur_a, cne1, routes, scratch);
    }
    g_goose++;
    if (g_goose == 1) fprintf(stderr, "MMID_GOOSE_FIRST ne00=%lld ne01=%lld cne1=%lld ith=%d nth=%d\n",
                              (long long) a.ne00, (long long) a.ne01, (long long) a.cne1, a.ith, a.nth);
    return true;
}

__attribute__((constructor)) static void init(void) {
    const char * e = getenv("FG_MMID_GOOSE");
    g_enabled = !(e && e[0] == '0');
    void * h = dlopen("libggml-cpu.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!h) { fprintf(stderr, "MMID_HOOK_NOLIB\n"); _exit(97); }
    g_target = dlsym(h, "ggml_compute_forward_mul_mat_id_tiled");
    if (!g_target) { fprintf(stderr, "MMID_HOOK_NOSYM\n"); _exit(96); }

    unsigned char * t = (unsigned char *) mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC,
                                               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (t == MAP_FAILED) _exit(95);
    memcpy(t, g_target, 18);
    t[18] = 0x48; t[19] = 0xB8;
    uintptr_t back = (uintptr_t) g_target + 18;
    memcpy(&t[20], &back, 8);
    t[28] = 0xFF; t[29] = 0xE0;
    g_tramp = t;

    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t)(page-1));
    if (mprotect(base, (size_t) page, PROT_READ|PROT_WRITE|PROT_EXEC) != 0) _exit(94);
    memcpy(g_saved, g_target, 18);
    unsigned char patch[18] = {0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0,
                               0x90,0x90,0x90,0x90,0x90,0x90};
    uintptr_t dest = (uintptr_t) &fg_mmid_hook_entry;
    memcpy(&patch[2], &dest, 8);
    memcpy(g_target, patch, 18);
    __builtin___clear_cache((char*)g_target, (char*)g_target + 18);
    fprintf(stderr, "MMID_HOOK_PATCHED goose=%d\n", g_enabled);
}

__attribute__((destructor)) static void fini(void) {
    if (g_target) {
        long page = sysconf(_SC_PAGESIZE);
        void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t)(page-1));
        if (mprotect(base, (size_t) page, PROT_READ|PROT_WRITE|PROT_EXEC) == 0)
            memcpy(g_target, g_saved, 18);
    }
    fprintf(stderr, "MMID_HOOK_TOTAL goose=%lu native=%lu fallback=%lu\n", g_goose, g_native, g_fallback);
    fprintf(stderr, "MMID_HOOK_BREAKDOWN native_not_q4k=%lu native_q4k_extra=%lu native_q4k_use_ref=%lu native_q4k_env_off=%lu native_q4k_cne1_lt8=%lu\n",
            g_n_notq4, g_n_extra, g_n_ref, g_n_envoff, g_n_cne);
}
