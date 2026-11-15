# EXP-2026-11-15-711 — decode gap: what is ruled out, and an important find in the reference

Base: `d9caa8be`. No new speed claim; this records eliminations and one correction to the
baseline itself.

## 1. The reference has two different runs, and they disagree

`runtime/exp244/results-apex-or244-or-goose/` contains both a `summary.json` and a
`response.json`, and they do not describe the run the baseline document quotes:

| source | prefill | decode | content sha |
|---|---:|---:|---|
| `summary.json` (EXP244 artefact) | 2500.05 | 24.51 | `e68df05b...` |
| `APEX-BASELINE-20261113.md` | 2228.76 | 41.62 | `9d90a010...` |
| our run | 2222.38 | 34.95 | `9d90a010...` |

`summary.json` also says `"reference": "Saved wholly-C++ correctness oracle"` and
`"preliminary": true`, with `cache_n: 0` - it is a correctness run without the GPU expert
cache, not a performance run.

**Our output hash matches the baseline table (`9d90a010`), not the EXP244 summary
(`e68df05b`).** So the comparison to 2228.76 / 41.62 is the right one, and the 2500.05 figure
belongs to a different configuration. This was worth pinning down: quoting 2500.05 as the
target would have sent the next attempts after a number that a different run produced.

## 2. Ruled out: the environment

Our profile is byte-identical to the reference's `env-actual.json` - the same 18
`GGML_GOOSE_*` values plus `GGML_EXPERT_HOTSTORE=0`. Nothing is missing there. The accepted
CPU library also knows `GGML_GOOSE_CPU_REGISTRY`, which neither profile sets; it is not the
difference.

## 3. Ruled out: GGML_BACKEND_PATH

Hypothesis: `ggml_backend_load_all` searches the executable's directory, and the executable
lives in `stage-run` where the slow `libggml-cpu-shared-*` also sits, so setting
`GGML_BACKEND_PATH` to our directory should pick the accepted build.

Measured: prefill **2006.50**, decode 35.27 - worse than 2222.38. With both CPU files already
the accepted build, registering the same backend twice appears to initialise it twice. The
variable is removed from `run-apex.sh` with a comment saying not to re-add it without a
measurement that beats 2222.

## 4. The decode gap is two silent modules

Per-module call counts, reference versus ours:

| module | reference | ours |
|---|---:|---:|
| `FG148_QUANT` | 482 400 | **absent** |
| `FG150_EXPERT` | 135 360 | **0** |
| `FG147_BACKEND` | 1 | 1 |
| `FG149_POOL` | 2 | 2 |

Both modules exist in our accepted library (`nm` finds 17 `fg14[78]`/`fg150` symbols) and
neither is called. `FG150_EXPERT` printing `calls=0` is not "not loaded" - it is loaded and
never entered.

That is where the remaining 16% of decode almost certainly lives: a hot per-token path that
exists but is not entered.

## 5. Next step, specific

Find what calls `fg150_expert_config` and `fg148_encode` in the accepted library and why the
entry condition is false. `nm` shows `fg150_expert_config` and `_ZL21fg150_registry_configv`
as local symbols, so the gate is inside that library and readable from its own disassembly -
not from the environment.

## 6. State

prefill 2222.38 (99.7% of 2228.76), decode 34.95 (84% of 41.62), output identical.
