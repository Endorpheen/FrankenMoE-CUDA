# Verified against compiled native code

Every entry below compares the ported Goose decision with native code that was **compiled and called**, not with
a transcription. That distinction is the whole point of this document: this session produced eight verification
errors where the reference itself was wrong, and every one of them would have been impossible in the arrangement
used here.

Two mechanisms, both runnable without a model and without a GPU:

- **link** a library that exports the native function (`bmoe_core`, built with `llama.cpp present = OFF`);
- **white-box include** the native translation unit, for functions that are `static` or class-bound.

## Reproduction status

`tools/run-verification.sh` runs the whole set: **11 passed, 0 failed, 1 stale**. Every differential listed in this
document is re-runnable in one command, which is what keeps the evidence from becoming a claim about a previous
tree.

The three sampler gates of EXP-1188 (adaptive-p 33211 cases, mirostat 500, RNG 1280) turned out to be live all
along: they call the exported doors directly and only the directory's `diff.cpp` still declares the run-local
dispatcher. A `verify.sh` in that directory now says so, and the runner prefers it over the default recipe.

## A correction: the harness was never missing anything

I wrote in three places that `EXP-2026-11-18-1188` referenced `gate_dispatch_c` with no definition in the tree,
and that the definition had lived in a run-local scratch file. That was wrong. The definition is committed in
`wrap_ship.c` in that directory and was there all along.

The real cause: `wrap_ship.c` defines the symbol with C linkage, and I was handing the file to `g++`, which mangles
it. Compiled as C and linked into the C++ harness, the whole measurement reproduces exactly:

```
typical-p 1800/1800, top-p 900/900, top-p large 45/45, XTC 720/720, DRY step 1 200/200
adaptive-p 33211, mirostat 500, RNG 1280
```

Two of my own conclusions were wrong before this one, and both had the same shape: a failed step was blamed on the
artefact's history rather than on the command I had just written. The record above is corrected rather than
appended to, because a wrong reason in a document is worse than no reason.

## The rule that does hold

A harness outlives a run only if everything it needs is committed. The sweep names anything it cannot run
(`tools/run-verification.sh` prints a separate `stale` count), because a harness that quietly leaves the sweep
looks exactly like coverage.

## Sampler chain (P6-10) - nine entry points driven through the shipped library

| decision | evidence | mechanism |
|---|---|---|
| typical, top-p, XTC, DRY (steps 1-4) | 1800 + 900 + 45 + 720 + 200 | harness |
| adaptive-p, mirostat | 33211 logits, 500 k-values | harness |
| **penalties, typical, xtc, mirostat, adaptive-p** | arrays identical (values, order, size), state carried across draws | public API, door on/off |
| **top-p, temp_ext** | identical including order and size | public API, door on/off |
| **dist** | 8000 selected tokens identical, door counter at every 100th call | public API, door on/off |
| acceptance | 6 official `apex-test.sh` runs, hash `c2ff42cc1219` | live |

The public-API arrangement is the one that matters: the same sampler object is driven twice with the door enabled
and disabled, so the comparison is with the native code rather than with a restatement of it. It found a **real
defect** - the DRY functions read the context oldest-first while the caller builds it newest-first, so any context
ending in a single-token breaker received a penalty the native never applies. Fixed; the acceptance hash is
unchanged because the fixed profile sets no DRY multiplier.

It also produced three alarms that were my instruments: a mirrored DRY harness, an unsorted array, and a dump that
ignored `cur_p->data` being advanced. Each is recorded as a correction in its card rather than quietly fixed.

## Grammar (P6-10) - correct, opt-in for cost

| decision | evidence | mechanism |
|---|---|---|
| full automaton | 0 disagreements of 1 736 513 candidates | per-candidate differential in the host |
| cost | decode 4.32 vs 50.3 tok/s (11.7x) | live, TD-S10 |

## Configuration (P9-01) - DONE

| decision | evidence | mechanism |
|---|---|---|
| recipe registry | 106 cases | `bmoe_core` symbols |
| recipe matcher | 4160 cases | white-box `router_hook.cpp` |
| config validation, 44 rules | 4000 configs, 0 uncovered | `bmoe_core` symbols |

## Deterministic workload (P9-07) - workload side complete

| decision | evidence | mechanism |
|---|---|---|
| metadata agreement | 2000 cases | white-box `deterministic_workload.cpp` |
| file reading, every failure mode | 9 crafted files | white-box, via `load()` |
| file writing | byte-exact, 4 workloads | white-box, via `save()` |
| release reproduction | rebuild hash equals `d5c566d0…` | byte comparison against the EXP1298 checkpoint |
| replay guards | 1000 cases | white-box |
| replay route + cursor | 1000 cases | white-box |

## Metrics (P9-07) - four decisions

| decision | evidence | mechanism |
|---|---|---|
| CSV field escaping | 4000 strings | white-box `decode_trace_sink.cpp` |
| compute row + 29 columns | 2000 rows | white-box |
| route row + NaN arm | 3000 rows | white-box `route_trace_sink.cpp` |
| io row + 12 columns | 2000 rows | white-box |
| encoder/unescaper pair | 4000 round-trips (961 lossy) | invariant only - no native reference exists |

## Which composed modules have a shipped-artefact check

A module can be proven against native code and still be absent from the runtime, so the question "is it checked
against the artefact everyone runs" is separate. For the twelve modules whose doors are in the shipped library:

| module | shipped-artefact check |
|---|---|
| metrics (21 doors) | yes - the CSV/row harnesses dlopen the shipped library |
| sampler chain, spec doors, spec decisions | yes - the door harnesses and the live counters on real runs |
| arch registry (6 doors) | yes - the recipe harnesses dlopen the shipped library |
| tokenizer, tokenizer doors | yes - `FG_TK_DIAG` inside a real server, 1678 comparisons |
| token pieces, end of generation | yes - 248 320 comparisons each, through the shipped library |
| detokenizer (3 doors) | yes for pieces and EOG; sequence door not measured |
| unicode tables and text | yes - through the tokenizer path above |
| gguf tensors, reader, saver | yes - 601's writer cases relinked against the shipped library |
| **config validation** | **no** - its harness includes the module source and calls `gg_function_1`, the pre-export-fn dispatcher, so it cannot be pointed at the library without a wrapper for the current signature |

Config validation's own evidence stands - 4000 configurations, 0 failures, 0 uncovered rules, twice - but it is
evidence about the module, not about the artefact.

## Tokenizer and detokenizer (P7-06, P7-07)

| decision | evidence | mechanism |
|---|---|---|
| BPE tokenization | 1678 live comparisons, 0 disagreements | `FG_TK_DIAG` inside a real server, mixed-script prompt |
| token piece | **248 320** comparisons - the whole vocabulary, byte for byte | door vs the native producer, in the shipped library |
| end of generation | **248 320** comparisons against a 5-token set | same arrangement |
| detokenize over sequences | **not measured** | two attempts failed in my measurement, not in the port |

The BPE module and its dependency closure are composed into the shipped unit, which is what makes these
comparisons possible at all: before that they lived only in a separate build.

## Speculative decoding (P6-11)

| decision | evidence | mechanism |
|---|---|---|
| prompt-lookup draft | 3000 cases | `bmoe_core` symbols |
| prefix agreement on real drafts | 2 live comparisons, `calls=2 disagreements=0` | shipped server with ngram drafters |
| prefix agreement | 14 live comparisons | live server |
| draft width | 1 live comparison, 4 configurations | live server |
| synthetic and rejection acceptance | gated synthetically only | their routes need a draft model |

## Not verified, and why

P9-03, P9-04 and the session half of P9-05: their native code needs a real shard (`ExpertStreamSource::init`), or
`llama.cpp`, or the **bmoe fork** which does not compile against `vendor/flint`
(`docs/goose-migration/P9-DEPENDENCY-BOUNDARY.md`). The three options for closing that are named there; none is
mine to choose.
