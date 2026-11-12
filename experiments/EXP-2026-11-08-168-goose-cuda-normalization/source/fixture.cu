// ---------------------------------------------------------------------------
// EXP168 CUDA normalization correctness fixture.
//
// Proves the candidate Goose normalization implementation against:
//   1. the native oracle (fg168_native_* prefixed upstream kernels) with
//      bit-exact comparisons on native-compatible ordinary finite patterns;
//   2. an independent strided reference (arithmetic transcribed from
//      inputs/native-norm.cu with only the addressing corrected - see the
//      provenance label above the kernels) wherever the native oracle is
//      layout-bound (padded outer destination strides, strided backward
//      forward source).
// Also verifies guard/padding canaries, the host/device witness for the
// default, rollback (GGML_GOOSE_CUDA_NORM=0) and legacy (GGML_GOOSE_RMS=0)
// configurations, broadcast and swapped fused operands, fused intermediates
// that must not be materialized, and the documented NaN policy.
// No timing, no tolerances. See tests/cases.json for the case map,
// exit protocol, witness expectations and known gaps.
// ---------------------------------------------------------------------------

#include "norm.cuh"
#include "fixture-api.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" uint64_t fg168_cuda_host_seen();
extern "C" int      fg168_cuda_device_read(uint64_t *);

static size_t checks = 0, cases = 0, native_cases = 0, reference_cases = 0,
              nan_policy_cases = 0, native_info_divergent = 0, values = 0;

static void ck(cudaError_t e) {
    if (e != cudaSuccess) {
        fprintf(stderr, "CUDA error: %s\n", cudaGetErrorString(e));
        std::exit(2);
    }
}

static void req(bool b, const char * what) {
    ++checks;
    if (!b) {
        fprintf(stderr, "FAIL %s\n", what);
        std::exit(3);
    }
}

// ---------------------------------------------------------------------------
// Device buffer with 64-byte 0xA5 canary guards on both ends. Bytes between
// logical elements (stride padding) stay 0xA5 unless a kernel writes them.
// ---------------------------------------------------------------------------
struct Buffer {
    static constexpr size_t guard = 64;
    unsigned char * raw;
    size_t          n;
    explicit Buffer(size_t bytes) : raw(nullptr), n(bytes + 2 * guard) {
        ck(cudaMalloc(&raw, n));
        ck(cudaMemset(raw, 0xA5, n));
        ck(cudaStreamSynchronize(nullptr));  // Order fixture initialization before nonblocking compute streams.
    }
    ~Buffer() { ck(cudaFree(raw)); }
    Buffer(const Buffer &) = delete;
    Buffer & operator=(const Buffer &) = delete;
    void * data() const { return raw + guard; }
    size_t bytes() const { return n - 2 * guard; }
};

// ggml_nbytes() equivalent for F32 tensors (blck_size == 1):
// 4 + sum_i (ne[i]-1)*nb[i].
static size_t tensor_span(const ggml_tensor & t) {
    size_t s = 4;
    for (int i = 0; i < 4; ++i) {
        s += (size_t)(t.ne[i] - 1) * t.nb[i];
    }
    return s;
}

// ne plus whole-float outer stride padding on nb[1..3]; nb[0] stays 4.
static void set_shape(ggml_tensor & t, const int64_t ne[4], const int64_t pad[3]) {
    std::memset(&t, 0, sizeof t);
    t.type = GGML_TYPE_F32;
    for (int i = 0; i < 4; ++i) {
        t.ne[i] = ne[i];
    }
    t.nb[0] = 4;
    t.nb[1] = (size_t)(ne[0] + pad[0]) * 4;
    t.nb[2] = t.nb[1] * (size_t)ne[1] + (size_t)pad[1] * 4;
    t.nb[3] = t.nb[2] * (size_t)ne[2] + (size_t)pad[2] * 4;
}

static float sample_value(size_t i, int pattern) {
    switch (pattern) {
        case 1: {
            // finite special values: +-0, +-1, +-0.5, +-2, min normal,
            // min denormal, ~1e-10, +-1e6, 0.1 (all exactly representable)
            static const uint32_t fin[16] = {
                0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u,
                0x3f000000u, 0xbf000000u, 0x40000000u, 0xc0000000u,
                0x00800000u, 0x00000001u, 0x2edbe6ffu, 0xaedbe6ffu,
                0x49742400u, 0xc9742400u, 0x3dcccccdu, 0xbdcccccdu};
            float v;
            std::memcpy(&v, &fin[i % 16], 4);
            return v;
        }
        case 4: {
            // finite overflow patterns: squares overflow to +inf so the
            // normalized output is deterministically +-0 (modes 2 and 7 only)
            static const uint32_t big[8] = {0x7f7fffffu, 0xff7fffffu, 0x6f000000u, 0xef000000u,
                                            0x3f800000u, 0xbf800000u, 0x3f000000u, 0xbf000000u};
            float v;
            std::memcpy(&v, &big[i % 8], 4);
            return v;
        }
        default:
            // ordinary deterministic values, roughly +-6.1
            return float((int)((i * 31337) % 50003) - 25001) / 4096.0f;
    }
}

static void fill_tensor(Buffer & b, const ggml_tensor & t, int pattern, int seed, size_t mis) {
    std::vector<unsigned char> host(b.bytes(), 0xA5);
    unsigned char * base = host.data() + mis;
    int64_t total = 1;
    for (int i = 0; i < 4; ++i) {
        total *= t.ne[i];
    }
    size_t at = 0;
    for (int64_t i3 = 0; i3 < t.ne[3]; ++i3)
        for (int64_t i2 = 0; i2 < t.ne[2]; ++i2)
            for (int64_t i1 = 0; i1 < t.ne[1]; ++i1)
                for (int64_t i0 = 0; i0 < t.ne[0]; ++i0) {
                    const size_t off = (size_t)i0 * 4 + (size_t)i1 * t.nb[1] + (size_t)i2 * t.nb[2] + (size_t)i3 * t.nb[3];
                    float v;
                    if (pattern == 2 && (int64_t)at * 2 < total) {
                        v = 0.0f;  // first half of the flat logical range is zero
                    } else if (pattern == 3 && i1 == 1) {
                        const uint32_t q = 0x7fc00000u;  // quiet NaN row
                        std::memcpy(&v, &q, 4);
                    } else {
                        v = sample_value(at + (size_t)seed * 11, pattern);
                    }
                    ++at;
                    std::memcpy(base + off, &v, 4);
                }
    ck(cudaMemcpy(b.data(), host.data(), b.bytes(), cudaMemcpyHostToDevice));
    ck(cudaStreamSynchronize(nullptr));  // Pageable H2D staging must finish before compute on another stream.
}

static std::vector<unsigned char> download(const Buffer & b) {
    std::vector<unsigned char> v(b.n);
    ck(cudaMemcpy(v.data(), b.raw, b.n, cudaMemcpyDeviceToHost));
    return v;
}

static void check_guards(const std::vector<unsigned char> & v, bool padding_too, const ggml_tensor * logical, size_t mis) {
    bool ok = true;
    for (size_t i = 0; i < Buffer::guard; ++i) {
        if (v[i] != 0xA5 || v[v.size() - 1 - i] != 0xA5) {
            ok = false;
        }
    }
    req(ok, "guard canaries intact");
    if (padding_too && logical != nullptr) {
        std::vector<unsigned char> covered(v.size() - 2 * Buffer::guard, 0);
        for (int64_t i3 = 0; i3 < logical->ne[3]; ++i3)
            for (int64_t i2 = 0; i2 < logical->ne[2]; ++i2)
                for (int64_t i1 = 0; i1 < logical->ne[1]; ++i1)
                    for (int64_t i0 = 0; i0 < logical->ne[0]; ++i0) {
                        const size_t off = mis + (size_t)i0 * 4 + (size_t)i1 * logical->nb[1] +
                                           (size_t)i2 * logical->nb[2] + (size_t)i3 * logical->nb[3];
                        for (int k = 0; k < 4; ++k) {
                            covered[off + k] = 1;
                        }
                    }
        ok = true;
        for (size_t i = 0; i < covered.size(); ++i) {
            if (!covered[i] && v[Buffer::guard + i] != 0xA5) {
                ok = false;
            }
        }
        req(ok, "padding canaries intact");
    }
}

static bool bit_eq(float a, float b) {
    return std::memcmp(&a, &b, 4) == 0;
}

static float logical_at(const std::vector<unsigned char> & v, const ggml_tensor & t,
                        int64_t i0, int64_t i1, int64_t i2, int64_t i3) {
    const size_t off = Buffer::guard + (size_t)i0 * 4 + (size_t)i1 * t.nb[1] +
                       (size_t)i2 * t.nb[2] + (size_t)i3 * t.nb[3];
    float f;
    std::memcpy(&f, v.data() + off, 4);
    return f;
}

static float packed_at(const std::vector<unsigned char> & v, const int64_t ne[4],
                       int64_t i0, int64_t i1, int64_t i2, int64_t i3) {
    const size_t off = Buffer::guard + ((((size_t)i3 * (size_t)ne[2] + (size_t)i2) * (size_t)ne[1] +
                                          (size_t)i1) * (size_t)ne[0] + (size_t)i0) * 4;
    float f;
    std::memcpy(&f, v.data() + off, 4);
    return f;
}

// Compares candidate logical (strided) elements against native packed output.
// hard=true fails on the first mismatch; hard=false records one INFO line.
static bool compare_l_p(const char * label, const std::vector<unsigned char> & cand, const ggml_tensor & ct,
                        const std::vector<unsigned char> & nat, const int64_t ne[4], bool skip_row1, bool hard) {
    bool ok = true, reported = false;
    for (int64_t i3 = 0; i3 < ne[3]; ++i3)
        for (int64_t i2 = 0; i2 < ne[2]; ++i2)
            for (int64_t i1 = 0; i1 < ne[1]; ++i1) {
                if (skip_row1 && i1 == 1) continue;
                for (int64_t i0 = 0; i0 < ne[0]; ++i0) {
                    const float a = logical_at(cand, ct, i0, i1, i2, i3);
                    const float b = packed_at(nat, ne, i0, i1, i2, i3);
                    if (!bit_eq(a, b)) {
                        ok = false;
                        uint32_t ba, bb;
                        std::memcpy(&ba, &a, 4);
                        std::memcpy(&bb, &b, 4);
                        if (hard) {
                            fprintf(stderr, "MISMATCH %s at [%lld,%lld,%lld,%lld] got %08x want %08x (candidate vs native)\n",
                                    label, (long long)i0, (long long)i1, (long long)i2, (long long)i3,
                                    (unsigned)ba, (unsigned)bb);
                            req(false, "bit-exact candidate vs native");
                        } else if (!reported) {
                            reported = true;
                            fprintf(stderr, "INFO native divergence %s first at [%lld,%lld,%lld,%lld] got %08x want %08x\n",
                                    label, (long long)i0, (long long)i1, (long long)i2, (long long)i3,
                                    (unsigned)ba, (unsigned)bb);
                        }
                    }
                }
            }
    return ok;
}

// Compares candidate against the independent reference; both use the same
// strided destination specification.
static bool compare_l_l(const char * label, const std::vector<unsigned char> & cand, const ggml_tensor & ct,
                        const std::vector<unsigned char> & ref, const ggml_tensor & rt, bool skip_row1) {
    bool ok = true, reported = false;
    for (int64_t i3 = 0; i3 < ct.ne[3]; ++i3)
        for (int64_t i2 = 0; i2 < ct.ne[2]; ++i2)
            for (int64_t i1 = 0; i1 < ct.ne[1]; ++i1) {
                if (skip_row1 && i1 == 1) continue;
                for (int64_t i0 = 0; i0 < ct.ne[0]; ++i0) {
                    const float a = logical_at(cand, ct, i0, i1, i2, i3);
                    const float b = logical_at(ref, rt, i0, i1, i2, i3);
                    if (!bit_eq(a, b)) {
                        ok = false;
                        uint32_t ba, bb;
                        std::memcpy(&ba, &a, 4);
                        std::memcpy(&bb, &b, 4);
                        if (!reported) {
                            reported = true;
                            fprintf(stderr, "MISMATCH %s at [%lld,%lld,%lld,%lld] got %08x want %08x (candidate vs reference)\n",
                                    label, (long long)i0, (long long)i1, (long long)i2, (long long)i3,
                                    (unsigned)ba, (unsigned)bb);
                        }
                    }
                }
            }
    req(ok, "bit-exact candidate vs independent reference");
    return ok;
}

// ---------------------------------------------------------------------------
// Contract launch-policy mirror (inputs/CONTRACT.md launch policy and
// inputs/host.goose). Used only to configure the independent reference
// kernels and to document the expected scalar/vector path.
// ---------------------------------------------------------------------------
struct Cfg {
    int bs = 32;
    int vpt = 1;
    bool vec = false;
    unsigned gx = 1, gy = 1, gz = 1;
    size_t shmem = 0;
};

static size_t misalign_or(const ggml_tensor & t) {
    return ((uintptr_t)t.data % 16) | (t.nb[1] % 16) | (t.nb[2] % 16) | (t.nb[3] % 16);
}

static Cfg mirror_cfg(int mode, const ggml_tensor & X, const ggml_tensor & D,
                      const ggml_tensor * M, const ggml_tensor * A, int groups) {
    const int64_t n = X.ne[0], rows = X.ne[1], ch = X.ne[2], smp = X.ne[3];
    Cfg c;
    if (mode == 1) {
        const int64_t size = n * rows * ((ch + groups - 1) / groups);
        c.bs = size < 1024 ? 32 : 1024;
        c.shmem = c.bs > 32 ? 256 : 0;
        c.gx = (unsigned)(groups * smp);
        return c;
    }
    if (mode == 6) {
        c.bs = n < 1024 ? 32 : 1024;
        c.shmem = c.bs > 32 ? 256 : 0;
        c.gx = (unsigned)(rows * ch * smp);
        return c;
    }
    c.gx = (unsigned)rows;
    c.gy = (unsigned)(ch < 65535 ? ch : 65535);
    c.gz = (unsigned)(smp < 65535 ? smp : 65535);
    if (mode == 0 || mode == 7) {
        c.bs = n < 1024 ? 32 : 1024;
        c.shmem = c.bs > 32 ? (mode == 0 ? 256 : 128) : 0;
        return c;
    }
    // modes 2..5: scalar/vector selection with the exact contract conditions
    bool vec = (n % 4 == 0) && misalign_or(X) == 0 && misalign_or(D) == 0;
    if (M != nullptr) {
        vec = vec && M->ne[0] == n && misalign_or(*M) == 0;
    }
    if (A != nullptr) {
        vec = vec && A->ne[0] == n && misalign_or(*A) == 0;
    }
    c.vec = vec;
    if (vec) {
        const int64_t n4 = n / 4;
        if (n4 <= 32) {
            c.bs = 32;
        } else if (n4 <= 128) {
            c.bs = 128;
        } else if (n4 <= 256) {
            c.bs = 256;
        } else if (n4 <= 512) {
            c.bs = 512;
        } else {
            c.bs = 1024;
            if (n4 > 1024 && n4 <= 4096) {
                c.vpt = 4;
            }
            if (n4 > 4096) {
                c.vec = false;  // scalar bs1024, matches host.goose semantics
            }
        }
    } else {
        c.bs = n < 1024 ? 256 : 1024;
    }
    c.shmem = c.bs > 32 ? 128 : 0;
    return c;
}

// ---------------------------------------------------------------------------
// Independent reference kernels ("strided oracle").
//
// PROVENANCE: the arithmetic below is transcribed from inputs/native-norm.cu
// (the upstream ggml CUDA normalization kernels, which also serve as the
// native oracle of this experiment) with exactly one deliberate change: the
// destination - and, for rms_norm_back, the forward source - is addressed
// through explicit element strides instead of the packed contiguous layout
// the upstream kernels assume. No candidate (Goose) code is called or linked
// here. These kernels prove the corrected strided output/source semantics
// required by inputs/CONTRACT.md; on contiguous layouts they are expected to
// be bit-identical to the native oracle. Launch configurations come from
// mirror_cfg() so a candidate that dispatches differently fails the bitwise
// comparison. The fixture must be compiled with the same floating-point
// contraction flags as the native oracle so identical expressions round
// identically.
// ---------------------------------------------------------------------------

static __device__ float ref_warp_sum(float v) {
#pragma unroll
    for (int off = 16; off > 0; off >>= 1) {
        v += __shfl_xor_sync(0xffffffffu, v, off, 32);
    }
    return v;
}

static __device__ float2 ref_warp_sum2(float2 a) {
#pragma unroll
    for (int off = 16; off > 0; off >>= 1) {
        a.x += __shfl_xor_sync(0xffffffffu, a.x, off, 32);
        a.y += __shfl_xor_sync(0xffffffffu, a.y, off, 32);
    }
    return a;
}

static __device__ float ref_block_sum(float v, float * shared) {
    const int bs = (int)blockDim.x;
    v = ref_warp_sum(v);
    if (bs > 32) {
        const int warp = threadIdx.x / 32;
        const int lane = threadIdx.x % 32;
        if (lane == 0) {
            shared[warp] = v;
        }
        __syncthreads();
        v = 0.0f;
        if (lane < bs / 32) {
            v = shared[lane];
        }
        v = ref_warp_sum(v);
    }
    return v;
}

static __device__ float2 ref_block_sum2(float2 a, float2 * shared) {
    const int bs = (int)blockDim.x;
    a = ref_warp_sum2(a);
    if (bs > 32) {
        const int warp = threadIdx.x / 32;
        const int lane = threadIdx.x % 32;
        if (lane == 0) {
            shared[warp].x = a.x;
            shared[warp].y = a.y;
        }
        __syncthreads();
        a = make_float2(0.0f, 0.0f);
        if (lane < bs / 32) {
            a = shared[lane];
        }
        a = ref_warp_sum2(a);
    }
    return a;
}

static __global__ void ref_norm_f32(const float * x, float * dst, const int ncols, const int nchannels,
                                    const int nsamples, const int64_t sr, const int64_t sc, const int64_t ss,
                                    const int64_t dr, const int64_t dc, const int64_t dsm, const float eps) {
    const int nrows = gridDim.x;
    const int row   = blockIdx.x;
    const int tid   = threadIdx.x;
    extern __shared__ float2 ref_s2[];
    for (int sample = blockIdx.z; sample < nsamples; sample += gridDim.z) {
        for (int channel = blockIdx.y; channel < nchannels; channel += gridDim.y) {
            const float * xc   = x + sample * ss + channel * sc + row * sr;
            float *       dstc = dst + sample * dsm + channel * dc + row * dr;  // strided (corrected)
            float2 mv = make_float2(0.0f, 0.0f);
            for (int col = tid; col < ncols; col += blockDim.x) {
                const float xi = xc[col];
                mv.x += xi;
                mv.y += xi * xi;
            }
            mv = ref_block_sum2(mv, ref_s2);
            const float mean    = mv.x / ncols;
            const float var     = mv.y / ncols - mean * mean;
            const float inv_std = rsqrtf(var + eps);
            for (int col = tid; col < ncols; col += blockDim.x) {
                dstc[col] = (xc[col] - mean) * inv_std;
            }
            if (blockDim.x > 32) {
                __syncthreads();
            }
        }
    }
}

static __global__ void ref_l2_f32(const float * x, float * dst, const int ncols, const int nchannels,
                                  const int nsamples, const int64_t sr, const int64_t sc, const int64_t ss,
                                  const int64_t dr, const int64_t dc, const int64_t dsm, const float eps) {
    const int nrows = gridDim.x;
    const int row   = blockIdx.x;
    const int tid   = threadIdx.x;
    extern __shared__ float ref_sl2[];
    for (int sample = blockIdx.z; sample < nsamples; sample += gridDim.z) {
        for (int channel = blockIdx.y; channel < nchannels; channel += gridDim.y) {
            const float * xc   = x + sample * ss + channel * sc + row * sr;
            float *       dstc = dst + sample * dsm + channel * dc + row * dr;  // strided (corrected)
            float tmp = 0.0f;
            for (int col = tid; col < ncols; col += blockDim.x) {
                const float xi = xc[col];
                tmp += xi * xi;
            }
            tmp = ref_block_sum(tmp, ref_sl2);
            const float scale = rsqrtf(fmaxf(tmp, eps * eps));
            for (int col = tid; col < ncols; col += blockDim.x) {
                dstc[col] = scale * xc[col];
            }
            if (blockDim.x > 32) {
                __syncthreads();
            }
        }
    }
}

template<bool do_multiply,bool do_add,bool do_scale>
static __global__ void ref_rms_f32(const float * x, float * dst,
                                   const int ncols, const int nchannels, const int nsamples,
                                   const int64_t sr, const int64_t sc, const int64_t ss, const float eps,
                                   const float * mul, const int64_t mr, const int64_t mc, const int64_t ms,
                                   const int mul_ncols, const int mul_nrows, const int mul_nchannels, const int mul_nsamples,
                                   const float * add, const int64_t ar, const int64_t ac, const int64_t as,
                                   const int add_ncols, const int add_nrows, const int add_nchannels, const int add_nsamples,
                                   const float scale_out,
                                   const bool do_vec, const int vpt,
                                   const int64_t dr, const int64_t dc, const int64_t dsm) {
    const int nrows = gridDim.x;
    const int row   = blockIdx.x;
    const int tid   = threadIdx.x;
    extern __shared__ float ref_s[];
    for (int sample = blockIdx.z; sample < nsamples; sample += gridDim.z) {
        for (int channel = blockIdx.y; channel < nchannels; channel += gridDim.y) {
            const float * xc   = x + sample * ss + channel * sc + row * sr;
            float *       dstc = dst + sample * dsm + channel * dc + row * dr;  // strided (corrected)
            const float * mulc = nullptr;
            if (do_multiply) {
                const int mul_row     = (int)((int64_t)row % mul_nrows);
                const int mul_channel = (int)((int64_t)channel % mul_nchannels);
                const int mul_sample  = (int)((int64_t)sample % mul_nsamples);
                mulc = mul + (int64_t)mul_sample * ms + (int64_t)mul_channel * mc + (int64_t)mul_row * mr;
            }
            const float * addc = nullptr;
            if (do_add) {
                const int add_row     = (int)((int64_t)row % add_nrows);
                const int add_channel = (int)((int64_t)channel % add_nchannels);
                const int add_sample  = (int)((int64_t)sample % add_nsamples);
                addc = add + (int64_t)add_sample * as + (int64_t)add_channel * ac + (int64_t)add_row * ar;
            }
            if (do_vec) {
                const int       ncols4 = ncols / 4;
                const float4 *  x4     = (const float4 *) xc;
                const float4 *  mul4   = (const float4 *) mulc;
                const float4 *  add4   = (const float4 *) addc;
                float4 *        dst4   = (float4 *) dstc;
                float4          xs[4]  = {};
                float           tmp    = 0.0f;
                for (int k = 0; k < vpt; ++k) {
                    const int i = tid + k * (int)blockDim.x;
                    if (i < ncols4) {
                        xs[k] = x4[i];
                        tmp += xs[k].x*xs[k].x + xs[k].y*xs[k].y + xs[k].z*xs[k].z + xs[k].w*xs[k].w;
                    }
                }
                tmp = ref_block_sum(tmp, ref_s);
                const float scale = rsqrtf(tmp / ncols + eps);
                for (int k = 0; k < vpt; ++k) {
                    const int i = tid + k * (int)blockDim.x;
                    if (i < ncols4) {
                        float4 r = make_float4(scale*xs[k].x, scale*xs[k].y, scale*xs[k].z, scale*xs[k].w);
                        if (do_multiply) {
                            const float4 m = mul4[i];
                            r.x *= m.x;
                            r.y *= m.y;
                            r.z *= m.z;
                            r.w *= m.w;
                        }
                        if (do_add) {
                            const float4 a = add4[i];
                            r.x += a.x;
                            r.y += a.y;
                            r.z += a.z;
                            r.w += a.w;
                        }
                        if (do_scale) {
                            r.x *= scale_out;
                            r.y *= scale_out;
                            r.z *= scale_out;
                            r.w *= scale_out;
                        }
                        dst4[i] = r;
                    }
                }
            } else {
                float tmp = 0.0f;
                for (int col = tid; col < ncols; col += blockDim.x) {
                    const float xi = xc[col];
                    tmp += xi * xi;
                }
                tmp = ref_block_sum(tmp, ref_s);
                const float mean  = tmp / ncols;
                const float scale = rsqrtf(mean + eps);
                for (int col = tid; col < ncols; col += blockDim.x) {
                    if (do_multiply && do_add) {
                        const int mul_col = (int)((int64_t)col % mul_ncols);
                        const int add_col = (int)((int64_t)col % add_ncols);
                        dstc[col]         = scale * xc[col] * mulc[mul_col] + addc[add_col];
                    } else if (do_multiply) {
                        const int mul_col = (int)((int64_t)col % mul_ncols);
                        dstc[col]         = scale * xc[col] * mulc[mul_col];
                    } else if (do_scale) {
                        dstc[col] = scale_out * (scale * xc[col]);
                    } else {
                        dstc[col] = scale * xc[col];
                    }
                }
            }
            if (blockDim.x > 32) {
                __syncthreads();
            }
        }
    }
}

// One block per flattened (row, channel, sample); grad is contiguous per the
// contract, the forward source and destination use explicit strides.
static __global__ void ref_rms_back_f32(const float * grad, const float * xf_in, float * dst,
                                        const int ncols, const int nrows, const int nchannels, const float eps,
                                        const int64_t xr, const int64_t xch, const int64_t xsmp,
                                        const int64_t dr, const int64_t dc, const int64_t dsm) {
    const int flat    = blockIdx.x;
    const int row     = flat % nrows;
    const int channel = (flat / nrows) % nchannels;
    const int sample  = flat / (nrows * nchannels);
    const int tid     = threadIdx.x;
    const float * gradp = grad + (((int64_t)sample * nchannels + channel) * (int64_t)nrows + row) * ncols;
    const float * xfp   = xf_in + (int64_t)sample * xsmp + (int64_t)channel * xch + (int64_t)row * xr;
    float *       dstp  = dst + (int64_t)sample * dsm + (int64_t)channel * dc + (int64_t)row * dr;
    float sum_xx = 0.0f;
    float sum_xg = 0.0f;
    for (int col = tid; col < ncols; col += blockDim.x) {
        const float xfi = xfp[col];
        sum_xx += xfi * xfi;
        sum_xg += xfi * gradp[col];
    }
    sum_xx = ref_warp_sum(sum_xx);
    sum_xg = ref_warp_sum(sum_xg);
    if (blockDim.x > 32) {
        __shared__ float s_sum_xx[32];
        __shared__ float s_sum_xg[32];
        const int warp_id = threadIdx.x / 32;
        const int lane_id = threadIdx.x % 32;
        if (lane_id == 0) {
            s_sum_xx[warp_id] = sum_xx;
            s_sum_xg[warp_id] = sum_xg;
        }
        __syncthreads();
        sum_xx = s_sum_xx[lane_id];
        sum_xx = ref_warp_sum(sum_xx);
        sum_xg = s_sum_xg[lane_id];
        sum_xg = ref_warp_sum(sum_xg);
    }
    const float mean_eps   = sum_xx / ncols + eps;
    const float sum_eps    = sum_xx + ncols * eps;
    const float scale_grad = rsqrtf(mean_eps);
    const float scale_x    = -scale_grad * sum_xg / sum_eps;
    for (int col = tid; col < ncols; col += blockDim.x) {
        dstp[col] = scale_grad * gradp[col] + scale_x * xfp[col];
    }
}


// ---------------------------------------------------------------------------
// Case table
// ---------------------------------------------------------------------------

enum NativeMode { NATIVE_EXACT = 0, NATIVE_SKIP = 1, NATIVE_INFO = 2 };

struct Case {
    std::string name;
    int mode = 0;
    int64_t ne[4]   = {1, 1, 1, 1};
    int64_t xpad[3] = {0, 0, 0};  // source outer stride padding, floats
    int64_t dpad[3] = {0, 0, 0};  // destination outer stride padding, floats
    int64_t opad[3] = {0, 0, 0};  // fused operand outer stride padding, floats
    int64_t mulne[4] = {1, 1, 1, 1};
    int64_t addne[4] = {1, 1, 1, 1};
    bool mul_swap = false;        // mul node references the rms node via src[1]
    bool add_swap = false;        // add node references the mul node via src[1]
    float eps = 1e-5f;
    float scale = 1.0f;
    int groups = 1;
    int pattern = 0;              // 0 ordinary, 1 finite specials, 2 zero prefix, 3 NaN row, 4 overflow
    int x_mis = 0;                // source base misalignment in bytes (0 or 4)
    int op_mis = 0;               // fused operand base misalignment in bytes
    bool x_stride0_ch = false;    // source channel stride zero (legal per contract: nonnegative)
    bool alias_inplace = false;   // modes 0/2/7: destination aliases the source buffer
    bool alias_operand = false;   // modes 3/5: fused operand aliases the fused output buffer
    bool node_canary = false;     // rms intermediate gets writable data that must stay untouched
    NativeMode native_mode = NATIVE_EXACT;
    bool ref_check = false;       // require the independent strided reference proof
    bool nan_policy = false;      // NaN presence policy instead of full bitwise proof
};

static void wire_unary(const Case & cs, const ggml_tensor & dspec, ggml_tensor & nd,
                       const ggml_tensor & xsrc, const ggml_tensor & gsrc, void * out) {
    std::memset(&nd, 0, sizeof nd);
    nd.type = GGML_TYPE_F32;
    for (int i = 0; i < 4; ++i) {
        nd.ne[i] = cs.ne[i];
        nd.nb[i] = dspec.nb[i];
    }
    nd.data = out;
    if (cs.mode == 6) {
        nd.src[0] = const_cast<ggml_tensor*>(&gsrc);   // grad
        nd.src[1] = const_cast<ggml_tensor*>(&xsrc);   // forward source
    } else {
        nd.src[0] = const_cast<ggml_tensor*>(&xsrc);
    }
    if (cs.mode == 1) {
        nd.op_params[0] = (int32_t)cs.groups;
        std::memcpy(&nd.op_params[1], &cs.eps, 4);
    } else {
        std::memcpy(&nd.op_params[0], &cs.eps, 4);
    }
}

static void wire_fused(const Case & cs, const ggml_tensor & dspec, ggml_tensor & nd, ggml_tensor & sec, ggml_tensor & ad,
                       const ggml_tensor & xsrc, const ggml_tensor & mssrc, const ggml_tensor & assrc,
                       void * rms_data, void * out) {
    std::memset(&nd, 0, sizeof nd);
    nd.type = GGML_TYPE_F32;
    for (int i = 0; i < 4; ++i) {
        nd.ne[i] = cs.ne[i];
        nd.nb[i] = xsrc.nb[i];  // mirrors the source; unused by both implementations
    }
    nd.data = rms_data;         // NULL or canary: intermediate must not be materialized
    nd.src[0] = const_cast<ggml_tensor*>(&xsrc);
    std::memcpy(&nd.op_params[0], &cs.eps, 4);
    if (cs.mode == 3) {
        std::memset(&sec, 0, sizeof sec);
        sec.type = GGML_TYPE_F32;
        for (int i = 0; i < 4; ++i) {
            sec.ne[i] = cs.ne[i];
            sec.nb[i] = dspec.nb[i];
        }
        sec.data = out;
        sec.src[cs.mul_swap ? 1 : 0] = &nd;
        sec.src[cs.mul_swap ? 0 : 1] = const_cast<ggml_tensor*>(&mssrc);
    } else if (cs.mode == 4) {
        std::memset(&sec, 0, sizeof sec);
        sec.type = GGML_TYPE_F32;
        for (int i = 0; i < 4; ++i) {
            sec.ne[i] = cs.ne[i];
            sec.nb[i] = dspec.nb[i];
        }
        sec.data = out;
        sec.src[0] = &nd;
        std::memcpy(&sec.op_params[0], &cs.scale, 4);
        const float zero_bias = 0.0f;
        std::memcpy(&sec.op_params[1], &zero_bias, 4);
    } else {
        // mode 5: the mul intermediate node is never materialized
        std::memset(&sec, 0, sizeof sec);
        sec.type = GGML_TYPE_F32;
        for (int i = 0; i < 4; ++i) {
            sec.ne[i] = cs.ne[i];
            sec.nb[i] = dspec.nb[i];
        }
        sec.data = nullptr;
        sec.src[cs.mul_swap ? 1 : 0] = &nd;
        sec.src[cs.mul_swap ? 0 : 1] = const_cast<ggml_tensor*>(&mssrc);
        std::memset(&ad, 0, sizeof ad);
        ad.type = GGML_TYPE_F32;
        for (int i = 0; i < 4; ++i) {
            ad.ne[i] = cs.ne[i];
            ad.nb[i] = dspec.nb[i];
        }
        ad.data = out;
        ad.src[cs.add_swap ? 1 : 0] = &sec;
        ad.src[cs.add_swap ? 0 : 1] = const_cast<ggml_tensor*>(&assrc);
    }
}

static void invoke(ggml_backend_cuda_context & ctx, bool native_impl, int mode,
                   ggml_tensor & nd, ggml_tensor & sec, ggml_tensor & ad) {
    if (native_impl) {
        switch (mode) {
            case 0: fg168_native_ggml_cuda_op_norm(ctx, &nd); break;
            case 1: fg168_native_ggml_cuda_op_group_norm(ctx, &nd); break;
            case 2: fg168_native_ggml_cuda_op_rms_norm(ctx, &nd); break;
            case 3: fg168_native_ggml_cuda_op_rms_norm_fused(ctx, &nd, &sec); break;
            case 4: fg168_native_ggml_cuda_op_rms_norm_scale_fused(ctx, &nd, &sec); break;
            case 5: fg168_native_ggml_cuda_op_rms_norm_fused_add(ctx, &nd, &sec, &ad); break;
            case 6: fg168_native_ggml_cuda_op_rms_norm_back(ctx, &nd); break;
            default: fg168_native_ggml_cuda_op_l2_norm(ctx, &nd); break;
        }
    } else {
        switch (mode) {
            case 0: ggml_cuda_op_norm(ctx, &nd); break;
            case 1: ggml_cuda_op_group_norm(ctx, &nd); break;
            case 2: ggml_cuda_op_rms_norm(ctx, &nd); break;
            case 3: ggml_cuda_op_rms_norm_fused(ctx, &nd, &sec); break;
            case 4: ggml_cuda_op_rms_norm_scale_fused(ctx, &nd, &sec); break;
            case 5: ggml_cuda_op_rms_norm_fused_add(ctx, &nd, &sec, &ad); break;
            case 6: ggml_cuda_op_rms_norm_back(ctx, &nd); break;
            default: ggml_cuda_op_l2_norm(ctx, &nd); break;
        }
    }
    ck(cudaStreamSynchronize(ctx.stream()));
}

static void run_case(ggml_backend_cuda_context & ctx, const Case & cs) {
    const int mode = cs.mode;
    cudaStream_t stream = ctx.stream();

    ggml_tensor X{}, D{}, MS{}, AS{}, G{};
    set_shape(X, cs.ne, cs.xpad);
    if (cs.x_stride0_ch) {
        X.nb[2] = 0;  // legal: higher strides are nonnegative whole floats
    }
    set_shape(D, cs.ne, cs.dpad);
    const bool has_mul = (mode == 3 || mode == 5);
    const bool has_add = (mode == 5);
    if (has_mul) {
        set_shape(MS, cs.mulne, cs.opad);
    }
    if (has_add) {
        set_shape(AS, cs.addne, cs.opad);
    }
    if (mode == 6) {
        const int64_t zero_pad[3] = {0, 0, 0};
        set_shape(G, cs.ne, zero_pad);
    }

    const size_t xspan = tensor_span(X);
    const size_t dspan = tensor_span(D);
    size_t packed_bytes = 1;
    for (int i = 0; i < 4; ++i) {
        packed_bytes *= (size_t)cs.ne[i];
    }
    packed_bytes *= 4;

    Buffer xb(xspan + (size_t)cs.x_mis);
    std::unique_ptr<Buffer> msb, asb, gb, obuf, nbuf, rb, alias_n, canary_c, canary_n;
    if (has_mul) {
        msb.reset(new Buffer(tensor_span(MS) + (size_t)cs.op_mis));
    }
    if (has_add) {
        asb.reset(new Buffer(tensor_span(AS) + (size_t)cs.op_mis));
    }
    if (mode == 6) {
        gb.reset(new Buffer(tensor_span(G)));
    }
    obuf.reset(new Buffer(dspan));
    nbuf.reset(new Buffer(packed_bytes));
    if (cs.alias_inplace) {
        alias_n.reset(new Buffer(xspan));
    }
    if (cs.node_canary) {
        canary_c.reset(new Buffer(256));
        canary_n.reset(new Buffer(256));
    }

    fill_tensor(xb, X, cs.pattern, 0, (size_t)cs.x_mis);
    if (has_mul && !cs.alias_operand) {
        fill_tensor(*msb, MS, cs.pattern, 1, (size_t)cs.op_mis);
    }
    if (has_add && !cs.alias_operand) {
        fill_tensor(*asb, AS, cs.pattern, 2, (size_t)cs.op_mis);
    }
    if (mode == 6) {
        fill_tensor(*gb, G, cs.pattern, 3, 0);
    }
    if (cs.alias_inplace) {
        fill_tensor(*alias_n, X, cs.pattern, 0, 0);
    }
    if (cs.alias_operand) {
        fill_tensor(*obuf, MS, cs.pattern, 1, 0);
        if (cs.native_mode != NATIVE_SKIP) {
            fill_tensor(*nbuf, MS, cs.pattern, 1, 0);
        }
    }

    X.data = (char *)xb.data() + cs.x_mis;
    if (has_mul) {
        MS.data = cs.alias_operand ? obuf->data() : (char *)msb->data() + cs.op_mis;
    }
    if (has_add) {
        AS.data = cs.alias_operand ? obuf->data() : (char *)asb->data() + cs.op_mis;
    }
    if (mode == 6) {
        G.data = gb->data();
    }
    D.data = cs.alias_inplace ? xb.data() : obuf->data();

    // ---- candidate run ----
    ggml_tensor cn{}, csec{}, cad{};
    if (mode >= 3 && mode <= 5) {
        wire_fused(cs, D, cn, csec, cad, X, MS, AS, cs.node_canary ? canary_c->data() : nullptr, D.data);
    } else {
        wire_unary(cs, D, cn, X, G, D.data);
    }
    invoke(ctx, false, mode, cn, csec, cad);

    // ---- independent strided reference (never calls candidate code) ----
    if (cs.ref_check) {
        rb.reset(new Buffer(dspan));
        auto launch_ref = [&](float * out) {
            const Cfg c = mirror_cfg(mode, X, D, has_mul ? &MS : nullptr, has_add ? &AS : nullptr, cs.groups);
            const dim3 grid(c.gx, c.gy, c.gz);
            const dim3 block(c.bs, 1, 1);
            const int n = (int)X.ne[0], rows = (int)X.ne[1], ch = (int)X.ne[2], smp = (int)X.ne[3];
            const int64_t sr = X.nb[1] / 4, sc2 = X.nb[2] / 4, ss2 = X.nb[3] / 4;
            const int64_t dr = D.nb[1] / 4, dc2 = D.nb[2] / 4, dsm = D.nb[3] / 4;
            const float * xd = (const float *)X.data;
            switch (mode) {
                case 0:
                    ref_norm_f32<<<grid, block, c.shmem, stream>>>(xd, out, n, ch, smp, sr, sc2, ss2, dr, dc2, dsm, cs.eps);
                    break;
                case 7:
                    ref_l2_f32<<<grid, block, c.shmem, stream>>>(xd, out, n, ch, smp, sr, sc2, ss2, dr, dc2, dsm, cs.eps);
                    break;
                case 2:
                    ref_rms_f32<false,false,false><<<grid, block, c.shmem, stream>>>(xd, out, n, ch, smp, sr, sc2, ss2, cs.eps,
                        nullptr, 0, 0, 0, 0, 0, 0, 0, nullptr, 0, 0, 0, 0, 0, 0, 0, 1.0f,
                        c.vec, c.vpt, dr, dc2, dsm);
                    break;
                case 3:
                    ref_rms_f32<true,false,false><<<grid, block, c.shmem, stream>>>(xd, out, n, ch, smp, sr, sc2, ss2, cs.eps,
                        (const float *)MS.data, MS.nb[1] / 4, MS.nb[2] / 4, MS.nb[3] / 4,
                        (int)MS.ne[0], (int)MS.ne[1], (int)MS.ne[2], (int)MS.ne[3],
                        nullptr, 0, 0, 0, 0, 0, 0, 0, 1.0f,
                        c.vec, c.vpt, dr, dc2, dsm);
                    break;
                case 4:
                    ref_rms_f32<false,false,true><<<grid, block, c.shmem, stream>>>(xd, out, n, ch, smp, sr, sc2, ss2, cs.eps,
                        nullptr, 0, 0, 0, 0, 0, 0, 0, nullptr, 0, 0, 0, 0, 0, 0, 0, cs.scale,
                        c.vec, c.vpt, dr, dc2, dsm);
                    break;
                case 5:
                    ref_rms_f32<true,true,false><<<grid, block, c.shmem, stream>>>(xd, out, n, ch, smp, sr, sc2, ss2, cs.eps,
                        (const float *)MS.data, MS.nb[1] / 4, MS.nb[2] / 4, MS.nb[3] / 4,
                        (int)MS.ne[0], (int)MS.ne[1], (int)MS.ne[2], (int)MS.ne[3],
                        (const float *)AS.data, AS.nb[1] / 4, AS.nb[2] / 4, AS.nb[3] / 4,
                        (int)AS.ne[0], (int)AS.ne[1], (int)AS.ne[2], (int)AS.ne[3],
                        1.0f, c.vec, c.vpt, dr, dc2, dsm);
                    break;
                default:
                    ref_rms_back_f32<<<grid, block, 0, stream>>>((const float *)G.data, xd, out,
                        n, rows, ch, cs.eps, sr, sc2, ss2, dr, dc2, dsm);
                    break;
            }
            ck(cudaGetLastError());
        };
        launch_ref((float *)rb->data());
        ck(cudaStreamSynchronize(stream));
    }

    // ---- native oracle run ----
    std::vector<unsigned char> nat;
    if (cs.native_mode != NATIVE_SKIP) {
        ggml_tensor XN = X, GN = G, MSN{}, ASN{};
        XN.data = cs.alias_inplace ? alias_n->data() : X.data;
        if (has_mul) {
            MSN = MS;
            MSN.data = cs.alias_operand ? nbuf->data() : MS.data;
        }
        if (has_add) {
            ASN = AS;
            ASN.data = cs.alias_operand ? nbuf->data() : AS.data;
        }
        Buffer & nout = cs.alias_inplace ? *alias_n : *nbuf;
        ggml_tensor nn{}, nsec{}, nad{};
        if (mode >= 3 && mode <= 5) {
            wire_fused(cs, D, nn, nsec, nad, XN, has_mul ? MSN : MS, has_add ? ASN : AS,
                       cs.node_canary ? canary_n->data() : nullptr, nout.data());
        } else {
            wire_unary(cs, D, nn, XN, GN, nout.data());
        }
        invoke(ctx, true, mode, nn, nsec, nad);
        nat = download(nout);
        check_guards(nat, false, nullptr, 0);
    }

    // ---- candidate output guard/padding integrity; inputs must be untouched ----
    const std::vector<unsigned char> cand = download(cs.alias_inplace ? xb : *obuf);
    check_guards(cand, true, &D, 0);
    if (!cs.alias_inplace) {
        check_guards(download(xb), true, &X, (size_t)cs.x_mis);
    }
    if (has_mul && !cs.alias_operand) {
        check_guards(download(*msb), true, &MS, (size_t)cs.op_mis);
    }
    if (has_add && !cs.alias_operand) {
        check_guards(download(*asb), true, &AS, (size_t)cs.op_mis);
    }
    if (mode == 6) {
        check_guards(download(*gb), true, &G, 0);
    }

    // ---- NaN policy: the affected row must be entirely NaN ----
    if (cs.nan_policy) {
        size_t nan_seen = 0;
        for (int64_t i3 = 0; i3 < cs.ne[3]; ++i3)
            for (int64_t i2 = 0; i2 < cs.ne[2]; ++i2)
                for (int64_t i1 = 0; i1 < cs.ne[1]; ++i1)
                    for (int64_t i0 = 0; i0 < cs.ne[0]; ++i0) {
                        if (i1 == 1) {
                            const float got = logical_at(cand, D, i0, i1, i2, i3);
                            uint32_t gbits = 0;
                            std::memcpy(&gbits, &got, 4);
                            req(((gbits & 0x7f800000u) == 0x7f800000u) && ((gbits & 0x007fffffu) != 0u),
                                "NaN propagates to the affected row");
                            ++nan_seen;
                        }
                    }
        req(nan_seen == (size_t)cs.ne[0] * (size_t)cs.ne[2] * (size_t)cs.ne[3], "full NaN row");
        ++nan_policy_cases;
    }

    // ---- numeric proofs ----
    if (cs.native_mode == NATIVE_EXACT) {
        compare_l_p(cs.name.c_str(), cand, D, nat, cs.ne, cs.nan_policy, true);
        ++native_cases;
    } else if (cs.native_mode == NATIVE_INFO) {
        if (!compare_l_p(cs.name.c_str(), cand, D, nat, cs.ne, cs.nan_policy, false)) {
            ++native_info_divergent;
        }
    }
    if (cs.ref_check) {
        compare_l_l(cs.name.c_str(), cand, D, download(*rb), D, cs.nan_policy);
        ++reference_cases;
    }

    // ---- fused intermediates must not be materialized ----
    if (cs.node_canary) {
        const std::vector<unsigned char> cc = download(*canary_c);
        bool ok = true;
        for (size_t i = 0; i < cc.size(); ++i) {
            if (cc[i] != 0xA5) {
                ok = false;
            }
        }
        req(ok, "rms intermediate node not materialized (candidate)");
        if (!nat.empty()) {
            const std::vector<unsigned char> nc = download(*canary_n);
            ok = true;
            for (size_t i = 0; i < nc.size(); ++i) {
                if (nc[i] != 0xA5) {
                    ok = false;
                }
            }
            req(ok, "rms intermediate node not materialized (native)");
        }
    }

    ++cases;
    int64_t total = 1;
    for (int i = 0; i < 4; ++i) {
        total *= cs.ne[i];
    }
    values += (size_t)total;
}

static std::vector<Case> build_cases() {
    std::vector<Case> v;
    auto add = [&](Case c) { v.push_back(c); };
    auto base = [](int mode, std::initializer_list<int64_t> ne) {
        Case c;
        c.mode = mode;
        int i = 0;
        for (int64_t d : ne) {
            c.ne[i++] = d;
        }
        return c;
    };
    char nm[64];

    // ---- unary modes 0/2/7: contiguous, scalar and vector block-size tiers ----
    for (int m : {0, 2, 7}) {
        for (int64_t cols : {61, 64, 132, 520, 1023, 1024, 2052}) {
            Case c = base(m, {cols, 2, 2, 1});
            snprintf(nm, sizeof nm, "unary_m%d_cols%lld", m, (long long)cols);
            c.name = nm;
            add(c);
        }
    }
    { Case c = base(2, {8192, 1, 1, 1});  c.name = "rms_vec_vpt4_8192";   add(c); }
    { Case c = base(2, {16388, 1, 1, 1}); c.name = "rms_over4096_scalar"; add(c); }

    // ---- strided sources: both implementations read via real strides ----
    { Case c = base(2, {64, 2, 2, 1}); c.xpad[0] = 1; c.name = "rms_xpad1f_scalar"; add(c); }
    { Case c = base(2, {64, 2, 2, 1}); c.xpad[0] = 4; c.name = "rms_xpad4f_vec";    add(c); }
    { Case c = base(0, {61, 2, 2, 1}); c.xpad[0] = 3; c.name = "norm_xpad3f";       add(c); }
    { Case c = base(7, {64, 2, 2, 1}); c.xpad[0] = 2; c.name = "l2_xpad2f";         add(c); }
    { Case c = base(2, {64, 2, 2, 1}); c.xpad[0] = 4; c.xpad[1] = 2; c.name = "rms_xpad_nb1_nb2"; add(c); }
    { Case c = base(2, {64, 2, 3, 1}); c.x_stride0_ch = true; c.name = "rms_x_stride0_channels";  add(c); }

    // ---- strided destinations: independent reference; native packed twin ----
    { Case c = base(0, {64, 2, 2, 1}); c.dpad[0] = 4; c.ref_check = true; c.name = "norm_dpad16B"; add(c); }
    { Case c = base(2, {64, 2, 2, 1}); c.dpad[0] = 4; c.ref_check = true; c.name = "rms_dpad16B_vec"; add(c); }
    { Case c = base(2, {64, 2, 2, 1}); c.dpad[0] = 4; c.dpad[1] = 2; c.ref_check = true; c.name = "rms_dpad_multi_scalar"; add(c); }
    { Case c = base(7, {64, 2, 2, 1}); c.dpad[0] = 4; c.ref_check = true; c.name = "l2_dpad16B"; add(c); }
    { Case c = base(2, {64, 2, 2, 1}); c.dpad[0] = 1; c.ref_check = true; c.native_mode = NATIVE_INFO; c.name = "rms_dpad4B_order_divergent"; add(c); }
    { Case c = base(0, {64, 2, 2, 1}); c.dpad[0] = 1; c.ref_check = true; c.name = "norm_dpad4B"; add(c); }

    auto m3 = [&](const char * tag, std::initializer_list<int64_t> ne, std::initializer_list<int64_t> mulne) {
        Case c = base(3, ne);
        int i = 0;
        for (int64_t d : mulne) {
            c.mulne[i++] = d;
        }
        c.name = std::string("m3_") + tag;
        return c;
    };
    auto m5 = [&](const char * tag, std::initializer_list<int64_t> ne, std::initializer_list<int64_t> mulne, std::initializer_list<int64_t> addne) {
        Case c = base(5, ne);
        int i = 0;
        for (int64_t d : mulne) {
            c.mulne[i++] = d;
        }
        i = 0;
        for (int64_t d : addne) {
            c.addne[i++] = d;
        }
        c.name = std::string("m5_") + tag;
        return c;
    };
    auto m4 = [&](const char * tag, std::initializer_list<int64_t> ne) {
        Case c = base(4, ne);
        c.name = std::string("m4_") + tag;
        return c;
    };
    auto m1 = [&](const char * tag, std::initializer_list<int64_t> ne, int groups) {
        Case c = base(1, ne);
        c.groups = groups;
        c.name = std::string("m1_") + tag;
        return c;
    };
    auto m6 = [&](const char * tag, std::initializer_list<int64_t> ne) {
        Case c = base(6, ne);
        c.name = std::string("m6_") + tag;
        return c;
    };

    // ---- mode 3: fused rms * mul ----
    add(m3("full", {64, 2, 2, 1}, {64, 2, 2, 1}));
    { Case c = m3("swap", {64, 2, 2, 1}, {64, 2, 2, 1}); c.mul_swap = true; add(c); }
    add(m3("rowbcast", {64, 2, 2, 1}, {64, 1, 1, 1}));
    add(m3("scalarbcast", {64, 2, 2, 1}, {1, 1, 1, 1}));
    add(m3("colbcast", {64, 2, 2, 1}, {16, 1, 1, 1}));
    add(m3("chanbcast", {64, 2, 2, 1}, {64, 2, 1, 1}));
    { Case c = m3("mulpad16B", {64, 2, 2, 1}, {64, 2, 2, 1}); c.opad[0] = 4; add(c); }
    { Case c = m3("mulmisaligned", {64, 2, 2, 1}, {64, 2, 2, 1}); c.op_mis = 4; add(c); }
    add(m3("vec_bs128", {132, 2, 1, 1}, {132, 2, 1, 1}));
    add(m3("scalar_c61", {61, 2, 1, 1}, {61, 2, 1, 1}));
    { Case c = m3("canary", {64, 2, 2, 1}, {64, 2, 2, 1}); c.node_canary = true; add(c); }
    { Case c = m3("alias_operand", {64, 2, 2, 1}, {64, 2, 2, 1}); c.alias_operand = true; add(c); }
    { Case c = m3("dpad16B", {64, 2, 2, 1}, {64, 2, 2, 1}); c.dpad[0] = 4; c.ref_check = true; add(c); }

    // ---- mode 5: fused rms * mul + add ----
    add(m5("full", {64, 2, 2, 1}, {64, 2, 2, 1}, {64, 2, 2, 1}));
    { Case c = m5("mulswap", {64, 2, 2, 1}, {64, 2, 2, 1}, {64, 2, 2, 1}); c.mul_swap = true; add(c); }
    { Case c = m5("addswap", {64, 2, 2, 1}, {64, 2, 2, 1}, {64, 2, 2, 1}); c.add_swap = true; add(c); }
    add(m5("addrowbcast", {64, 2, 2, 1}, {64, 2, 2, 1}, {64, 1, 1, 1}));
    add(m5("mulscalar_addrow", {64, 2, 2, 1}, {1, 1, 1, 1}, {64, 1, 1, 1}));
    add(m5("addcolbcast", {64, 2, 2, 1}, {64, 2, 2, 1}, {16, 1, 1, 1}));
    add(m5("vec_bs128", {132, 2, 1, 1}, {132, 2, 1, 1}, {132, 2, 1, 1}));
    add(m5("scalar_c61", {61, 2, 1, 1}, {61, 2, 1, 1}, {61, 2, 1, 1}));
    { Case c = m5("dpad16B", {64, 2, 2, 1}, {64, 2, 2, 1}, {64, 2, 2, 1}); c.dpad[0] = 4; c.ref_check = true; add(c); }
    { Case c = m5("alias_operand", {64, 2, 2, 1}, {64, 2, 2, 1}, {64, 2, 2, 1}); c.alias_operand = true; add(c); }

    // ---- mode 4: fused rms scaled ----
    { Case c = m4("scale_pos", {64, 2, 2, 1}); c.scale = 2.5f; add(c); }
    { Case c = m4("scale_neg", {64, 2, 2, 1}); c.scale = -1.5f; add(c); }
    { Case c = m4("scale_zero", {64, 2, 2, 1}); c.scale = 0.0f; add(c); }
    { Case c = m4("vec_bs128", {132, 2, 1, 1}); c.scale = 2.5f; add(c); }
    { Case c = m4("scalar_c61", {61, 2, 1, 1}); c.scale = -0.75f; add(c); }
    { Case c = m4("dpad16B", {64, 2, 2, 1}); c.scale = 2.5f; c.dpad[0] = 4; c.ref_check = true; add(c); }
    { Case c = m4("eps0", {64, 2, 2, 1}); c.scale = 2.5f; c.eps = 0.0f; add(c); }
    { Case c = m4("canary", {64, 2, 2, 1}); c.scale = 3.0f; c.node_canary = true; add(c); }

    // ---- mode 1: group norm (contiguous per contract) ----
    add(m1("basic", {24, 2, 4, 1}, 2));
    add(m1("bs1024", {64, 4, 8, 2}, 2));
    add(m1("partial_final_denominator", {3, 1, 3, 1}, 2));
    add(m1("empty_tail_block", {8, 2, 3, 2}, 2));
    add(m1("groups_eq_channels", {16, 2, 3, 1}, 3));
    { Case c = m1("eps0", {16, 2, 2, 1}, 2); c.eps = 0.0f; add(c); }
    { Case c = m1("zero_group", {16, 2, 2, 1}, 2); c.pattern = 2; add(c); }

    // ---- mode 6: rms backward ----
    add(m6("c64", {64, 2, 2, 1}));
    add(m6("c61", {61, 2, 1, 1}));
    add(m6("c1024_bs1024", {1024, 1, 1, 1}));
    { Case c = m6("xpad_strided_source", {64, 2, 2, 1}); c.xpad[0] = 4; c.ref_check = true; c.native_mode = NATIVE_INFO; add(c); }
    { Case c = m6("dpad_strided_output", {64, 2, 2, 1}); c.dpad[0] = 4; c.ref_check = true; c.native_mode = NATIVE_INFO; add(c); }
    { Case c = m6("xdpad_strided", {64, 2, 2, 1}); c.xpad[0] = 4; c.dpad[0] = 4; c.ref_check = true; c.native_mode = NATIVE_INFO; add(c); }
    { Case c = m6("eps0", {64, 1, 1, 1}); c.eps = 0.0f; add(c); }

    // ---- finite special patterns ----
    { Case c = base(0, {16, 1, 1, 1}); c.pattern = 1; c.name = "special_finite_m0"; add(c); }
    { Case c = base(2, {16, 1, 1, 1}); c.pattern = 1; c.name = "special_finite_m2"; add(c); }
    { Case c = base(7, {16, 1, 1, 1}); c.pattern = 1; c.name = "special_finite_m7"; add(c); }
    { Case c = base(1, {16, 2, 1, 1}); c.pattern = 1; c.name = "special_finite_m1"; add(c); }
    { Case c = base(6, {16, 1, 1, 1}); c.pattern = 1; c.name = "special_finite_m6"; add(c); }
    { Case c = base(2, {16, 1, 1, 1}); c.pattern = 4; c.name = "overflow_to_zero_m2"; add(c); }
    { Case c = base(7, {16, 1, 1, 1}); c.pattern = 4; c.name = "overflow_to_zero_m7"; add(c); }
    for (int m : {0, 2, 7}) {
        Case c = base(m, {8, 2, 1, 1});
        c.pattern = 2;
        c.name = std::string("zero_row_m") + std::to_string(m);
        add(c);
    }
    for (int m : {0, 2, 7}) {
        Case c = base(m, {64, 1, 1, 1});
        c.eps = 0.0f;
        c.name = std::string("eps0_m") + std::to_string(m);
        add(c);
    }
    { Case c = base(0, {1, 1, 65537, 1}); c.name = "grid_clamp_channels_m0"; add(c); }
    { Case c = base(2, {1, 1, 1, 65537}); c.name = "grid_clamp_samples_m2"; add(c); }
    { Case c = base(0, {32, 2, 1, 1}); c.pattern = 3; c.nan_policy = true; c.name = "nan_policy_m0"; add(c); }
    { Case c = base(2, {32, 2, 1, 1}); c.pattern = 3; c.nan_policy = true; c.name = "nan_policy_m2"; add(c); }

    // ---- aliasing (native-supported shapes only) ----
    { Case c = base(2, {64, 2, 1, 1}); c.alias_inplace = true; c.name = "alias_inplace_m2"; add(c); }
    { Case c = base(0, {64, 2, 1, 1}); c.alias_inplace = true; c.name = "alias_inplace_m0"; add(c); }

    return v;
}

static bool env_zero(const char * name) {
    const char * v = getenv(name);
    return v != nullptr && strcmp(v, "0") == 0;
}

int main() {
    ggml_backend_cuda_context ctx(0);
    ck(cudaSetDevice(0));
    req(fg168_cuda_host_seen() == 0, "host witness starts at zero");

    const bool rollback       = env_zero("GGML_GOOSE_CUDA_NORM");
    const bool legacy_rms_off = env_zero("GGML_GOOSE_RMS");

    const std::vector<Case> all = build_cases();
    for (const Case & c : all) {
        const char*filter=getenv("FG168_CASE_FILTER");
        if(filter&&c.name!=filter)continue;
        const bool native_path=rollback||(legacy_rms_off&&c.mode>=2&&c.mode<=5);
        if(native_path&&(c.native_mode!=NATIVE_EXACT||c.dpad[0]||c.dpad[1]||c.dpad[2]))continue;
        run_case(ctx, c);
    }

    if(getenv("FG168_CASE_FILTER")){printf("PASS isolated fixture case values=%zu\n",values);return 0;}
    uint64_t bits[3] = {};
    req(fg168_cuda_device_read(bits) == 0, "device witness available");
    const uint64_t host = fg168_cuda_host_seen();
    if (rollback) {
        req(host == 0, "rollback: no Goose host paths recorded");
        req(bits[0] == 0 && bits[1] == 0 && bits[2] == 0, "rollback: device witness silent");
    } else if (legacy_rms_off) {
        req(host == 0xC3, "GGML_GOOSE_RMS=0: only modes 0,1,6,7 reach the Goose host path");
        req(bits[0] == 0xC3, "GGML_GOOSE_RMS=0: device mode mask");
        req(bits[2] == 0, "GGML_GOOSE_RMS=0: no mul/add/scale fusion executed");
    } else {
        req(host == 0xFF, "all 8 host paths seen");
        req(bits[0] == 0xFF, "all 8 device mode bits seen");
        req((bits[1] & 3) == 3, "scalar and vector rms paths both seen");
        req((bits[2] & 7) == 7, "mul, add and scaled fusion bits all seen");
    }

    printf("PASS cases=%zu native_bitexact=%zu independent_ref=%zu nan_policy=%zu native_info_divergent=%zu values=%zu checks=%zu rollback=%d legacy_rms_off=%d\\n",
           cases, native_cases, reference_cases, nan_policy_cases, native_info_divergent, values, checks,
           (int)rollback, (int)legacy_rms_off);
    return 0;
}
