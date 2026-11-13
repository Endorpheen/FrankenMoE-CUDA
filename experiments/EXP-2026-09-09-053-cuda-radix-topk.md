# EXP-2026-09-09-053: CUDA radix top-k for long-context decode

Status: BUILD READY; correctness and performance pending independent testing. No tests or model runs performed in this session.
Kind: isolated implementation, pending independent correctness and model A/B.
Owner: Igor. Implementation: senior helper in Codex. Testing: a separate agent selected by Igor.

## Hypothesis recorded before implementation

Replacing full argsort plus index copy with radix selection for wide CUDA TOP_K rows can improve Qwen3.8-Flash-Next sustained decode at an actually populated long context. Practical acceptance target: at least 2% paired decode tok/s improvement with correctness preserved. This is a usefulness threshold, not a forecast.

The QSA indexer invokes TOP_K during generation. A large configured context alone does not exercise wide rows: the request must populate the context. The old short EXP-046 prompt is a short-context regression control, not the primary performance workload.

## Baseline and one variable

- Accepted runtime and launcher remain unchanged: `build/exp046-default-runtime` and `scripts/run_qwen38_server.sh`.
- Source baseline: exact working-tree copy of `work/llama.cpp-exp046-default`, upstream HEAD `4aaad5d318a790a42c2197975ec8fadbad42602b` plus its accepted local chain. HEAD alone does not describe this dirty source tree.
- Candidate source/build: `work/llama.cpp-exp053-radix-topk`, `build/exp053-radix-topk`.
- One behavior variable: wide-row CUDA TOP_K selection. A: `GGML_CUDA_TOPK_ARGSORT=1`; B: `GGML_CUDA_TOPK_ARGSORT=0` or unset. Read once per process; restart between arms.
- Keep fused IQ2_S, two-thread gather, pinned ring, MTP n_max=2, threads=12, checkpoint policy, quantization, KV q4_0 and EHS=0 unchanged. No EXP-052 graph variants.
- Native DeviceTopK takes precedence when available; HIP/MUSA and narrow-row fallback remain unchanged in this port.

## External evidence and provenance

Implementation source: Inovello/llama.cpp, branch `flashnext-2x3090`, pinned commit `dd64a3db0c453b0e03de15574ff874c2dc77cb28`, `ggml/src/ggml-cuda/top-k.cu`. The external implementation builds on the radix selection associated with llama.cpp PR #27466. Related CUDA proposal: PR #28366. Preserve MIT licensing and attribution; this is a local adaptation, not a claim of original authorship.

- https://github.com/Inovello/llama.cpp/blob/dd64a3db0c453b0e03de15574ff874c2dc77cb28/examples/flashnext-topk/README.md
- https://github.com/ggml-org/llama.cpp/pull/28366
- https://github.com/ggml-org/llama.cpp/pull/27466

The author's reported 9-12% decode gain concerns Flash-Next at about 119k populated context, two RTX 3090s, DDR4, CUDA 12.0 and MTP-3. It is not a local prediction and is not a benchmark of the exact upstream PR head. The local build uses CUDA 12.6 / CUB 2.5 and the old argsort fallback.

## Risks and pending gates

- Unordered output and arbitrary choices among equal cutoff scores are legal TOP_K behavior; model parity still requires explicit checks. Do not dismiss output changes as harmless without analysis.
- Test negative values, signed zeros, masked negative infinities, repeated values, k=1 and k=ncols, 8191/8192/8193-column boundaries, QSA k=2051, rows=1/2/3 and prompt-like row counts.
- NaN input has no useful attention-ranking meaning; do not silently claim NaN equivalence with the previous sorting implementation.
- Scratch allocation, stream ordering, repeated CUDA graph execution and memory safety must be checked by the tester.
- Radix scans and atomics may lose on short rows or different GPUs. The inherited 8192 threshold is not tuned for RTX 4070.
- No correctness PASS, performance result, baseline promotion or test completion is claimed by compilation alone.

## Implementation and build outcome

- Separate source copy completed. Static directory comparison against the accepted source, excluding `.git`, found exactly two changed files: `ggml/src/ggml-cuda/top-k.cu` and `tests/test-backend-ops.cpp`.
- Runtime port: radix helpers, inherited 8192-column threshold, integer arithmetic bounds, process-cached control switch, optional dispatch diagnostics, post-launch error check. No new stream synchronization or dependency changes.
- Added 182 cases to the existing TOP_K suite. These cases were compiled, NOT executed.
- Clean CMake configure EXIT=0. Combined build of `llama-server` and `test-backend-ops` EXIT=0. CUDA sm_89, Release, native CPU, CUDA graphs ON; CUDA compilation flags match the accepted build. The new top-k translation unit compiled successfully and radix symbols are present in the resulting CUDA library.
- Build warnings: two missing-field-initializer warnings for `set_tensor_async_pinned_ring` in unchanged baseline backend files; no compilation errors. No warning is presented as a runtime correctness result.
- Server RUNPATH and CUDA-library RUNPATH point to the isolated EXP-053 `bin` directory. This was inspected with readelf, not by starting the binaries.
- Accepted server, accepted CUDA library and launcher SHA256 values remain identical to their pre-work values.
- Tests, model, server, sanitizer and profiler: NOT RUN, as explicitly requested by Igor. No speedup or correctness PASS claimed. No commit, push or baseline promotion performed by this agent.

## Memory impact

Runtime memory is unmeasured. Scratch uses the existing device pool: about `nrows * (20 + 1024 * min(ceil(ncols/1024), 64))` bytes before allocator rounding, about 192 KiB for 3 rows at 131072 columns or 32 MiB for 512 rows. No new pinned ring or expert cache. Lower scratch usage is shape-dependent, not guaranteed against every old chunked-sort case.

## Artifacts and next step

- [Manual tester instructions](../results/archive/EXP-2026-09-09-053/TESTING.md).
- [Provenance and local adaptations](../results/archive/EXP-2026-09-09-053/PROVENANCE.md).
- `results/archive/EXP-2026-09-09-053/cuda-radix-topk.patch` and `topk-test-cases.patch`: reproducible delta against the accepted source snapshot, not installed in the default patch chain.
- `results/archive/EXP-2026-09-09-053/configure.txt` and `build.txt`: full compiler output.
- `results/archive/EXP-2026-09-09-053/SHA256SUMS`: exact source, patch and build artifact hashes; paths are relative to the project root.
- Candidate server SHA256: `b19ac6a18ae13a0b7acab298baa6b86d0e45204612d101c7e266e5837d4fc329`.
- Candidate CUDA library SHA256: `0933c18b17faa184ba79bb8af18484caf2af3f4fcefec3d6470181e168c4f358`.
- Next action belongs to Igor's tester: model-free correctness, followed only with approval by diagnostic long-context validation and a preliminary paired decode A/B. The actual meaningful long document must be selected and frozen before those performance runs.

## Results (2026-09-10, read/run by OpenCode)

Model-free correctness (`build/exp053-radix-topk/bin/test-backend-ops test -b CUDA0 -o TOP_K`):
- control `GGML_CUDA_TOPK_ARGSORT=1`: 340/340 `path=argsort`, 0 FAIL, exit 0;
- candidate `GGML_CUDA_TOPK_ARGSORT=0`: 340/340 `path=radix`, 0 FAIL, exit 0;
- both arms: `2/2 backends passed`. SHA256SUMS verified before the run.
The wide-row branch was really selected (ncols up to 196608, rows 1/2/3/128) and
the candidate matches the CPU reference.

Decode A/B (same binary, MTP n_max=2, exact EXP-046 profile, one fixed long
prompt, warmup then a 256-token chat request, temperature 0):

| test | control tok/s | candidate tok/s | paired change | control/candidate acceptance | notes |
| --- | --- | --- | --- | --- | --- |
| prompt 17899 tokens | 19.441 | 20.215 | +3.98% | 0.819 / 0.796 | outputs diverged: control emitted reasoning only, candidate emitted reasoning + answer |
| prompt 11018 tokens | 20.882 | 20.933 | +0.24% | 0.847 / 0.828 | both reasoning-only; within noise |

## Verdict

REJECTED as a speed candidate. The only positive pair (+3.98%) had divergent
outputs and was not confirmed; the clean small-context pair is +0.24%, well
below the >=2% target. A ~119k populated-context run, where the radix selection
was expected to matter most (Inovello reports +9-12% at ~119k on 2x RTX 3090),
was not completed because prefill of that prompt exceeded a practical budget.
The correctness gate passed and the implementation is preserved; the result is
a null decode effect at the contexts actually measured, not a correctness
failure. The 8192-column threshold is inherited and not tuned for the RTX 4070.
