// Differential gate for the CPU-feature slice.
//
// Oracle:
//   - the Linux decode is the real ggml-feats.h, compiled here with getauxval
//     and prctl redirected to injected values, so the native bit decode (the
//     FPHP+ASIMDHP conjunction, the SVE vector-length mask and the "failed
//     prctl leaves zero" rule) is executed, not retyped;
//   - the Apple and Windows decodes are transcribed from ggml-feats.h:112-140,
//     because the host cannot include sysctl.h or windows.h;
//   - both score functions are transcribed from the native #ifdef chains with
//     the compile-flag word lifted to a parameter (the native word is fixed at
//     build time);
//   - the kleidiai mask and name are transcribed from kleidiai.cpp:75-90 and
//     :317-321,380-386.
//
// Port: source/feat-cpu-features.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>

// ---- native ggml-feats.h, Linux branch, with injected platform calls -------
static unsigned long g_hwcap   = 0;
static unsigned long g_hwcap2  = 0;
static int           g_sve_vl  = -1;

#define getauxval fg_getauxval
#define prctl     fg_prctl
#define __aarch64__ 1
#include "ggml-feats.h"
#undef __aarch64__
#undef prctl
#undef getauxval

extern "C" unsigned long fg_getauxval(unsigned long type) noexcept {
    if (type == AT_HWCAP)  { return g_hwcap; }
    if (type == AT_HWCAP2) { return g_hwcap2; }
    return 0;
}

extern "C" int fg_prctl(int option, ...) noexcept {
    (void) option;
    return g_sve_vl;
}

#include "feat.h"

static int g_checks = 0, g_fails = 0;

static void expect(const char * group, const char * what, int64_t got, int64_t want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 40) {
            std::printf("FAIL %s / %s: got %lld want %lld\n", group, what,
                        (long long) got, (long long) want);
        }
    }
}

// ---- transcribed Apple and Windows decodes --------------------------------
// ggml-feats.h:124-137 (Apple): a failed sysctl leaves the flag false.
static int64_t nat_apple(int64_t vals, int64_t ok) {
    int64_t bits = 0;
    for (int i = 0; i < 7; ++i) {
        const int64_t key = (int64_t) 1 << i;
        if ((ok & key) && (vals & key)) { bits |= key; }
    }
    return bits;
}

// ggml-feats.h:134-140 (Windows): one IsProcessorFeaturePresent id per flag.
static int64_t nat_windows(int64_t pf) { return pf & 127; }

// ---- transcribed score chains ---------------------------------------------
// cpu-feats.cpp:7-44. flags: bit 0 DOTPROD, 1 FP16, 2 SVE, 3 MATMUL_INT8,
// 4 SVE2, 5 SME.
static int64_t nat_arch64_score(int64_t f, int64_t flags) {
    int64_t score = 1;
    if (flags & 1)  { if (!(f & 1))  { return 0; } score += 1 << 1; }
    if (flags & 2)  { if (!(f & 2))  { return 0; } score += 1 << 2; }
    if (flags & 4)  { if (!(f & 4))  { return 0; } score += 1 << 3; }
    if (flags & 8)  { if (!(f & 16)) { return 0; } score += 1 << 4; }
    if (flags & 16) { if (!(f & 8))  { return 0; } score += 1 << 5; }
    if (flags & 32) { if (!(f & 32)) { return 0; } score += 1 << 6; }
    return score;
}

// cpu-feats.cpp:263-325. flags: 0 FMA, 1 F16C, 2 SSE42, 3 BMI2, 4 AVX, 5 AVX2,
// 6 AVX_VNNI, 7 AVX512, 8 AVX512_VBMI, 9 AVX512_BF16, 10 AVX512_VNNI,
// 11 AMX_INT8. The AVX512 block needs F, CD, VL, DQ and BW together.
static int64_t nat_x86_score(int64_t f, int64_t flags) {
    int64_t score = 1;
    if (flags & 1)    { if (!(f & 1))   { return 0; } score += 1; }
    if (flags & 2)    { if (!(f & 2))   { return 0; } score += 2; }
    if (flags & 4)    { if (!(f & 4))   { return 0; } score += 4; }
    if (flags & 8)    { if (!(f & 8))   { return 0; } score += 8; }
    if (flags & 16)   { if (!(f & 16))  { return 0; } score += 16; }
    if (flags & 32)   { if (!(f & 32))  { return 0; } score += 32; }
    if (flags & 64)   { if (!(f & 64))  { return 0; } score += 64; }
    if (flags & 128)  {
        if (!(f & 128) || !(f & 256) || !(f & 512) || !(f & 1024) || !(f & 2048)) { return 0; }
        score += 128;
    }
    if (flags & 256)  { if (!(f & 4096))  { return 0; } score += 256; }
    if (flags & 512)  { if (!(f & 8192))  { return 0; } score += 512; }
    if (flags & 1024) { if (!(f & 16384)) { return 0; } score += 1024; }
    if (flags & 2048) { if (!(f & 32768)) { return 0; } score += 2048; }
    return score;
}

// kleidiai.cpp:317-321 and :380-386.
static int64_t nat_kleidiai_mask(int64_t f, int64_t sve_cnt, int64_t sme_cores) {
    int64_t mask = 0;
    if (f & 1)  { mask |= 1; }
    if (f & 16) { mask |= 2; }
    if (f & 2)  { mask |= 32; }
    if (sve_cnt == 32) { mask |= 4; }
    if (sme_cores > 0 && (f & 32)) {
        mask |= 8;
        if (f & 64) { mask |= 16; }
    }
    return mask;
}

// kleidiai.cpp:75-90.
static const char * nat_kleidiai_name(int64_t mask) {
    if (mask == 0)        { return "NONE"; }
    if ((mask & 16) == 16) { return "SME2"; }
    if ((mask & 8) == 8)   { return "SME"; }
    if ((mask & 4) == 4)   { return "SVE"; }
    if ((mask & 2) == 2)   { return "I8MM"; }
    if ((mask & 1) == 1)   { return "DOTPROD"; }
    return "UNKNOWN";
}

// ---- decode comparison ----------------------------------------------------
// Runs the real header and the port on the same injected values and compares
// the whole record, field by field.
static void gate_linux(const char * id, unsigned long hwcap, unsigned long hwcap2, int vl) {
    g_hwcap  = hwcap;
    g_hwcap2 = hwcap2;
    g_sve_vl = vl;

    const ggml_feats_arch64_runtime_t nat = ggml_feats_get_arch64_runtime();

    int64_t out[8] = { 9, 9, 9, 9, 9, 9, 9, 9 };
    const int64_t n = feat_arch64_linux((int64_t) hwcap, (int64_t) hwcap2, (int64_t) vl, sl_i64{out, 8});

    expect(id, "count", n, 8);
    expect(id, "dotprod", out[0], nat.has_dotprod ? 1 : 0);
    expect(id, "fp16",    out[1], nat.has_fp16 ? 1 : 0);
    expect(id, "sve",     out[2], nat.has_sve ? 1 : 0);
    expect(id, "sve2",    out[3], nat.has_sve2 ? 1 : 0);
    expect(id, "i8mm",    out[4], nat.has_i8mm ? 1 : 0);
    expect(id, "sme",     out[5], nat.has_sme ? 1 : 0);
    expect(id, "sme2",    out[6], nat.has_sme2 ? 1 : 0);
    expect(id, "sve_cnt", out[7], nat.sve_cnt);
}

int main() {
    goose_init(0, nullptr);
    setvbuf(stdout, nullptr, _IONBF, 0);

    // ---- Linux decode: 16 cases ------------------------------------------
    gate_linux("L0  empty",            0,               0,               -1);
    gate_linux("L1  dotprod",          1ul << 20,       0,               -1);
    gate_linux("L2  fphp only",        1ul << 9,        0,               -1);
    gate_linux("L3  asimdhp only",     1ul << 10,       0,               -1);
    gate_linux("L4  fp16 pair",        (1ul << 9) | (1ul << 10), 0,      -1);
    gate_linux("L5  sve vl 64",        1ul << 22,       0,               64);
    gate_linux("L6  sve prctl fails",  1ul << 22,       0,               -1);
    gate_linux("L7  sve vl masked",    1ul << 22,       0,               0x10040);
    gate_linux("L8  sve2",             0,               1ul << 1,        -1);
    gate_linux("L9  i8mm",             0,               1ul << 13,       -1);
    gate_linux("L10 sme",              0,               1ul << 23,       -1);
    gate_linux("L11 sme2",             0,               1ul << 37,       -1);
    gate_linux("L12 all bits",         (1ul << 20) | (1ul << 9) | (1ul << 10) | (1ul << 22),
                                       (1ul << 1) | (1ul << 13) | (1ul << 23) | (1ul << 37),
                                       0x1ffff);
    gate_linux("L13 foreign bits",     1ul << 63,       1ul << 62,       -1);
    gate_linux("L14 no sve, vl set",   1ul << 20,       0,               64);
    gate_linux("L15 dotprod+sve2",     (1ul << 20) | (1ul << 10), 1ul << 1, -1);

    // ---- Apple decode: 5 cases -------------------------------------------
    {
        int64_t out[8];
        struct { const char * id; int64_t vals, ok, want_bits; } c[] = {
            { "A0  all set",    127, 127, 127 },
            { "A1  none set",     0, 127,   0 },
            { "A2  all failed", 127,   0,   0 },
            { "A3  partial ok", 127,   5,   5 },
            { "A4  fp16 only",   2, 127,   2 },
        };
        for (auto & k : c) {
            const int64_t n = feat_arch64_apple(k.vals, k.ok, sl_i64{out, 8});
            int64_t bits = 0;
            for (int i = 0; i < 7; ++i) { if (out[i]) { bits |= (int64_t) 1 << i; } }
            expect(k.id, "count",   n, 8);
            expect(k.id, "bits",    bits, nat_apple(k.vals, k.ok));
            expect(k.id, "want",    bits, k.want_bits);
            expect(k.id, "sve_cnt", out[7], 0);
        }
    }

    // ---- Windows decode: 4 cases -----------------------------------------
    {
        int64_t out[8];
        struct { const char * id; int64_t pf, want; } c[] = {
            { "W0 empty",      0,   0 },
            { "W1 all",      127, 127 },
            { "W2 fp16+sme2", 66,  66 },
            { "W3 foreign",  1 << 20, 0 },
        };
        for (auto & k : c) {
            const int64_t n = feat_arch64_windows(k.pf, sl_i64{out, 8});
            int64_t bits = 0;
            for (int i = 0; i < 7; ++i) { if (out[i]) { bits |= (int64_t) 1 << i; } }
            expect(k.id, "count", n, 8);
            expect(k.id, "bits",  bits, nat_windows(k.pf));
            expect(k.id, "want",  bits, k.want);
            expect(k.id, "sve_cnt", out[7], 0);
        }
    }

    // ---- aarch64 score: 10 cases -----------------------------------------
    {
        struct { const char * id; int64_t bits, flags; } c[] = {
            { "S0  no flags",              0,    0 },
            { "S1  all satisfied",       127,   63 },
            { "S2  dotprod missing",       0,    1 },
            { "S3  fp16 only",             2,    2 },
            { "S4  sve2 missing",          4,   20 },
            { "S5  sve+sve2",             12,   20 },
            { "S6  sme",                  32,   32 },
            { "S7  i8mm",                 16,    8 },
            { "S8  all flags, no sme2",   95,   63 },
            { "S9  fp16 pair missing",     8,    2 },
        };
        for (auto & k : c) {
            expect(k.id, "score", feat_arch64_score(k.bits, k.flags),
                   nat_arch64_score(k.bits, k.flags));
        }
    }

    // ---- x86 score: 11 cases ---------------------------------------------
    {
        struct { const char * id; int64_t bits, flags; } c[] = {
            { "X0  no flags",                0,    0 },
            { "X1  all satisfied",       65535, 4095 },
            { "X2  fma missing",             0,    1 },
            { "X3  avx2",                   32,   32 },
            { "X4  avx512 no bw",          128 + 256 + 512 + 1024, 128 },
            { "X5  avx512 complete",       128 + 256 + 512 + 1024 + 2048, 128 },
            { "X6  amx missing",             0, 2048 },
            { "X7  vnni without avx2",      64,   64 },
            { "X8  bmi2 no fma",             8,    9 },
            { "X9  bf16 missing",            0,  512 },
            { "X10 sse42+avx",               4 + 16, 4 + 16 },
        };
        for (auto & k : c) {
            expect(k.id, "score", feat_x86_score(k.bits, k.flags),
                   nat_x86_score(k.bits, k.flags));
        }
    }

    // ---- kleidiai mask: 8 cases ------------------------------------------
    {
        struct { const char * id; int64_t bits, cnt, cores; } c[] = {
            { "K0  empty",                 0,   0,  0 },
            { "K1  dotproc+i8mm+fp16+vl",  19,  32,  0 },
            { "K2  vl 16 rejected",        19,  16,  0 },
            { "K3  sme no budget",         32,   0,  0 },
            { "K4  sme with budget",       32,   0,  4 },
            { "K5  sme2 with budget",      96,   0,  2 },
            { "K6  sme2 negative budget",  96,   0, -1 },
            { "K7  everything",           119,  32,  8 },
            { "K8  vl 64 rejected",        19,  64,  0 },
        };
        for (auto & k : c) {
            expect(k.id, "mask", feat_kleidiai_mask(k.bits, k.cnt, k.cores),
                   nat_kleidiai_mask(k.bits, k.cnt, k.cores));
        }
    }

    // ---- kleidiai name: 8 cases ------------------------------------------
    {
        const int64_t c[] = { 0, 16, 8, 4, 2, 1, 32, 33, 24 };
        char nbuf[32];
        for (int64_t mask : c) {
            char id[48];
            std::snprintf(id, sizeof(id), "N mask %lld", (long long) mask);
            std::memset(nbuf, 0, sizeof(nbuf));
            const int64_t n = feat_kleidiai_name(mask, sl_u8{(uint8_t *) nbuf, (int64_t) sizeof(nbuf)});
            const char * want = nat_kleidiai_name(mask);
            expect(id, "len", n, (int64_t) std::strlen(want));
            expect(id, "name", std::strcmp(nbuf, want) == 0, 1);
        }
    }

    std::printf("feat: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
