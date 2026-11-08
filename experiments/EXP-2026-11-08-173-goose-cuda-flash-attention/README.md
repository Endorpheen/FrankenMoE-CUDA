# EXP173: coherent Apex FlashAttention decode/prefill path

Hypothesis: transfer the actual D256, QF32, KVQ4_0 vector decode and F16 TensorCore prefill path to the current completed self-hosted Goose compiler, including codecs, masks, online softmax, partition/combine/fixup, the original async pipeline and host scratch/route policy. Do not force vector prefill or disable async/GQA/stream-K. Preserve immutable EXP172 as rollback. Production and compiler remain unchanged.

Status: **COMPONENT CHECKPOINT; NOT RUNTIME ACCEPTED**. No combined candidate is linked/activated and no EXP173 Apex request has run. No speed or complete-Apex coverage claim.

Validated components: vector decode has 12 native-bitwise cases, one graph capture/replay and actual device witnesses for N1/N2/combine and mask/sinks/KVmax/parallel partitions; asynchronous K/V+mask loaders have four native and independent-byte-oracle cases; contiguous/strided/tail Q4-to-F16 staging has six native-bitwise cases; prefill TensorCore iteration has four native-bitwise complete per-thread accumulator/max/rowsum state cases, including both GQA configurations and multiple KV chunks. The latter is a component-state comparison, not a complete FlashAttention or independent mathematical reference. An actual illegal shared read in the new ldmatrix adapter justified a narrowly scoped memcheck; after matching EXP172's proven address ABI, all four cases pass with zero memcheck errors. Loader/staging canaries do not prove absence of every read-only out-of-bounds access.

The initial vector check differed by a few ULP. Quantized Q and QK dot probes matched; per-thread tracing isolated second-chunk VKQ accumulation. Same-command PTX proves native NVCC contracts the old-accumulator scaling with the first rounded V*prob term, instead of first rounding old*scale. Goose now explicitly expresses that contraction through scalar ISA primitives. The unchanged zero-tolerance 12-case check passes. No original numerical callback replaces Goose. The failed receipt and native PTX excerpt are retained; no tolerance was relaxed.

Parent review also corrected worker MMA tile byte offsets (each 32x128-half2 tile is 16384 bytes), float bit reinterpret assumptions, Q addressing/packing/mask offsets and host planning bugs. Worker-named files and contracts are historical unaccepted drafts; the corrected `*-device.goose` files are canonical. Host corrected draft emits C but remains unaccepted: shared attribute once-cache concurrency, bounds/alias/output ownership, deferred scope, effect ABI and fault paths require integration checks. Process/mask/fixup drafts remain outside the measured runtime.

Reproduce the component checks from this experiment directory using Python3:

```bash
python3 build_vector_component.py
python3 check_vector.py
python3 check_loaders.py
python3 check_staging.py
python3 check_mma_iter.py
```

These build only isolated components under `runtime/exp173/build`, pin external source headers and the completed Stage3 compiler, and link the fixtures against immutable EXP172. They do not build/activate a full server, change production or benchmark any operation. The iteration check does not routinely rerun memcheck; its concrete-error diagnostic is retained in the result receipt. The final coherent gate still needs host/kernel/fixup integration, rollback/preflight/liveness/export checks and one fixed Apex32K/128 request. Do not count this checkpoint as a completed delivery batch, universal requirement or performance qualification.
