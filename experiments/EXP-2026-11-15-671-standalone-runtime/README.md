# EXP-2026-11-15-671 — the single Goose runtime links and runs on its own

Base: `8005e98b`. No model, no server, no GPU. Build and smoke only.

## 1. What this establishes

The migration had been proceeding by editing llama.cpp carrier patches, which
kept the shipped executable a C++ program with Goose inside it. This experiment
takes the other road: the composed Goose unit plus its host adapter, linked with
nothing but libc, libm and pthread.

Result:

```
bash tools/build-standalone-runtime.sh --check
  binary: builds/goose-standalone/frankenmoe-standalone (4 276 288 B)
  deps:   linux-vdso.so.1 libm.so.6 libc.so.6
  run:    FG_STANDALONE init_runs=1 iq_bound=0 types=43 blck_q4k=256 \
          cfg_rule=1 tool_read_file=0 mime_id=0 mime=text/html; charset=utf-8
  standalone runtime: ok (single Goose runtime, libc/libm/pthread only)
```

No `libstdc++`, no `libggml`, no `libllama` in `ldd`. The two `ggml_`-looking
symbols in `nm` (`qt_ggml_quantize_requires_imatrix`, `sys_ggml_plan`) are Goose
functions named after the code they replace, not C++ code.

## 2. What is inside

| input | role |
|---|---|
| `build/goose-main/main.c` | the composed unit, 110 modules, 998 globals, 99 externs |
| `integration/m1/host/goose_host.c` | the host primitives: fp16 conversion, bit views, JSON/locale helpers, one `pthread_once` |
| `integration/goose-standalone/smoke.c` | the smoke host that exercises four real entry points |

The 99 externs of the unit split exactly as EXP664 recorded: 65 Goose helpers
supplied by the adapter and 34 libc/POSIX symbols. The adapter closes all 65, so
the link needs no other object.

## 3. Why these four calls are the right smoke

Each one crosses a different boundary, so a link that happens to succeed but a
runtime that is not wired fails at least one:

| call | what it proves |
|---|---|
| `abi_type_count()` → 43 | the ggml type table is bound and reachable |
| `abi_blck_size(12)` → 256 | arithmetic over that table answers correctly (Q4_K = 256) |
| `cfg_validate(empty)` → 1 | the 48-rule validator runs and refuses an empty config with rule 1 |
| `tool_index("read_file")` → 0 | the server tool table is present and indexed |
| `mime_of`/`mime_text` → `text/html; charset=utf-8` | a two-step slice-returning export works end to end |

`init_runs=1` is the invariant the whole design rests on: one `goose_init` per
process, whichever path reaches it first.

`iq_bound=0` is expected here and is not a defect: the IQ table descriptor is
installed by the CPU provider in `ggml-quants.c`, which this standalone link does
not include. A unit test that needs the IQ views must bind a provider first, and
the seam aborting loudly is exactly what EXP661 proved.

## 4. What this does not do yet

1. It is **not** the Apex server. It is the runtime core with a smoke host: no
   model loading, no HTTP, no GPU backend, no KV cache.
2. The Apex GPU path still lives in the existing binary, which is untouched. The
   standalone runtime has no CUDA backend because the composed unit does not own
   one yet.
3. `iq_bound=0` as above.

## 5. Next, in order

1. Grow the smoke host into the real entry point: load a GGUF through
   `goose-model-loader`, build a graph through `goose-model-graph`, serve through
   the server modules that are already in the unit.
2. Give the runtime a backend seam for the Apex GPU path, keeping the existing
   binary as the reference until the new one passes the same request.
3. Only then exclude the C++ runtime from the shipped build, in one change.

No performance claim is made, and no model was started.
