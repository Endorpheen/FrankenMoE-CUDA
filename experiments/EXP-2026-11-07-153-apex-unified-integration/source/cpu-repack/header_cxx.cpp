#include "repack-provider.h"
#include <cstddef>
static_assert(sizeof(fg006_block_q4_K) == 144);
static_assert(sizeof(fg006_block_q4_Kx8) == 1152);
static_assert(sizeof(fg006_block_q8_K) == 292);
static_assert(sizeof(fg006_block_q8_Kx4) == 1168);
static_assert(offsetof(fg006_block_q4_Kx8, scales) == 32);
int fg006_header_cxx_probe() {
    return fg006_repack_q4_K_supported(1, 8) && !fg006_repack_q4_K_supported(0, 8);
}
