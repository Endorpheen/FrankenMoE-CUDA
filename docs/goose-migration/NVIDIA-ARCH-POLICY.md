# NVIDIA architecture policy after 2026-11-13

Owner clarification: the supported NVIDIA target is Ampere (RTX 30xx), Ada
Lovelace (RTX 40xx) and Blackwell (RTX 50xx). The only hardware acceptance host
is the available RTX 4070 (Ada, sm_89). RTX 30xx and 50xx must be supported in
code but must not delay migration with separate builds or test matrices.

## Rules

1. Code paths for Ampere, Ada and Blackwell stay in the same universal runtime.
   Selection is by actual device capability (`cc`, compiled arch, CUDA version),
   not by model name or a build profile.
2. Acceptance is performed only on the RTX 4070. A path that cannot run on this
   host is recorded as hardware-unverified; it is never reported as tested.
3. No separate builds, CI matrices or benchmark series for RTX 30xx/50xx are
   created now. Their hardware validation is deferred.
4. A deferred path must still have a clear code location, a capability predicate
   and a documented fallback. "Not tested here" is not "not implemented".
5. The functional migration stays the priority. New architecture work must not
   reopen or delay Apex closure.

## Current state

| architecture | build/code | acceptance |
|---|---|---|
| Ampere (sm_80/86) | CUDA sources compile; capability predicates exist | not run on this host; hardware-unverified |
| Ada (sm_89) | full runtime, Apex baseline | accepted on RTX 4070 |
| Blackwell (sm_100/120) | NVFP4 source path exists under `BLACKWELL_MMA_AVAILABLE`; no Blackwell host | not built as a separate target, not tested |

## NVFP4

NVFP4 is a Blackwell FP4 path:

- the native activation quantizer is compiled only under
  `BLACKWELL_MMA_AVAILABLE` and is `NO_DEVICE_CODE` on sm_89;
- its consumer is a Blackwell FP4 tensor-core MMQ/GEMM path;
- CUDA 12.8+ provides the hardware E2M1/UE4M3 conversions.

It is therefore **hardware-unavailable on the RTX 4070** and is not claimed as
tested. On a Blackwell build the same runtime should dispatch to the native or
Goose NVFP4 path by capability.

For non-Blackwell GPUs the universal runtime needs a software compatibility
path, tracked as POST-05:

1. software NVFP4 activation quantizer in Goose: UE4M3 sub-block scale search,
   E2M1 LUT, `block_fp4_mmq` packing, row `scale` array;
2. BF16 dequant/GEMM fallback that consumes the packed blocks;
3. capability dispatch: Blackwell hardware path when compiled and available,
   software fallback otherwise;
4. gate the software quantizer against a CPU reference (no native oracle on
   sm_89), and mark the Blackwell hardware path unverified until a Blackwell
   host exists.

## What is not allowed

- Claiming NVFP4 is tested because the software quantizer produced bytes on
  sm_89.
- Adding RTX 30/50 build matrices to the migration critical path.
- Reopening P3-07 or delaying Apex closure for an unverifiable Blackwell run.
- Silent fallback: an unsupported capability must be visible in the dispatch
  decision and in the logs.
