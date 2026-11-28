#include "typed-memory.h"
#ifndef GOOSE_CUDA_FROZEN_ABI
#include "checked-device-arithmetic.h"
#endif
#pragma OPENCL FP_CONTRACT ON
#include "typed-memory-bridge.h"
#include "cuda-intrinsic-aliases.h"
#include "cuda-checked-abi.h"
#include "cuda-scalar-ptx.h"
#include "cuda-half-ptx.h"
#include "cuda-device-scalar-device.h"


static void fg248d_fail(int64_t code) { (void) code; device_trap(); }


#include "q4k-device.h"
#include "q6k-device.h"

static void fg_device_decode_q4(uint64_t w, uint64_t scratch, int32_t lane) {
    fg182d_q4_K_g(w, scratch, 0, lane);
}
static void fg_device_decode_q6(uint64_t w, uint64_t scratch, int64_t lane) {
    q6_K_g(w, scratch, 0, lane);
}
#include "cuda-device-ptx-0.h"

#include "cuda-device-ptx-1.h"

#include "matmul-device.h"







#include "cuda-device-ptx-2.h"


#ifndef GOOSE_CUDA_FROZEN_ABI
#ifdef GOOSE_CUDA_MMQ_IMAGE
#define fg_mla_mma_f16(acc,a0,a1,a2,a3,b0,b1) ({ \
    __typeof__(acc) d = (acc); \
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 " \
        "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};" \
        : "+f"(d.a_g), "+f"(d.b_g), "+f"(d.c_g), "+f"(d.d_g) \
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1)); d; })
#else
#define fg_mla_mma_f16(acc,a0,a1,a2,a3,b0,b1) (acc)
#endif
#include "cuda-device-ptx-3.h"
static uint32_t fg_dsa_bins_add(uint64_t base, int64_t index) {
    uint32_t old;
    __asm__ volatile("atom.shared.add.u32 %0, [%1], 1;" : "=r"(old) :
        "r"((uint32_t)(base + 4*index)) : "memory");
    return old;
}
static uint32_t fg_mla_any_visible(uint32_t seen) {
    uint32_t count;
    __asm__ volatile("{ .reg .pred visible; setp.ne.u32 visible, %1, 0;"
        " bar.red.popc.u32 %0, 0, visible; }" : "=r"(count) : "r"(seen) : "memory");
    return count;
}
static uint32_t fg_mla_visible_ballot(uint32_t seen) {
    uint32_t vote;
    __asm__ volatile("{ .reg .pred selected; setp.ne.u32 selected, %1, 0;"
        " vote.sync.ballot.b32 %0, selected, 0xffffffff; }" : "=r"(vote) : "r"(seen));
    return vote;
}
#include "cuda-device-ptx-4.h"
#endif
#include "q4-attention-device.h"
#undef fg_mla_mma_f16
#include "cuda-attention-entries.h"





// Eight pointer words precede the Goose-generated strides and launch plan.
typedef struct { uint64_t words[57]; } gdn_launch_packet;

static uint64_t fg_gdn_read_word(int64_t packet, int64_t index) {
    return ((const uint64_t *)packet)[index];
}


#include "gdn-device.h"

static float fg176r_ld_f32(uint64_t src, uint64_t at) {
    float value;
    __asm__ volatile("ld.f32 %0, [%1];" : "=f"(value) : "l"(src + at) : "memory");
    return value;
}





#pragma OPENCL FP_CONTRACT OFF
#include "q4-set-rows-device.h"


#include "cuda-device-ptx-5.h"

// Keep the validated shape words in CUDA parameter space. Both vector entries
// consume the same plan layout without a separate upload to device memory.
typedef struct { int64_t words[26]; } vector_launch_packet;
static int64_t fg_vector_word(uint64_t cfg, int64_t index) {
    return ((const int64_t *) cfg)[index];
}

typedef struct { float x_g, y_g, z_g, w_g; } CVVec4_g;
static CVVec4_g fg_vector_load4(uint64_t address) {
    const float4 value = *(__global const float4 *) address;
    return (CVVec4_g) {value.x, value.y, value.z, value.w};
}
static void fg_vector_store4(uint64_t address, CVVec4_g v) {
    *(__global float4 *) address = (float4) (v.x_g, v.y_g, v.z_g, v.w_g);
}

#ifndef GOOSE_CUDA_FROZEN_ABI

#include "cuda-device-ptx-6.h"
#endif
#include "vector-device.h"
#include "cuda-rms-reference-abi.h"



#include "cuda-device-ptx-7.h"
#include "rope-device.h"

static void fg170s_lc(void) {}
static void fg170s_sync(void) {}

#include "ssm-conv-device.h"


#include "routing-device.h"


static float fg434c_ld_f32(uint64_t base, uint64_t offset) {
    float value;
    __asm__ volatile("ld.f32 %0, [%1];" : "=f"(value) : "l"(base + offset) : "memory");
    return value;
}

static void fg434c_st_f32(uint64_t base, uint64_t offset, float value) {
    __asm__ volatile("st.f32 [%0], %1;" :: "l"(base + offset), "f"(value) : "memory");
}


#include "transform-device.h"


static void fg183f_st_f32(uint64_t base, uint64_t byte, float value) {
    __asm__ volatile("st.f32 [%0], %1;" :: "l"(base + byte), "f"(value) : "memory");
}


static void fg183f_fail(int64_t code) {
    (void) code;
    device_trap();
}

#include "cuda-device-ptx-8.h"
#include "fwht-device.h"

#include "cuda-fwht-entries.h"

#define fg179w_ld_f32 fg183f_ld_f32
#define fg179w_fail fg183f_fail
#pragma OPENCL FP_CONTRACT OFF
#include "moe-weighted-device.h"
#pragma OPENCL FP_CONTRACT ON
#undef fg179w_ld_f32
#undef fg179w_fail

#include "cuda-activation-entries.h"
#include "cuda-apex-epilogue-entries.h"

#ifdef GOOSE_CUDA_MMQ_IMAGE
static void fg_cuda_cp16(uint64_t dst, uint64_t src) {
    __asm__ volatile("cp.async.cg.shared.global [%0], [%1], 16;" :: "r"((uint32_t) dst), "l"(src) : "memory");
}
#include "cuda-device-ptx-9.h"
#include "cuda-mmq-abi.h"
#include "cuda-mmvq-abi.h"
#include "cuda-dense-mma-abi.h"
#include "cuda-gdn-mma-abi.h"
#include "cuda-flash-mma-abi.h"
#include "cuda-flash-vec-abi.h"
#endif

#include "cuda-up-gate-entries.h"
