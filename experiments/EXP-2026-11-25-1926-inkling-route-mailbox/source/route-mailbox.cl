__kernel void goose_i2_route_mailbox(__global uint8_t *input,
    __global uint8_t *output, __global int64_t *config,
    __global volatile uint32_t *mailbox, uint32_t sequence) {
    uint32_t lane;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    __local int32_t scratch[4096], flags[128];
    fg_route_sort_g((uint64_t)input, (uint64_t)output, (uint64_t)config, 0, lane, 128,
                    (uint64_t)scratch, (uint64_t)flags);
    fg_device_barrier();
    if (lane == 0) {
        const __global volatile uint32_t *ids = (const __global volatile uint32_t *)output;
        for (uint32_t i = 0; i < 8; ++i) mailbox[i] = ids[i];
        __asm__ volatile("st.release.sys.global.u32 [%0], %1;"
                         :: "l"((uint64_t)(mailbox + 8)), "r"(sequence) : "memory");
    }
}
