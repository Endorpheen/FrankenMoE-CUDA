# EXP-2026-11-15-634 — P9-08: standalone engine closure (first three slices)

Three independent Goose slices for the parts of `runtime/core/src/engine` that
P9-05 (EXP632, session lifecycle/runtime facade/draft source), P9-06
(EXP602/572/573/575, session loop, CLI protocol, thinking, chat parse),
P9-01 (EXP620, config) and P9-07 (EXP627/628, metrics) left native.

- **Base commit:** `137bc40c` (task). Sources read at the same commit;
  `git status --porcelain` over `runtime/` is empty, so every line reference
  below holds.
- **Production code changed:** none. No model, no GPU, no benchmark was run.
- **Working tree:** `/tmp/opencode/sub-638` (outside the repository).

## Hypothesis

What is left of the standalone engine below the graph is decision logic: an
admission table around the model load, the arithmetic of a perplexity pass, and
the KV/turn bookkeeping of a request. Each can be replaced by a Goose export
without llama.cpp, a file, a device or a model, and each can be gated against a
transcription of the native lines over synthetic state.

The hypothesis is accepted only if the port reproduces the native decisions on
every case in the tables, at `-O1` **and** `-O0`, and if 12 mutations per slice
are all caught, six in each half.

## Inventory — what was still native, and what this experiment took

Already accepted and not repeated: session loop/budget/interrupt (`sl_`,
EXP602), session lifecycle/runtime facade/draft source (`ss_`/`rts_`/`nd_`,
EXP632), spec/MTP (`sdp_`, EXP619), CLI protocol (`cli_`, EXP572), config
(`cc_`/`cfg_`/`ar_`, EXP620), metrics (`mt_`, EXP627/628), router/overlap
(`rh_`/`ov_`/`rs_`, EXP629), streaming policy (`xl_`/`dw_`/`sp_`, EXP621),
GGUF/file I/O (`P9-02`).

Still native in `runtime/core/src/engine/session.cpp` at `137bc40c`, and clean
enough to port without the backend:

1. **Load admission and init order** — the refusal table around the model load
   (explicit hot slots, the `n_expert_used` override), the
   `llama_model_params`/`llama_context_params` projection, and the stage spine
   of `Session::open()`. The sizing arithmetic and the teardown order already
   belong to `ss_`; the widths to `nd_`.
2. **Perplexity pass** — `Session::perplexity()` (~165 lines) had no Goose
   decision at all: request gates, the step-mode prefix, the scored-position
   set in both regimes, the choice row, the aggregates.
3. **Request KV state** — `clear_kv` side-effect set, KV prefix reuse, draft
   rewind, `rollback_turn`, token-list appends. Not in `sl_` (loop head), not
   in `ss_` (teardown).

## Slices

| Slice | Module | Native source | Exports |
|---|---|---|---|
| S1 `lga_` | `source/lga-load-admission.goose` | `session.cpp:693-697, 737-751, 699-722, 819-847, 628-1271` | 10 |
| S2 `ppl_` | `source/ppl-perplexity-plan.goose` | `session.cpp:459-622` | 12 |
| S3 `kvp_` | `source/kvp-kv-turn-state.goose` | `session.cpp:1324-1337, 1434-1455, 1459-1478, 1575-1576, 1854` | 9 |

Prefix check against the canon (`goose/` exports, `_archive` included since it
is inside `goose/`): `lga_` 0, `ppl_` 0, `kvp_` 0. Evidence:
`results/prefix-check.log`.

## Gate

Differential, native transcription as the oracle, synthetic state only.

| slice | cases | checks | failures |
|---|---|---|---|
| `lga` | 118 | 512 | 0 |
| `ppl` | 287 | 2455 | 0 |
| `kvp` | 62 | 281 | 0 |

Identical at `-O1` and `-O0`: `results/gate-O1.log`, `results/gate-O0.log`.

```
OUT_DIR=$PWD/build    OPT=-O1 bash tests/build.sh all
OUT_DIR=$PWD/build-O0 OPT=-O0 bash tests/build.sh all
```

## Mutations

12 per slice, halves 6/6, built at `-O0`, every mutant passes `goose check`:

```
tests/mutate.py --module all --half all
lga: 12/12   ppl: 12/12   kvp: 12/12   → 36/36
```

Evidence: `results/mutations.log`.

## Files and SHA-256

| file | sha256 |
|---|---|
| `source/lga-load-admission.goose` | `e89755c12fe2cc10596935c7f52cb4fb1ac4a46579c91a5abdcbf36f3b4c3276` |
| `source/ppl-perplexity-plan.goose` | `4a6508718a6328aabeddbbb2510495f18b5a3f45128657c7d43605a7310188ac` |
| `source/kvp-kv-turn-state.goose` | `aa2fb64bf8918758fa710be5722a497b49436660120752038f9117c9c1bc609e` |
| `tests/fixture_lga.cpp` | `c40a8b7a18034049d5fdab273e3c2cc3c70d37ed34857cef6df3efe15668045a` |
| `tests/fixture_ppl.cpp` | `f945f2e57bbf6bf59fd3cbc6996066e94ad0b70894f11def961a146d8511c696` |
| `tests/fixture_kvp.cpp` | `dece13cc796e7d3683ac76c2f7095de5610517024d730efac719e996d8d8e4ac` |
| `tests/build.sh` | `fb1d65c9b04e744282b0c790db1f90a1260e6a9c474d9d69f3decfc5a5d77fc2` |
| `tests/mutate.py` | `592b68f9e5d2385cf94f7d12fd833efa2bf59306c0a70fb442a89275373877a0` |
| `results/gate-O1.log` | `5f2ddb9280f7308f3177668a08f75a7fb98fb92ec494afcb17da747a691fe622` |
| `results/gate-O0.log` | `0605163129b686c61e244f394ecebdc4942cfa686d6c2540e7aa9f9aa156b093` |
| `results/mutations.log` | `74fcf9599013e3b6d3faebbb4e16998e287ea12fd5730f95902f8189f1c91c07` |
| `results/prefix-check.log` | `cc1222f89421c75929e828e8f93a8f6edb95a5a49dddf710414227de12fd9fff` |
| `results/link-check.log` | `a2794f7d8245022218b57547590439a38aac010d1bec52a93725c5615b7bf92d` |

Compiler: `goose-2026.3-string`,
sha256 `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`,
runtime `1e714e6146bc`. Host: `gcc-13`/`g++-13 (Ubuntu 13.4.0-10ubuntu1)`,
Linux `7.0.0-43-generic`.

## Native boundary

The carrier keeps: `llama_model_load_from_file`, `llama_init_from_model`, every
`ggml`/CUDA call, the tokenizer, the decode, the logits and the log-softmax,
the KV itself, the chat renderer, the sampler chain, `pio::process_memory` and
`pio::device_memory`, and every error **string**. The slices return codes,
numbers and orderings; `REPORT.md` lists the exact call sites to replace.

## Not covered by this experiment

`P9-08` asks for the whole standalone engine. This experiment closes three
slices of it; the honest remainder is listed in `REPORT.md` §7
(`GenTally::record`/`PrefillTally` arithmetic, the prefill chunk plan, the trace
framing order, the MTP capture warm-up sequence, the CLI driver loop, the
sampling-chain construction). The requirement therefore stays `PARTIAL` until
those are ported or consciously assigned.
