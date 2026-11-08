#include "repack-provider.h"
#include <stddef.h>
_Static_assert(sizeof(fg006_block_q4_K) == 144, "q4_K size");
_Static_assert(offsetof(fg006_block_q4_K, qs) == 16, "q4_K qs offset");
_Static_assert(sizeof(fg006_block_q4_Kx8) == 1152, "q4_Kx8 size");
_Static_assert(offsetof(fg006_block_q4_Kx8, d) == 0, "q4_Kx8 d offset");
_Static_assert(offsetof(fg006_block_q4_Kx8, dmin) == 16, "q4_Kx8 dmin offset");
_Static_assert(offsetof(fg006_block_q4_Kx8, scales) == 32, "q4_Kx8 scales offset");
_Static_assert(offsetof(fg006_block_q4_Kx8, qs) == 128, "q4_Kx8 qs offset");
_Static_assert(sizeof(fg006_block_q8_K) == 292, "q8_K size");
_Static_assert(offsetof(fg006_block_q8_K, qs) == 4, "q8_K qs offset");
_Static_assert(offsetof(fg006_block_q8_K, bsums) == 260, "q8_K bsums offset");
_Static_assert(sizeof(fg006_block_q8_Kx4) == 1168, "q8_Kx4 size");
_Static_assert(offsetof(fg006_block_q8_Kx4, qs) == 16, "q8_Kx4 qs offset");
_Static_assert(offsetof(fg006_block_q8_Kx4, bsums) == 1040, "q8_Kx4 bsums offset");
int fg006_header_c_probe(void) {
    return fg006_repack_q4_K_supported(1, 512) && !fg006_repack_q4_K_supported(1, 4);
}
