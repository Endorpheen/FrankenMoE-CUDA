# REPORT — P10-10 cross-platform slices

Date: 2026-11-15
Base commit: `aa9109b5`
Native tree: `upstream/llama.cpp` (same commit)
Production code changed: none — no model, no GPU, no device started
Working tree: `/tmp/opencode/sub-649`

## Result

| item | result |
|---|---|
| slices | 3 (`feat_`, `plat_`, `dev_`) |
| gate checks | 384, 0 failures |
| gate at `-O1` vs `-O0` | identical output for all three slices |
| mutations | 36/36 caught (12 per slice, halves 6/6) |
| real-native cross-checks | ARM64 decode vs the real `ggml-feats.h` (16 cases, 227 field checks); x86 score vs the real `ggml_backend_cpu_x86_score()` for 8 flag words |
| composed with the canonical unit | typecheck ok, 696 global symbols, 0 duplicates |

## What is covered

- **S1 `feat_`** — `ggml_feats_get_arch64_runtime` on all three platforms, the
  aarch64 and x86 dynamic-backend score chains, the kleidiai feature mask and
  feature-name branch order.
- **S2 `plat_`** — `DIRECTORY_SEPARATOR`, `fs_get_cache_directory`,
  `fs_get_config_directory`, `fs_get_cache_file`'s name rule, `common_set_env`,
  `set_process_priority` on POSIX and Windows, `tty_can_use_colors`.
- **S3 `dev_`** — name/type device lookup, `init_best`, the `load_best` score
  rule, the `[lib]ggml-<name>-*.[so|dll]` candidate test, `get_executable_path`,
  the search-path order and the registration/duplicate rules.

## What remains with the caller

Every platform call: `getauxval`, `prctl`, `sysctl`, `IsProcessorFeaturePresent`,
`getenv`, `getpwuid`, `setpriority`, `SetPriorityClass`, `isatty`, `dlopen`,
directory iteration and directory creation. The slices decide; they do not
perform. This is the integration boundary for the single runtime.

## Not proven here

- Apple and Windows decodes are transcriptions: no `sysctl.h`, no `windows.h` and
  no such host. Recorded as hardware-unverified, never as tested.
- The ARM64 accept paths are exercised through injected hwcap values; no ARM64
  host ran this code.
- The x86 real-function cross-check sees this host's CPU, which reports
  `bits=63` (no AVX-VNNI, no AVX512, no AMX), so those blocks were validated on
  their reject path against the real function and on their accept path only
  against the transcription.

## Integration step

Copy the three modules to `goose/cpu/`, `goose/misc/` and `goose/server/`, append
three lines to `MODULES` in `tools/build-main-runtime.sh`, run
`bash tools/build-main-runtime.sh --repro`, then rerun `tests/build.sh all` and
`tests/mutate.py --module all` after the rebase on the current canon.

## Ledger proposal (for the lead)

`P10-10` stays `PARTIAL`. Suggested slice accounting for
`docs/goose-migration/tasks.json`:

- `slices_total`: 3 for this batch (CPU feature/score, platform path/env/priority,
  device/backend selection);
- `slices_done`: 3;
- `remaining`: NVFP4 software quantizer and its capability dispatch, the
  alternate GPU backends (Vulkan/ROCm/Metal/SYCL entry points), the ARM64
  kernel dispatch tables, Windows/POSIX thread and file-IO wrappers, and the
  device capability queries (`ggml_backend_dev_caps`) that still call a native
  backend;
- `coverage_doc`: this `README.md`.

## Risks

- The layer is policy-only: a caller that still performs the platform call can
  drift from the decision. The first host wiring must replace the call, not wrap
  it.
- `fs_get_cache_directory` is also a target of pending download-cache work; the
  two slices must be merged rather than added twice.
- The live service tree's `ggml-backend-reg.cpp` already carries a patch in the
  `load_best` search-path block; a rebase must re-read that block before the
  gate is trusted again.
