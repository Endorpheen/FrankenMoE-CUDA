# EXP-2026-11-16-906: restore reference host-registration defaults

Status: **ACCEPTED** after the approved A-B-B-A block. Base: a11c5662e5b50040e08937d3a95073a0c1a978f6.

## Immutable control and scope

Control: `/home/end0/AI/LlamaServer/franken-recovery-20261116/run-fixed`, manifest in EXP905. Core SHA-256: `4eba824255f06ab1fb8e5b6a9e98ae7bd0706773abc8bf86547d86b60cde98ac`. CUDA SHA-256: `992e4e46dfbecb5199ae040f35e3dc1b5df9dbbce0429acd8f18208d88b0469d`. The existing runtime-store byte backup contains this control and the source checkpoint. Neither the control nor the reference is modified. No reference binary/object is a candidate input.

Candidate changes only `ggml_cuda_register_host_enabled`: when the variable is absent, use `!ggml_cuda_pinned_memory_disabled()`, exactly as the saved reference binary does. Explicit `GGML_CUDA_REGISTER_HOST=0` still disables registration. No model/profile, sampling, cache-policy, quantization, graph or kernel arithmetic changes.

## M0 and mechanism hypothesis

The fixed Apex profile uses CPU-resident mmap MoE experts, a 4992 MiB GPU expert cache, CUDA execution for eligible graph operations, and q4_0 KV at context 262144. The accepted core collects resident host expert ranges, page-aligns/merges them, checks RAM headroom and residency, then requests host registration through the CUDA backend. Weights remain mmap-owned; no extra full-weight copy is introduced. Cache admission/SLRU and transfers retain their existing order. Exact synchronization costs and time attributable to pageable transfers are UNKNOWN.

MEASURED: prior source-built control decoded at median 39.659 tok/s and prefills at 1837.808 tok/s, all six control hashes correct. This is historical context, not the new paired control.

STATICALLY CONFIRMED: saved reference CUDA registration defaults on unless pinned memory is disabled; the fresh source backend defaults off. Both use the same explicit override name and CUDA host-register flags.

HYPOTHESIS: restoring the reference default enables resident expert pinning and lowers transfer overhead, improving end-to-end single-response decode and prefill.

UNKNOWN: the critical-path transfer fraction. Amdahl upper bound is `1/(1-f)` if fraction `f` were eliminated; no measured `f` is available, so no numeric speedup is promised. Matching historical reference decode would require about +19.4% over the old control median. Overlapping CPU/GPU time is not added.

## Minimal candidate build and gate

Compile only the changed CUDA translation unit with its existing recorded flags, then link a new DSO using the unchanged objects from the independent source build. Store output in a new LlamaServer subdirectory. Record input hashes and commands. Copy all runtime components into a new candidate directory; only libggml-cuda.so.0 may differ from the immutable control. Require a fully resolved link and disassembly showing reference-equivalent default handling before any model run.

## Approved measurement block

Igor approved four sequential server lifecycles, A-B-B-A, three original 32768/512 requests each, at most 15 minutes total. Use the fixed Apex profile and port 9101, one server at a time, no concurrent build/research. GGUF residency must be at least 99%; use identical request bytes, compiler mode and sampling. Record startup, loaded-library maps, client full-response time, prefill/decode, RSS, GPU memory, process swap and system swap-out. The first candidate response is the correctness gate before any performance claim. Stop on a wrong response, OOM, hang, lost residency or swap-out; do not silently count failed requests.

## Acceptance criteria

All full content hashes and 512-token sequences match the fixed reference. Compare control/candidate medians and ranges for prefill, single-response decode, VRAM and RSS. Full speed parity requires decode >=47 tok/s and prefill >=2500 tok/s on the comparable control workload. Report partial recovery separately if those thresholds are not both met. No OOM, hang, process swap or additional full-weight copy. Increased pinned residency and any memory cost must be reported explicitly. Existing builds remain immutable; any accepted default is present in new source builds, with only a disable override.

## Results: all approved runs completed

The four server lifecycles ran sequentially from 23:38:21 to 23:44:45 on 2026-11-16, with no concurrent build. Each ran the exact saved 32768-token input / 512-token output request, seed 123, unchanged sampling and fixed model profile. Model residency was 100% before every lifecycle. Every one of the 12 full content hashes and token sequences matched the saved reference. A per-response gate stopped the lifecycle on any mismatch; no mismatch occurred.

| Lifecycle | Decode, single-response tok/s | Prefill tok/s |
|---|---|---|
| A1 control | 42.13 / 43.25 / 43.11 | 1915.5 / 2228.8 / 2195.2 |
| B1 candidate | 57.05 / 51.76 / 57.53 | 2668.0 / 2489.4 / 2906.6 |
| B2 candidate | 55.10 / 53.67 / 55.94 | 2511.2 / 2808.0 / 2582.4 |
| A2 control | 36.43 / 43.06 / 37.61 | 1942.8 / 2085.8 / 1952.8 |

The medians include all six responses per arm; no slow response was excluded.

| Axis | A control median | B candidate median | Change |
|---|---:|---:|---:|
| Decode tok/s | 42.5927 | 55.5176 | +30.35% |
| Prefill tok/s | 2019.2837 | 2625.2066 | +30.01% |
| Process RSS MiB | 21345.75 | 21350.90 | +5.15 MiB (+0.024%) |
| Total GPU usage MiB | 11387.5 | 11468.0 | +80.5 MiB (+0.707%) |

GPU values are total-device snapshots including desktop/services, not an isolated allocation delta. RSS values are after the third request in each lifecycle. Decode ranges: A 36.4260–43.2507, B 51.7606–57.5273. Prefill ranges: A 1915.5353–2228.8004, B 2489.3719–2906.5786. One candidate prefill was slightly below 2500; both candidate lifecycle medians and the combined candidate median exceeded 2500. All candidate decode values exceeded 47.

The supplied historical reference (47.34 median decode, 2508.6 median prefill) was not re-run in this block; no new paired superiority claim against that binary is made. The candidate passes the original speed thresholds on the identical control workload and improves the directly paired independent control by about 30% on both speed axes. Two server lifecycles per arm were measured; request samples within a lifecycle are correlated, so this is not a claim of six independent server-level samples or a formal confidence interval.

## Mechanism and memory evidence

A separate, model-free 4 KiB hardware probe passed all three cases: control default false, candidate default true, candidate explicit disable false. The source patch restores the reference's existing policy, not a new model/precision path.

During startup `nr_foll_pin_acquired` increased by 3889 pages on both A lifecycles and 4765609 pages on both B lifecycles. The difference is about 18.1645 GiB of additionally pinned pages with 4 KiB pages. These are existing resident mappings becoming unreclaimable while registered; no second full-weight copy was added. Pinning does have a memory-management cost: the OS cannot reclaim those pages until the registration is released. The core's existing residency/headroom/rollback guards are unchanged.

The global acquired-minus-released pin balance was 4102 before each of the four lifecycles and returned to 4102 after final shutdown. Thus the observed extra pins were released across both candidate restarts. Process swap was 0 and system swap-out delta was 0 in every lifecycle. No OOM, hang, allocation error or failed request occurred. Final port 9101 was free and no benchmark server remained alive.

## Artifacts and acceptance

- Only `libggml-cuda.so.0` differs between A and B. All other eight executable/DSO files match byte-for-byte.
- Candidate CUDA SHA-256: `17e79510f37817400c1b268e401d16eb2f392d1ef7f3da979d5ccf975cb32240`.
- Core SHA-256 remains `4eba824255f06ab1fb8e5b6a9e98ae7bd0706773abc8bf86547d86b60cde98ac`.
- Full response SHA-256: `c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f`.
- Accepted package: `/home/end0/AI/LlamaServer/apex-independent-fast-20261116`.
- Control and reference sets remain unchanged. The accepted package is an independent byte copy of the measured candidate, verified before handoff; its ldd resolves all runtime libraries inside its run directory.
- The source default is enabled in new builds. Existing `GGML_CUDA_REGISTER_HOST=0` disables registration, and `GGML_CUDA_NO_PINNED` retains the reference fallback behavior when no explicit registration override is present.
- Compiler flags, CUDA arithmetic, Goose core, quantization, model, context, sampling and thread placement were unchanged. No C++ migration/removal or other optimization was performed.
- Validation scope remains the fixed Apex profile on this RTX 4070. No claim is made for other models/profiles. Embedded UI remains outside this source-built package; API/control generation was tested, and a live Russian chat was already verified in EXP905 with the unchanged core/server path.

Build reproduction uses `build.py` with the support source-build directory, immutable control directory and a NEW output directory. `provenance.json` records the reused source-built objects, hashes and exact compile/link commands. The full independent source build also inherits the corrected default from vendor/flint.

Manual launch, same profile as the reference:

```bash
bash /home/end0/AI/LlamaServer/apex-independent-fast-20261116/launch-apex.sh
```

The launcher refuses an occupied test lock/port and never terminates another process. Both old control and new accepted binary sets remain available for rollback. No automatic next experiment is started.

## Preparation cost and latency

Host registration is performed during startup, outside the reported per-request prefill/decode windows; it is not free preparation. The model-loaded log timestamps and full client response durations are recorded in measurements.json. Client durations include complete responses and must not be called TTFT. No TTFT measurement is claimed.

A: model-loaded timestamps [2.557814, 2.515309] seconds; median client full-response duration 28.434238 seconds.

B: model-loaded timestamps [3.951309, 4.386727] seconds; median client full-response duration 21.558131 seconds.
