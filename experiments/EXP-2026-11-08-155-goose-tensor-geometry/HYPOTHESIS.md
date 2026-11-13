# EXP155: Goose tensor geometry

Port the complete shared shape/stride/byte-span/broadcast predicates as one coherent module from the verified EXP154 runtime. Preserve exact native integer/layout semantics, public C ABI and SIMD/CUDA numerical kernels. Compile generated Goose logic in the same translation unit as its typed POD projections to avoid per-field foreign-call overhead. Retain a default-on, cached explicit disable switch and sparse per-thread liveness witness. No EXP153 worker source is imported.

Acceptance: independent frozen-native differential corpus across quantized/unquantized, empty, strided, permuted, padded, broadcast and singleton layouts; checked exported ABI; one unchanged Apex32768/128 request with exact text/IDs/top5,127graph reuses, bounded RAM/VRAM and small speed variation versus EXP154. Full P4-03 remains open until constructors/context/type ownership are also transferred.
