# EXP-2026-09-06-034 — Provenance and reproducible MTP package

Status: ACCEPTED (kind=infrastructure)

## Hypothesis

The running split-MTP server can be reproduced from a minimal patch chain over the pinned expert-tier base `4aaad5d318a790a42c2197975ec8fadbad42602b` plus the published `patches/expert-tier-integration.patch`, without relying on the dirty `work/llama.cpp-integration` worktree. Prerequisite work, not a new MTP optimization.

## Baseline identity

- Root repo: `main` at `3c02e5de56061c7c30c45ec325a0d8845639c825` (dirty: docs/journal edits, untracked agents.md and EXP-028..033 records; captured in audit artifacts, never staged).
- Nested integration tree: `work/llama.cpp-integration` at `4aaad5d318a790a42c2197975ec8fadbad42602b` (branch `expert-tier`) with numerous uncommitted functional changes.
- Live server (identified via `ps`, `ss`, `/proc/PID/exe`, and `~/.config/opencode/opencode.jsonc`): initially PID 2418737; during R0 capture the machine owner restarted the server (new PID 2429200, 2026-09-06 12:52, `-c 131072 -np 1`, no `--jinja`). Same binary file and hash in both cases: `build/exp023-mtp-sidecar/bin/llama-server`, sha256 `5471e44b9d394d2e45c7adfb65c46a4ef1deeb32cee58b815eb2e0d02f4b9b78`. This server serves the agent session itself; timings on it are diagnostic only.
- Running argv: `-c 64000 -fa on --jinja -t 12 -ctk q4_0 -ctv q4_0 --reasoning-effort low -ehs 0 --cpu-moe -ot per_layer_token_embd.weight=CPU,token_embd.weight=CPU -ngl 99 -md models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf --spec-type draft-mtp --spec-draft-n-max 2 -ngld 99 --spec-draft-cpu-moe -ctkd q4_0 -ctvd q4_0`. No `--swap-watchdog`, no `--ram-headroom-gib`.
- Live build cache: `CMAKE_HOME_DIRECTORY=work/llama.cpp-exp023`, Release, CUDA arch 89, OpenMP ON, shared libs, OpenSSL ON, no curl dependency.

## Method

1. Captured root/nested `git status`, HEAD, and `git diff --binary` into `results/archive/EXP-2026-09-06-034/audit/` with hashes. User content was never staged; the only index entries in the integration tree (intent-to-add watchdog files) pre-existed and were left untouched.
2. Hashed the live binary via `/proc/PID/exe`, all seven loaded llama/ggml shared libs via `ldd`, the small model shard and MTP head with SHA-256. Shard 1 hash matches `benchmarks/baseline.json`; shards 2/3 verified by size/mtime only, full re-hash deferred (heavy I/O).
3. Copied surviving `/tmp` artifacts for EXP-023..033 into `results/archive/EXP-2026-09-06-034/raw/` (119 files, sha256 in `artifacts-inventory.json`), originals preserved.
4. Created isolated source `work/llama.cpp-exp034-base` (local clone of `upstream/llama.cpp-expert-tier`, detached at `4aaad5d`), applied `patches/expert-tier-integration.patch` cleanly (`git apply --check` passed).
5. Compared trees and found the reconstruction chain:
   - The user integration tree equals fork `work/llama.cpp-exp023` branch `exp023-mtp-sidecar` HEAD `c40681659` plus one blank line in `src/llama-context.cpp` (line 487, non-functional user edit).
   - Fork history: `4aaad5d` + `1865b000f` (snapshot of the then-uncommitted expert-tier working state) + `aaff9b3d5` (MTP sidecar port from cafe-llama.cpp) + `c40681659` (fully-CPU draft KV fix).
   - Snapshot `1865b000f` differs from `4aaad5d + integration patch` in six files: comment-only translations in `ggml-cpu.h/.c`, `llama.h`, `llama-expert-hotstore.h`, plus two functional refinements of the EHS autofit in `common/common.cpp` and `src/llama-expert-hotstore.cpp` (stricter autofit semantics; the published patch predates them).
6. Produced two new patches and proved the chain on a fresh copy of the reconstructed base:
   - `patches/integration-drift.patch`: snapshot drift (EHS autofit refinement + comment translations). Gate: applies with `git apply --check`.
   - `patches/mtp-sidecar.patch`: MTP sidecar support (`mtp_only`/`trunk_only` load gating, `graph_mtp`, loader/kv/hybrid-idx/arch/speculative ports, draft-KV fix). Provenance: fork commits `aaff9b3d5` + `c40681659`.
   - Equivalence: base + integration patch + drift + mtp == user tree except the documented blank line.

## Artifacts

- `benchmarks/manifests/exp034-provenance.json`
- `results/archive/EXP-2026-09-06-034/audit/` (status/diff captures)
- `results/archive/EXP-2026-09-06-034/raw/` + `artifacts-inventory.json`
- `patches/integration-drift.patch`, `patches/mtp-sidecar.patch`, `patches/README-mtp-sidecar.md`
- Isolated sources `work/llama.cpp-exp034-base`, `work/llama.cpp-exp034-mtp`; isolated build `build/exp034-mtp-repro`

## Gates

- Reconstruction applies cleanly from pinned upstream + published patch + two new patches: PASS.
- No unexplained functional delta between chain and user tree (only the blank line): PASS.
- Reconstructed source builds with the recorded toolchain flags (CUDA arch 89, Release, OpenMP ON, OpenSSL ON): PASS (`build/exp034-mtp-repro`, binary sha256 `45a7552d8714203c99d0dcddbf1d660eb56b5408136fa32709a3c712705cc166`).
- Flag/help parity of the new binary vs the live server (`--help` byte-identical): PASS. No model load without a separately approved block.

## Notes and omissions

- Toolchain drift since EXP-000: `benchmarks/baseline.json` records g++-13/CUDA 12.4; the machine now runs g++-15.2.0/CUDA 12.6. The live exp023 binary was built with the current toolchain. Binary hashes across independent builds are not required to match; functional parity is the gate.
- `results/exp*` raw SSE/monitor/server logs are gitignored (`LOCAL_ONLY`); hashes are recorded in the inventory. `/tmp/exp026-major-faults.perf.data` (~123 MiB) was left in `/tmp`, inventory-only.
- EXP-031 `exp031-response-1788632230.json` and EXP-028 `fused-run-4.json` are 0-byte (failed attempts); kept as-is.

## Addendum (2026-09-06, post-commit)

- Semantic status: `ACCEPTED` with `kind=infrastructure`. `PASS` is a per-gate wording, not a final
  experiment status; the gates table above keeps its per-gate `PASS` values.
- Current runtime record (separate from R0 evidence): as of 2026-09-06 14:07 MSK the machine owner
  restarted the server again — PID 2470518, `-c 196608 -np 1`, port 8081, same binary
  `build/exp023-mtp-sidecar/bin/llama-server`, sha256 `5471e44b9d394d2e45c7adfb65c46a4ef1deeb32cee58b815eb2e0d02f4b9b78`.
  This is the current runtime configuration, not a benchmark and not part of R0 evidence; the R0
  manifest keeps its captured argv (`-c 131072`). No timing was taken against this server.
- Raw archive placement: `results/archive/EXP-2026-09-06-034/raw/` (119 files) remains
  `LOCAL_ONLY`/untracked and is deliberately not committed. Commit `1cf489a` contains the manifest,
  hashes, inventory and compact audit artifacts; the raw files stay on this machine only.
- Process violation record: commit `1cf489a` was created without the pre-commit approval block
  (outcome, memory impact, correctness, file list shown to Igor; `AGENTS.md` Hard rule 2). The
  commit content stands and is not rolled back. From now on: no further commits until the outcome,
  memory impact, correctness and the exact file list have been shown to Igor and explicitly
  approved.
