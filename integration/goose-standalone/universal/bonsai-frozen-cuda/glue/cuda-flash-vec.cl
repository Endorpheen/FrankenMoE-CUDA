#include <cuda-flash-vec.cl>

__kernel void goose_flash_vec_q4_packed_pair(__global const uint8_t * q, __global const uint8_t * k,
    __global const uint8_t * v, __global uint8_t * parts, __global uint8_t * meta,
    __global const int64_t * cfg, int32_t keys, float scale) {
    uint32_t pair, part, tiles, thread;
    __local uint32_t shared[1152];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(pair));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(part));
    __asm__("mov.u32 %0, %%nctaid.y;" : "=r"(tiles));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    bonsai_flash_packed_g((uint64_t) q, (uint64_t) k, (uint64_t) v, (uint64_t) parts,
        (uint64_t) meta, (uint64_t) cfg, keys, pair, part, tiles, thread, (uint64_t) shared,
        2);
}

__kernel void goose_flash_vec_q4_packed_quad(__global const uint8_t * q, __global const uint8_t * k,
    __global const uint8_t * v, __global uint8_t * parts, __global uint8_t * meta,
    __global const int64_t * cfg, int32_t keys, float scale) {
    uint32_t group_of_heads, part, tiles, thread;
    __local uint32_t shared[1152];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group_of_heads));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(part));
    __asm__("mov.u32 %0, %%nctaid.y;" : "=r"(tiles));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    bonsai_flash_packed_g((uint64_t) q, (uint64_t) k, (uint64_t) v, (uint64_t) parts,
        (uint64_t) meta, (uint64_t) cfg, keys, group_of_heads, part, tiles, thread, (uint64_t) shared,
        4);
}
