# EXP-2026-09-07-041 — Default-on pinned ring as the daily runtime (R7)

Status: `ACCEPTED` (`kind=delivery`, commit approved by Igor 2026-09-07; model-free validation complete;
model smoke NOT run by the agent — handed to the senior helper in Codex)

## Hypothesis (one)

If the accepted EXP-038 pinned-ring patch is delivered through the reproducible root patch
chain, the ring is enabled by default, and the daily launcher points at a new clean build,
then the main Qwen model always uses the accepted accelerated prefill path without manual
`GGML_EXPERT_PINNED_RING=1`.

## What is NOT done here

- No repeat of the R4/R6 performance A/B; no parallel or scripted measurements.
- `work/llama.cpp-integration` untouched; `build/expert-tier-franken-cuda` never rebuilt.
- EHS stays off by default; THREADS=12, MTP n_max=2, `-np 1` preserved; no multi-slot/P10.

## Delivery chain (reproducible, over pinned base `4aaad5d318a790a42c2197975ec8fadbad42602b`)

Order and sha256 of the patch chain (all applied to `work/llama.cpp-exp041` with
`git apply --check` followed by `git apply`):

1. `patches/expert-tier-integration.patch` `1359f23095ca…`
2. `patches/integration-drift.patch` `b9d8db5d772b…`
3. `patches/mtp-sidecar.patch` `8873d64da544…`
4. `patches/pinned-ring.patch` `e1e6803cec1e26099ee452fcb3537b7e660132e6e30da69febffca4785dbd7de`
   (byte-identical copy of the accepted `results/archive/EXP-2026-09-06-038/exp038-pinned-ring.patch`)
5. `patches/pinned-ring-default-on.patch` `73ca9f558e12…` — minimal default-on change, only the
   two `GGML_EXPERT_PINNED_RING` gates in `ggml/src/ggml-cuda/ggml-cuda.cu` and
   `ggml/src/ggml-backend.cpp`: unset -> ring on; `1` -> on; `0` -> off; allocation failure
   still disables the ring permanently for the context and falls back to the pageable path.

## Build (new directory, never in place)

`cmake -S work/llama.cpp-exp041 -B build/exp041-default-runtime -DCMAKE_BUILD_TYPE=Release
-DBUILD_SHARED_LIBS=ON -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89 -DLLAMA_CURL=OFF
-DLLAMA_OPENSSL=ON -DLLAMA_BUILD_SERVER=ON`; builds clean.

Artifact sha256 (build/exp041-default-runtime/bin):
llama-server `8489fbe5a458916c5cdbfb6ccce04d7982d6e95afbeaa340c60f65e8703b5050`,
libggml-cuda.so `78cc19486cb368d2433bc844b6acde223118f64896ce1d39e695fa5d20febe26`,
libggml.so `5a96d3c323273bd693081d112216d84ed06d47541cc7a1e28be13b7801e18659`,
libllama.so.0 `df26a4b20807f468eb44e184f09da4dd9d741e3fc8bcdf0e3da5fb54b872e00a`,
libllama-common.so.0 `d30ebf134cb6c1fcdff3d5f799dcf04157afca7bf59c3639a38adce10c1554cf`,
libllama-server-impl.so `1b79cc228443809dcea54f9b799160b180841d89626513d5f3ce9e42bfd97c08`.

## Model-free validation (2026-09-07, no server, no model)

1. Full patch chain: `git apply --check` + `git apply` clean in order 1-5 on a fresh clone of
   the pinned base. PASS.
2. Default-on patch: applies cleanly on top of the after-ring state; only the two env gates differ. PASS.
3. Clean build of `build/exp041-default-runtime`. PASS.
4. The 12-check EXP-038 model-free harness adapted to default-on semantics
   (`results/archive/EXP-2026-09-07-041/tests/`, `run-model-free-exp041.sh`), run against the new
   build in 8 modes (ring: default/ON/OFF/alloc-fail; sched: default/ON/OFF/alloc-fail): all
   CHECK PASS. Default (no env) and explicit ON show ring init and non-zero staged counters;
   `GGML_EXPERT_PINNED_RING=0` shows zero ring use and no init; alloc-fail logs the fallback and
   stays on the pageable path. PASS.
5. Output equality: sched output sha256 `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510`
   identical across default/ON/OFF/alloc-fail and identical to the EXP-038/EXP-040 candidate —
   byte-identical results to the accepted runtime. PASS.
6. `bash -n scripts/run_qwen38_server.sh` clean. Stub-server run confirms the launcher passes the
   full validated profile: `-c 196608 -np 1 -fa on --jinja -t 12 -ctk q4_0 -ctv q4_0 -ctkd q4_0
   -ctvd q4_0 --reasoning-effort low -ehs 0 --cpu-moe -ot
   per_layer_token_embd.weight=CPU,token_embd.weight=CPU -ngl 99 -md …/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf
   --spec-type draft-mtp --spec-draft-n-max 2 -ngld 99 --spec-draft-cpu-moe`; default
   `GGML_EXPERT_PINNED_RING=1`, `PINNED_RING=0` passes `GGML_EXPERT_PINNED_RING=0`. PASS.
7. `work/llama.cpp-integration`: no file newer than this experiment's artifacts; untouched. PASS.

## Model smoke (NOT run; Igor decides when and who runs it)

Purpose: confirm the new default-on daily runtime end to end. Planned: 1 server start,
2 sequential requests (short + long), about 2-3 minutes. Expected: ring activation logged with
no manual env var; short/long answers match the accepted EXP-040 hashes (short content sha256
`2f409a00f774aeacfa1d8b92ac8ff58ed19f1a73ad7d184bbb625f477605a5aa`; long: single EOG, empty
content); staged calls/chunks non-zero; VmSwap=0; no CUDA/OOM; clean SIGINT; no orphans.
Correctness/delivery smoke only — no performance claims, no A/B repetition.

## Verdict

ACCEPTED (`kind=delivery`). Commit approved by Igor on 2026-09-07. All model-free gates PASS.
Model smoke pending — handed to the senior helper in Codex (plan and expected hashes above).
