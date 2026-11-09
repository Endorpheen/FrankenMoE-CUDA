# EXP203: dense-path dispatch policy in Goose (APX: P3-10)

`ggml_cuda_should_use_mmq` decides whether a quantized matmul takes the MMQ kernel or falls back
to cuBLAS / `mul_mat_vec_q`. The MMQ family is 39.5% of Apex kernel time, so this is the decision
that governs the largest remaining block. It is pure host-side integer logic, which is why the
emitter's address typing does not block it.

## Ported

| Goose | native | behaviour |
|---|---|---|
| `mmq_type_supported` | mmq.cu:363-402 | the 22-type MMQ support set |
| `should_use_mmq` | mmq.cu:355-481 | the full decision, NVIDIA/AMD/MUSA branches included |
| cc predicates | common.cuh:50-108 | `is_nvidia`, `is_cdna`, `is_cdna3`, `is_rdna3*`, `is_gcn_apu` |

Device facts arrive as arguments: `cc`, `smpbo`, `ggml_cuda_highest_compiled_arch(cc)`,
`turing_mma_available`, `fp16_mma_hardware_available`, `amd_mfma_available`, `amd_wmma_available`
and the two `GGML_CUDA_FORCE_*` switches. Goose owns the decision tree; the host owns what the
device is.

## Evidence

    FG203D_MMQ_DISPATCH calls=4679
    2539.36 / 25.65, tokens and all 128 probabilities equal, text sha256 e68df05b

Gate: 593,237 checks / 0 failures. The reference is a transcription of the native body (the
native function lives in a `.cu` TU that pulls device intrinsics through `mmq.cuh`, so it is
transcribed rather than linked, as the loader gate did for `std::algorithm`). The grid walks every
type, 22 compute capabilities, batch sizes around the 64 and 128/256 boundaries, expert counts
around 64, both sides of the 48 KiB shared-memory floor, all five compiled-arch values and all
64 combinations of the boolean switches, sampled deterministically.

## Defects the gate found

- **VEGA returned the wrong branch.** I wrote `if cc == vega: return 0`, but native treats VEGA
  and the GCN APUs together: `return n_experts > 0`. The gate caught it on
  `cc=16779520` (VEGA) with `n_experts=64`.
- **Function identification by arity failed.** Deduplication collapses the eleven one-argument
  bodies (`cc_pascal`, `cc_vega`, …) into candidates with identical signatures, so `mmq_type_supported`
  could not be identified from the generated file. Resolved by calling each with 12, 0, 41, 999, 14
  and matching the support pattern, the same method EXP202 needed.

## Switch

`GGML_GOOSE_CUDA_MMQ_DISPATCH=0` restores the native body.

## Build note

The link must start from EXP196's command (`local-test-commands.json`), not EXP185's: EXP196 is
the current CUDA base and already carries the pinned and VMM-pool modules. The first attempt was
built on EXP185 and would have silently dropped those.

`runtime/exp203/run` differs from `runtime/exp202/run` in exactly one file.
