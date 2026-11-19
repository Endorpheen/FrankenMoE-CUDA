# P10-10 wiring: the exact call sites

Written 2026-11-19 while a candidate build was running, so the edit that follows is small and its
effect is known before it is made. Three Goose decisions answer three questions that ggml answers in
C++ today.

## 1. CPU feature scoring

`vendor/flint/ggml/src/ggml-cpu/ggml-cpu.c`

| line | function | what the Goose decision replaces |
|---|---|---|
| 3649 | `ggml_cpu_has_avx_vnni` | whether the built variant may advertise VNNI |
| 3665 | `ggml_cpu_has_avx512` | the same for AVX512 |
| 3697 | `ggml_cpu_has_amx_int8` | the same for AMX int8 |

The Goose side is `goose/cpu/cpu-features.goose` (`feat_*`), which decodes the feature words and
scores a variant. The wiring is one call per function: ask the Goose decision first and keep the
native branch as the fallback for a build where the door is absent, exactly as `server-http.cpp` does
for `key_authorize`. Only AVX2 exists on this host, so the gate can compare one variant against the
native result and must say the rest are compile-only - which `P10-10-PLATFORMS-STATE.md` already does.

## 2. Backend selection

`vendor/flint/ggml/src/ggml-backend-reg.cpp`

| line | function | what the Goose decision replaces |
|---|---|---|
| 229-231 | `load_best` / `dl_get_sym("ggml_backend_score")` | rejection of a backend whose score is 0 |
| 480, 504 | `ggml_backend_load_best` | the "pick the highest score" loop |

The Goose side is `dev_*` in the same module (registry order, name matching, fallback). The registry
is the right place because it is where the question is actually asked, and it is the only place where
the answer changes a decision rather than a diagnostic.

## 3. Platform paths

`plat_*` (separators, cwd, executable prefix, temp dir) are asked by `common/` helpers. They are
transcription-only on this host for Apple and Windows, so wiring them buys nothing that can be
verified here: a transcription of an unsupported platform is not a test of it.

## Order and rollback

1. feature scoring first, because it is the one variant the gate can compare against native;
2. backend selection second, verified by a server start that names the chosen device;
3. platform paths not at all, and the reason recorded rather than a call added.

Each step is its own commit with the native branch kept, so removing it is a one-line revert until the
Goose answer is proven equal on the one comparable variant.
