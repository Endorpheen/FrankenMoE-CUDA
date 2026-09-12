# EXP-053 implementation provenance

Date: 2026-09-09. No inference or test execution in this implementation session.

## Local baseline snapshot

- Root repository HEAD at start: `f2714a3`.
- Accepted source: `work/llama.cpp-exp046-default`.
- Upstream HEAD: `4aaad5d318a790a42c2197975ec8fadbad42602b` plus the working-tree accepted patch chain; this is not a pristine upstream checkout.
- Isolated copy: `cp -a --reflink=auto work/llama.cpp-exp046-default work/llama.cpp-exp053-radix-topk`. Destination did not exist. The copy includes the source Git metadata but does not share mutable source files through hard links.
- Source comparison excluding `.git`: only `ggml/src/ggml-cuda/top-k.cu` and `tests/test-backend-ops.cpp` differ.
- Existing user edits to AGENTS.md/CLAUDE.md and the concurrent documentation translation are outside this implementation; they are not reverted, staged or committed by this agent.

Accepted artifact SHA256 values before work:

```text
3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec  build/exp046-default-runtime/bin/llama-server
592592566b115f76bb8f311537b928fd5a929906973378d063e3b9384338164d  build/exp046-default-runtime/bin/libggml-cuda.so.0
17fa4ebb9d4a935dd8afd63683c4df99fc7c90d2f1eff0d8cc2d2e9170a8eacc  scripts/run_qwen38_server.sh
```

## External source

Inovello/llama.cpp commit `dd64a3db0c453b0e03de15574ff874c2dc77cb28`, file `ggml/src/ggml-cuda/top-k.cu`, retrieved from the pinned raw GitHub URL. Its radix helpers are derived from the llama.cpp radix work associated with PR #27466. The surrounding project is MIT-licensed. Retain upstream license and attribution. No upstream PR or comment is submitted.

- https://github.com/Inovello/llama.cpp/blob/dd64a3db0c453b0e03de15574ff874c2dc77cb28/ggml/src/ggml-cuda/top-k.cu
- https://github.com/Inovello/llama.cpp/blob/dd64a3db0c453b0e03de15574ff874c2dc77cb28/LICENSE
- https://github.com/ggml-org/llama.cpp/pull/27466

Local differences from that external file:

1. CUDA old-CUB branch only; preserve EXP-046's HIP/MUSA/native DeviceTopK behavior.
2. Keep the 8192 threshold and nonzero `GGML_CUDA_TOPK_ARGSORT` kill-switch, reading the environment once with thread-safe static initialization.
3. Add upper bounds for 32-bit grid/stride arithmetic and k validity before radix dispatch.
4. Add a compile-time radix-bin/thread-count assertion and a post-launch error check, without stream/device synchronization.
5. Add optional `GGML_CUDA_TOPK_DIAG` host-dispatch logging; it is disabled for timing.
6. Add 182 deterministic extra cases to the existing operator suite, including index bounds, ties, masking and actual QSA k. Tests remain unexecuted.

The algorithm performs four 8-bit histogram/select passes, then gathers indices above the cutoff and enough equal-cutoff indices to produce exactly k entries. Output ordering and tied index choice are unspecified, as in the TOP_K contract. CPU reference testing and model-level parity are still required.

Temporary storage requested from the existing CUDA pool is approximately `nrows * (20 + 1024 * min(ceil(ncols/1024), 64))` bytes, excluding allocator rounding/output. At 131072 columns and 3 rows it is 196668 bytes (about 192 KiB); at 512 rows approximately 32 MiB. This is not a universal memory reduction versus the old row-chunked sort, especially at the lower threshold and large row counts. There is no new persistent expert cache or pinned allocation.

## Reproducible delta

`cuda-radix-topk.patch` and `topk-test-cases.patch` are normalized `a/`/`b/` patches against the exact accepted source snapshot. Apply only to another isolated copy, never to the accepted work tree. For example, from that new copy use `patch -p1 --dry-run -i /absolute/path/to/cuda-radix-topk.patch` before applying. A successful patch application is a source-integrity check, not a correctness test.

Configure EXIT=0 and the combined server/test-backend-ops build EXIT=0. Full output is in `configure.txt` and `build.txt`; source/patch/binary SHA256 values are in `SHA256SUMS` (relative to the project root). Two warnings concern missing initializers in unchanged baseline backend files. A completed build is not a passed gate. Model A/B, activation, memory safety, graph replay and output checks belong to the tester.
