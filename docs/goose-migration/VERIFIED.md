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

## Sampler chain (P6-10) - accepted, checkpointed

| decision | evidence | mechanism |
|---|---|---|
| typical, top-p, XTC, DRY | 1800 + 900 + 45 + 720 + 200 cases, 0 mismatches | differential stand |
| adaptive-p, mirostat | 33211 logits, 500 k-values | differential stand |
| RNG stream | 1280 draws vs `std::mt19937` | differential stand |
| acceptance | 6 official `apex-test.sh` runs, hash `c2ff42cc1219` | live |

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

## Speculative decoding (P6-11)

| decision | evidence | mechanism |
|---|---|---|
| prompt-lookup draft | 3000 cases | `bmoe_core` symbols |
| prefix agreement | 14 live comparisons | live server |
| draft width | 1 live comparison, 4 configurations | live server |
| synthetic and rejection acceptance | gated synthetically only | their routes need a draft model |

## Not verified, and why

P9-03, P9-04 and the session half of P9-05: their native code needs a real shard (`ExpertStreamSource::init`), or
`llama.cpp`, or the **bmoe fork** which does not compile against `vendor/flint`
(`docs/goose-migration/P9-DEPENDENCY-BOUNDARY.md`). The three options for closing that are named there; none is
mine to choose.
