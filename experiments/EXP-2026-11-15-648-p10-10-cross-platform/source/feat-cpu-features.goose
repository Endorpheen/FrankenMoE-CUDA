// CPU feature detection and backend scoring, ported from the ggml platform
// half: ggml/src/ggml-feats.h and the two architecture score functions.
//
// Native sources (upstream/llama.cpp at the task base commit):
//   ggml/src/ggml-feats.h:95-140                  ggml_feats_get_arch64_runtime
//   ggml/src/ggml-cpu/arch/arm/cpu-feats.cpp:7-44 ggml_backend_cpu_aarch64_score
//   ggml/src/ggml-cpu/arch/x86/cpu-feats.cpp:263-325
//                                                 ggml_backend_cpu_x86_score
//   ggml/src/ggml-cpu/kleidiai/kleidiai.cpp:317-321  the feature mask
//   ggml/src/ggml-cpu/kleidiai/kleidiai.cpp:380-386  the SME family branch
//   ggml/src/ggml-cpu/kleidiai/kleidiai.cpp:75-90    cpu_feature_to_string
//
// Owned decisions:
//   - the Linux decode of hwcap/hwcap2 into the seven ARM64 feature flags,
//     including the FPHP+ASIMDHP conjunction for fp16 and the SVE vector-length
//     mask (a prctl failure leaves the length at zero);
//   - the Apple decode: a failed sysctl leaves the flag at zero, so the result
//     is the conjunction of "the call succeeded" and "the value was non-zero";
//   - the Windows decode: one IsProcessorFeaturePresent id per flag, and no
//     vector length on that platform;
//   - the two score functions: the compile-flag gate (a missing required flag
//     returns 0, which makes the dynamic loader reject the variant) and the
//     per-flag weight; the AVX512 block needs five CPUID bits at once;
//   - the kleidiai mask, where SVE needs the vector length to be exactly QK8_0
//     (32) and the SME family is only added when the caller has a non-zero
//     SME core count;
//   - the kleidiai feature name, whose branch order matters.
//
// Not owned: CPUID itself, getauxval/prctl/sysctl/IsProcessorFeaturePresent,
// the compile-time flag word, the SME core count probe and the kernel tables.
// Every function here is a decision over values the caller supplies, so the
// module has no environment dependency and no device is touched.
//
// Feature bit packing used by every export of this module (the same order as
// the fields of ggml_feats_arch64_runtime_t, minus sve_cnt which travels in its
// own output slot):
//   bit 0 dotprod, 1 fp16, 2 sve, 3 sve2, 4 i8mm, 5 sme, 6 sme2.

// ARM64 feature bits.
const F_DOTPROD: i64 = 1;
const F_FP16: i64 = 2;
const F_SVE: i64 = 4;
const F_SVE2: i64 = 8;
const F_I8MM: i64 = 16;
const F_SME: i64 = 32;
const F_SME2: i64 = 64;
const F_ARCH64_ALL: i64 = 127;

// HWCAP / HWCAP2 bit positions (ggml-feats.h:17-45).
const F_HWCAP_FPHP: i64 = 9;
const F_HWCAP_ASIMDHP: i64 = 10;
const F_HWCAP_ASIMDDP: i64 = 20;
const F_HWCAP_SVE: i64 = 22;
const F_HWCAP2_SVE2: i64 = 1;
const F_HWCAP2_I8MM: i64 = 13;
const F_HWCAP2_SME: i64 = 23;
const F_HWCAP2_SME2: i64 = 37;
const F_VL_LEN_MASK: i64 = 65535;

// aarch64 score compile flags (GGML_USE_*, cpu-feats.cpp:14-32).
const F_USE_DOTPROD: i64 = 1;
const F_USE_FP16: i64 = 2;
const F_USE_SVE: i64 = 4;
const F_USE_MATMUL_INT8: i64 = 8;
const F_USE_SVE2: i64 = 16;
const F_USE_SME: i64 = 32;

// x86 feature bits used by the score (cpu-feats.cpp:18-88).
const F_X_FMA: i64 = 1;
const F_X_F16C: i64 = 2;
const F_X_SSE42: i64 = 4;
const F_X_BMI2: i64 = 8;
const F_X_AVX: i64 = 16;
const F_X_AVX2: i64 = 32;
const F_X_AVX_VNNI: i64 = 64;
const F_X_AVX512F: i64 = 128;
const F_X_AVX512CD: i64 = 256;
const F_X_AVX512VL: i64 = 512;
const F_X_AVX512DQ: i64 = 1024;
const F_X_AVX512BW: i64 = 2048;
const F_X_AVX512_VBMI: i64 = 4096;
const F_X_AVX512_BF16: i64 = 8192;
const F_X_AVX512_VNNI: i64 = 16384;
const F_X_AMX_INT8: i64 = 32768;
// The five bits the GGML_AVX512 block requires together.
const F_X_AVX512_REQ: i64 = 128 + 256 + 512 + 1024 + 2048;

// x86 score compile flags (GGML_*, cpu-feats.cpp:266-322).
const F_USE_FMA: i64 = 1;
const F_USE_F16C: i64 = 2;
const F_USE_SSE42: i64 = 4;
const F_USE_BMI2: i64 = 8;
const F_USE_AVX: i64 = 16;
const F_USE_AVX2: i64 = 32;
const F_USE_AVX_VNNI: i64 = 64;
const F_USE_AVX512: i64 = 128;
const F_USE_AVX512_VBMI: i64 = 256;
const F_USE_AVX512_BF16: i64 = 512;
const F_USE_AVX512_VNNI: i64 = 1024;
const F_USE_AMX_INT8: i64 = 2048;

// kleidiai cpu_feature mask (kernels.h:9-16).
const F_K_NONE: i64 = 0;
const F_K_DOTPROD: i64 = 1;
const F_K_I8MM: i64 = 2;
const F_K_SVE: i64 = 4;
const F_K_SME: i64 = 8;
const F_K_SME2: i64 = 16;
const F_K_FP16: i64 = 32;

// QK8_0, the only SVE vector length the kleidiai mask accepts.
const F_QK8_0: i64 = 32;

fn f_bit(v: i64, bit: i64) -> bool {
    return (v & (1 << bit)) != 0;
}

// Write the decoded record: out[0..6] the feature bits 0..6, out[7] sve_cnt.
fn f_emit(bits: i64, sve_cnt: i64, out: i64[:]) -> i64 {
    var i: i64 = 0;

    while i < 7 {
        if (bits & (1 << i)) != 0 {
            out[i] = 1;
        } else {
            out[i] = 0;
        }
        i = i + 1;
    }
    out[7] = sve_cnt;
    return 8;
}

fn f_put(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < s.len {
        dst[p] = s[i];
        p = p + 1;
        i = i + 1;
    }
    return p;
}

// ---- ggml_feats_get_arch64_runtime, Linux branch --------------------------

// sve_raw is the prctl(PR_SVE_GET_VL) return value: a negative value models the
// failed call, exactly as the native code leaves sve_cnt at zero.
fn f_arch64_linux(hwcap: i64, hwcap2: i64, sve_raw: i64, out: i64[:]) -> i64 {
    var bits: i64 = 0;
    var cnt: i64 = 0;

    if f_bit(hwcap, F_HWCAP_ASIMDDP) {
        bits = bits | F_DOTPROD;
    }
    if f_bit(hwcap, F_HWCAP_FPHP) && f_bit(hwcap, F_HWCAP_ASIMDHP) {
        bits = bits | F_FP16;
    }
    if f_bit(hwcap, F_HWCAP_SVE) {
        bits = bits | F_SVE;
    }
    if f_bit(hwcap2, F_HWCAP2_SVE2) {
        bits = bits | F_SVE2;
    }
    if f_bit(hwcap2, F_HWCAP2_I8MM) {
        bits = bits | F_I8MM;
    }
    if f_bit(hwcap2, F_HWCAP2_SME) {
        bits = bits | F_SME;
    }
    if f_bit(hwcap2, F_HWCAP2_SME2) {
        bits = bits | F_SME2;
    }
    if (bits & F_SVE) != 0 && sve_raw >= 0 {
        cnt = sve_raw & F_VL_LEN_MASK;
    }
    return f_emit(bits, cnt, out);
}

// ---- the same record on the other two platforms ---------------------------

// Apple: vals[i] is the sysctl value for feature i, ok[i] says whether the call
// returned 0. A failed call leaves the flag false, so the result is the
// conjunction. Apple never reports a vector length (ggml-feats.h:139).
fn f_arch64_apple(vals: i64, ok: i64, out: i64[:]) -> i64 {
    var key: i64 = 1;
    var bits: i64 = 0;

    while key <= F_SME2 {
        if (ok & key) != 0 && (vals & key) != 0 {
            bits = bits | key;
        }
        key = key << 1;
    }
    return f_emit(bits, 0, out);
}

// Windows: pf[i] is IsProcessorFeaturePresent for feature i. No vector length.
fn f_arch64_windows(pf: i64, out: i64[:]) -> i64 {
    return f_emit(pf & F_ARCH64_ALL, 0, out);
}

// ---- ggml_backend_cpu_aarch64_score --------------------------------------

// 0 means "this variant cannot run here"; otherwise the score is 1 plus one bit
// per satisfied compile flag, which is how the loader picks the best variant.
fn f_arch64_score(bits: i64, flags: i64) -> i64 {
    var score: i64 = 1;

    if (flags & F_USE_DOTPROD) != 0 {
        if (bits & F_DOTPROD) == 0 {
            return 0;
        }
        score = score + 2;
    }
    if (flags & F_USE_FP16) != 0 {
        if (bits & F_FP16) == 0 {
            return 0;
        }
        score = score + 4;
    }
    if (flags & F_USE_SVE) != 0 {
        if (bits & F_SVE) == 0 {
            return 0;
        }
        score = score + 8;
    }
    if (flags & F_USE_MATMUL_INT8) != 0 {
        if (bits & F_I8MM) == 0 {
            return 0;
        }
        score = score + 16;
    }
    if (flags & F_USE_SVE2) != 0 {
        if (bits & F_SVE2) == 0 {
            return 0;
        }
        score = score + 32;
    }
    if (flags & F_USE_SME) != 0 {
        if (bits & F_SME) == 0 {
            return 0;
        }
        score = score + 64;
    }
    return score;
}

// ---- ggml_backend_cpu_x86_score ------------------------------------------

fn f_x86_score(bits: i64, flags: i64) -> i64 {
    var score: i64 = 1;

    if (flags & F_USE_FMA) != 0 {
        if (bits & F_X_FMA) == 0 {
            return 0;
        }
        score = score + 1;
    }
    if (flags & F_USE_F16C) != 0 {
        if (bits & F_X_F16C) == 0 {
            return 0;
        }
        score = score + 2;
    }
    if (flags & F_USE_SSE42) != 0 {
        if (bits & F_X_SSE42) == 0 {
            return 0;
        }
        score = score + 4;
    }
    if (flags & F_USE_BMI2) != 0 {
        if (bits & F_X_BMI2) == 0 {
            return 0;
        }
        score = score + 8;
    }
    if (flags & F_USE_AVX) != 0 {
        if (bits & F_X_AVX) == 0 {
            return 0;
        }
        score = score + 16;
    }
    if (flags & F_USE_AVX2) != 0 {
        if (bits & F_X_AVX2) == 0 {
            return 0;
        }
        score = score + 32;
    }
    if (flags & F_USE_AVX_VNNI) != 0 {
        if (bits & F_X_AVX_VNNI) == 0 {
            return 0;
        }
        score = score + 64;
    }
    if (flags & F_USE_AVX512) != 0 {
        if (bits & F_X_AVX512_REQ) != F_X_AVX512_REQ {
            return 0;
        }
        score = score + 128;
    }
    if (flags & F_USE_AVX512_VBMI) != 0 {
        if (bits & F_X_AVX512_VBMI) == 0 {
            return 0;
        }
        score = score + 256;
    }
    if (flags & F_USE_AVX512_BF16) != 0 {
        if (bits & F_X_AVX512_BF16) == 0 {
            return 0;
        }
        score = score + 512;
    }
    if (flags & F_USE_AVX512_VNNI) != 0 {
        if (bits & F_X_AVX512_VNNI) == 0 {
            return 0;
        }
        score = score + 1024;
    }
    if (flags & F_USE_AMX_INT8) != 0 {
        if (bits & F_X_AMX_INT8) == 0 {
            return 0;
        }
        score = score + 2048;
    }
    return score;
}

// ---- kleidiai feature mask and name --------------------------------------

// The mask the kleidiai context starts from (kleidiai.cpp:317-321) plus the SME
// family (kleidiai.cpp:380-386). sme_cores <= 0 means "no SME thread budget",
// which keeps the family out even when the CPU reports it.
fn f_kleidiai_mask(bits: i64, sve_cnt: i64, sme_cores: i64) -> i64 {
    var mask: i64 = F_K_NONE;

    if (bits & F_DOTPROD) != 0 {
        mask = mask | F_K_DOTPROD;
    }
    if (bits & F_I8MM) != 0 {
        mask = mask | F_K_I8MM;
    }
    if (bits & F_FP16) != 0 {
        mask = mask | F_K_FP16;
    }
    if sve_cnt == F_QK8_0 {
        mask = mask | F_K_SVE;
    }
    if sme_cores > 0 && (bits & F_SME) != 0 {
        mask = mask | F_K_SME;
        if (bits & F_SME2) != 0 {
            mask = mask | F_K_SME2;
        }
    }
    return mask;
}

// cpu_feature_to_string: NONE first, then SME2, SME, SVE, I8MM, DOTPROD, and
// UNKNOWN for a mask of only the weaker flags (FP16 alone, for instance).
fn f_kleidiai_name(mask: i64, out: u8[:]) -> i64 {
    if mask == F_K_NONE {
        return f_put(out, 0, "NONE");
    }
    if (mask & F_K_SME2) == F_K_SME2 {
        return f_put(out, 0, "SME2");
    }
    if (mask & F_K_SME) == F_K_SME {
        return f_put(out, 0, "SME");
    }
    if (mask & F_K_SVE) == F_K_SVE {
        return f_put(out, 0, "SVE");
    }
    if (mask & F_K_I8MM) == F_K_I8MM {
        return f_put(out, 0, "I8MM");
    }
    if (mask & F_K_DOTPROD) == F_K_DOTPROD {
        return f_put(out, 0, "DOTPROD");
    }
    return f_put(out, 0, "UNKNOWN");
}

// ---- exports --------------------------------------------------------------

export fn feat_arch64_linux(hwcap: i64, hwcap2: i64, sve_raw: i64, out: i64[:]) -> i64 {
    return f_arch64_linux(hwcap, hwcap2, sve_raw, out);
}

export fn feat_arch64_apple(vals: i64, ok: i64, out: i64[:]) -> i64 {
    return f_arch64_apple(vals, ok, out);
}

export fn feat_arch64_windows(pf: i64, out: i64[:]) -> i64 {
    return f_arch64_windows(pf, out);
}

export fn feat_arch64_score(bits: i64, flags: i64) -> i64 {
    return f_arch64_score(bits, flags);
}

export fn feat_x86_score(bits: i64, flags: i64) -> i64 {
    return f_x86_score(bits, flags);
}

export fn feat_kleidiai_mask(bits: i64, sve_cnt: i64, sme_cores: i64) -> i64 {
    return f_kleidiai_mask(bits, sve_cnt, sme_cores);
}

export fn feat_kleidiai_name(mask: i64, out: u8[:]) -> i64 {
    return f_kleidiai_name(mask, out);
}
