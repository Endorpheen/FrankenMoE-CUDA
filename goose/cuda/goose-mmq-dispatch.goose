// Dense-path dispatch policy in Goose (APX: P3-10).
//
// From ggml/src/ggml-cuda/mmq.cu:355 ggml_cuda_should_use_mmq. This decides whether a
// quantized matmul takes the MMQ kernel or falls back to cuBLAS / mul_mat_vec_q. The
// decision covers 39.5% of Apex kernel time, and it is pure host-side integer logic:
// the emitter's address typing does not block it.
//
// Device facts (cc, shared-memory budget, compiled arch, MMA availability, the
// compile-time GGML_CUDA_FORCE_* switches) arrive as arguments. Goose owns the decision
// tree; the host owns what the device is.
extern "fg203d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// Compute capabilities, from common.cuh:50-108.

fn cc_pascal() -> i64 {
    return 600 as i64;
}

fn cc_dp4a() -> i64 {
    return 610 as i64;
}

fn cc_turing() -> i64 {
    return 750 as i64;
}

fn cc_mthreads_off() -> i64 {
    return 1048576 as i64;
}

fn cc_amd_off() -> i64 {
    return 16777216 as i64;
}

fn cc_vega() -> i64 {
    return 16777216 as i64 + 2304 as i64;
}

fn cc_gfx909() -> i64 {
    return 16777216 as i64 + 2313 as i64;
}

fn cc_gfx90c() -> i64 {
    return 16777216 as i64 + 2316 as i64;
}

fn cc_cdna1() -> i64 {
    return 16777216 as i64 + 2312 as i64;
}

fn cc_cdna3() -> i64 {
    return 16777216 as i64 + 2370 as i64;
}

fn cc_cdna4() -> i64 {
    return 16777216 as i64 + 2384 as i64;
}

fn cc_rdna1() -> i64 {
    return 16777216 as i64 + 4112 as i64;
}

fn cc_rdna3() -> i64 {
    return 16777216 as i64 + 4352 as i64;
}

fn cc_rdna3_5() -> i64 {
    return 16777216 as i64 + 4432 as i64;
}

fn cc_rdna4() -> i64 {
    return 16777216 as i64 + 4608 as i64;
}

fn is_nvidia(cc: i64) -> i64 {
    if cc < cc_mthreads_off() {
        return 1 as i64;
    }
    return 0 as i64;
}

fn is_gcn_apu(cc: i64) -> i64 {
    if cc == cc_gfx909() {
        return 1 as i64;
    }

    if cc == cc_gfx90c() {
        return 1 as i64;
    }
    return 0 as i64;
}

fn is_cdna(cc: i64) -> i64 {
    if is_gcn_apu(cc) != 0 as i64 {
        return 0 as i64;
    }

    if cc >= cc_cdna1() && cc < cc_rdna1() {
        return 1 as i64;
    }
    return 0 as i64;
}

fn is_cdna3(cc: i64) -> i64 {
    if is_cdna(cc) == 0 as i64 {
        return 0 as i64;
    }

    if cc >= cc_cdna3() && cc < cc_cdna4() {
        return 1 as i64;
    }
    return 0 as i64;
}

fn is_rdna3_0(cc: i64) -> i64 {
    if cc >= cc_rdna3() && cc < cc_rdna3_5() {
        return 1 as i64;
    }
    return 0 as i64;
}

fn is_rdna3_5(cc: i64) -> i64 {
    if cc >= cc_rdna3_5() && cc < cc_rdna4() {
        return 1 as i64;
    }
    return 0 as i64;
}

fn is_rdna3(cc: i64) -> i64 {
    if is_rdna3_0(cc) != 0 as i64 {
        return 1 as i64;
    }

    if is_rdna3_5(cc) != 0 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}
// mmq_type_supported: the type switch at mmq.cu:363-402.
// Q1_0=41 Q2_0=42 Q4_0=2 Q4_1=3 Q5_0=6 Q5_1=7 Q8_0=8 Q2_K=10 Q3_K=11 Q4_K=12 Q5_K=13
// Q6_K=14 IQ1_S=19 IQ2_XXS=16 IQ2_XS=17 IQ2_S=22 IQ3_XXS=18 IQ3_S=21 IQ4_XS=23
// IQ4_NL=20 MXFP4=39 NVFP4=40

fn mmq_type_supported(t: i64) -> i64 {
    if t == 41 as i64 {
        return 1 as i64;
    }

    if t == 42 as i64 {
        return 1 as i64;
    }

    if t == 2 as i64 {
        return 1 as i64;
    }

    if t == 3 as i64 {
        return 1 as i64;
    }

    if t == 6 as i64 {
        return 1 as i64;
    }

    if t == 7 as i64 {
        return 1 as i64;
    }

    if t == 8 as i64 {
        return 1 as i64;
    }

    if t == 10 as i64 {
        return 1 as i64;
    }

    if t == 11 as i64 {
        return 1 as i64;
    }

    if t == 12 as i64 {
        return 1 as i64;
    }

    if t == 13 as i64 {
        return 1 as i64;
    }

    if t == 14 as i64 {
        return 1 as i64;
    }

    if t == 19 as i64 {
        return 1 as i64;
    }

    if t == 16 as i64 {
        return 1 as i64;
    }

    if t == 17 as i64 {
        return 1 as i64;
    }

    if t == 22 as i64 {
        return 1 as i64;
    }

    if t == 18 as i64 {
        return 1 as i64;
    }

    if t == 21 as i64 {
        return 1 as i64;
    }

    if t == 23 as i64 {
        return 1 as i64;
    }

    if t == 20 as i64 {
        return 1 as i64;
    }

    if t == 39 as i64 {
        return 1 as i64;
    }

    if t == 40 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}
// should_use_mmq: mmq.cu:355-481.
//   smpbo        per-block shared memory budget in bytes
//   highest_arch ggml_cuda_highest_compiled_arch(cc)
//   turing_mma   turing_mma_available(cc)
//   fp16_mma_hw  fp16_mma_hardware_available(cc)
//   mfma, wmma   amd_mfma_available(cc), amd_wmma_available(cc)
//   force_cublas, force_mmq  GGML_CUDA_FORCE_CUBLAS, GGML_CUDA_FORCE_MMQ

fn should_use_mmq(
    t: i64,
    cc: i64,
    ne11: i64,
    n_experts: i64,
    smpbo: i64,
    highest_arch: i64,
    turing_mma: i64,
    fp16_mma_hw: i64,
    mfma: i64,
    wmma: i64,
    force_cublas: i64,
    force_mmq: i64) -> i64 {
    ensure(ne11 >= 0 as i64 && n_experts >= 0 as i64 && smpbo >= 0 as i64, 10);

    if force_cublas != 0 as i64 {
        return 0 as i64;
    }

    if mmq_type_supported(t) == 0 as i64 {
        return 0 as i64;
    }

    if smpbo < 49152 as i64 {
        return 0 as i64;
    }

    if turing_mma != 0 as i64 {
        return 1 as i64;
    }

    if highest_arch < cc_dp4a() {
        if cc >= cc_pascal() && n_experts > 0 as i64 {
            return 1 as i64;
        }
        return 0 as i64;
    }

    if force_mmq != 0 as i64 {
        return 1 as i64;
    }

    if is_nvidia(cc) != 0 as i64 {
        if fp16_mma_hw == 0 as i64 {
            return 1 as i64;
        }

        if ne11 < 64 as i64 {
            return 1 as i64;
        }
        return 0 as i64;
    }

    if mfma != 0 as i64 {
        if is_cdna3(cc) != 0 as i64 {
            return 1 as i64;
        }

        if n_experts > 64 as i64 {
            return 1 as i64;
        }

        if ne11 <= 128 as i64 {
            return 1 as i64;
        }

        if t == 2 as i64 {
            return 1 as i64;
        }

        if t == 3 as i64 {
            return 1 as i64;
        }

        if t == 6 as i64 {
            return 1 as i64;
        }

        if t == 7 as i64 {
            return 1 as i64;
        }

        if ne11 <= 256 as i64 {
            if t == 12 as i64 {
                return 1 as i64;
            }

            if t == 13 as i64 {
                return 1 as i64;
            }
        }
        return 0 as i64;
    }

    if wmma != 0 as i64 {
        if is_rdna3(cc) != 0 as i64 {
            if n_experts >= 64 as i64 {
                return 1 as i64;
            }

            if t == 10 as i64 {
                if ne11 <= 128 as i64 {
                    return 1 as i64;
                }
                return 0 as i64;
            }

            if t == 14 as i64 {
                if is_rdna3_0(cc) != 0 as i64 {
                    if ne11 <= 128 as i64 {
                        return 1 as i64;
                    }
                    return 0 as i64;
                }

                if ne11 <= 256 as i64 {
                    return 1 as i64;
                }
                return 0 as i64;
            }

            if t == 17 as i64 {
                if is_rdna3_5(cc) != 0 as i64 {
                    return 1 as i64;
                }

                if ne11 <= 128 as i64 {
                    return 1 as i64;
                }
                return 0 as i64;
            }

            if t == 22 as i64 {
                if is_rdna3_5(cc) != 0 as i64 {
                    return 1 as i64;
                }

                if ne11 <= 128 as i64 {
                    return 1 as i64;
                }
                return 0 as i64;
            }
            return 1 as i64;
        }
        return 1 as i64;
    }
    // mmq.cu:474-478: VEGA and the GCN APUs keep MMQ only for MoE.

    if cc == cc_vega() {
        if n_experts > 0 as i64 {
            return 1 as i64;
        }
        return 0 as i64;
    }

    if is_gcn_apu(cc) != 0 as i64 {
        if n_experts > 0 as i64 {
            return 1 as i64;
        }
        return 0 as i64;
    }

    if is_cdna(cc) == 0 as i64 {
        return 1 as i64;
    }

    if ne11 < 64 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}

fn main() {
    mmq_type_supported(12 as i64);
    should_use_mmq(
        12 as i64,
        890 as i64,
        1 as i64,
        0 as i64,
        101376 as i64,
        1200 as i64,
        1 as i64,
        1 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64);
}

