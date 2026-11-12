# The merged candidate

`assemble_candidate.py` builds `runtime/reconstructed/candidate-20261112` from three pieces:

- the EXP340 stage as the base, which carries the older but reproducible `libggml-base`, `libggml-cpu`, `libggml-cuda` and the modular support
  libraries;
- the merged `libllama-exp371.so.0.7.0`, which now contains the batch chain, the context reorder, the batch container plan, the sampler, vocab,
  graph work, plus `fg291b` (batch reject), `fg286m` (lazy/mmap) and `fg262a`/`fg263c` (MoE cache admission and copy decisions);
- the merged `libllama-server-impl.so` and `llama-server`, which contain `fg268p` (checkpoints), `fg270q` (queue), `fg272s` (slot selection),
  `fg274b` (batch compatibility) and `fg276t` (stop strings).

## Smoke test, without Apex

With the tiny `stories260K` model and `-c 256`: the server starts, `/health` answers `{"ok":...}`, `/props` returns the default generation
settings, and the log reports `n_slots = 4, n_ctx_slot = 256` followed by `model loaded`. That is a load-and-serve check on a 260K model, not a
measurement; nothing here says anything about Apex behaviour or throughput.

## What the candidate still lacks

The three `libggml-*` libraries are still the older ones, because their sources live in the flint tree and the post-244 work does not touch
them. That means the candidate is complete on the llama and server side and unchanged on the ggml side -- and the Apex run that would tell
whether the eleven merged bridges behave is the next step, with agreement first.

## Status of each merged bridge: observed or only linked

A witness is the only thing that separates "linked" from "runs", so here is the split for every bridge the merge added, from the smoke runs.

| bridge | port | status | evidence |
|---|---|---|---|
| `fg274b` | batch compatibility | **observed** | `calls=15 yes=15` |
| `fg291b` | batch reject | **observed** | `continuity=18 walk=18 mrope=0 refused=0` |
| `fg272s` | slot selection | **observed** | `similar=1 lru=1` |
| `fg270q` | queue | **observed** | witness line printed, no events on one request |
| `fg276t` | stop strings | **observed** | witness line printed, no stop in the request |
| `fg268p` | checkpoints | **observed** | witness line printed, no eviction at this size |
| `fg262a` | MoE cache admission | **linked only** | needs a MoE model; no witness printer exists in its experiment |
| `fg263c` | MoE cache copy | **linked only** | same |
| `fg286m` | lazy/mmap bookkeeping | **observed** | `gdb` breakpoint on `fg286m_should_lock` hit during model load |

"Observed" means the bridge counted a decision during a run. "Linked only" means the symbol resolves but nothing in this session proved it
executed -- and for those three, that is not a formality: the batch-reject bridge was in exactly this state until it was linked from the wrong
object and had to be fixed.

`fg286m` was promoted from "linked only" to "observed" without touching any generated code: `gdb` with a pending breakpoint on
`fg286m_should_lock` stops inside the bridge during model load, which proves the call site is reached. The same probe on `fg262a_run`,
`fg263c_run_from_slot` and `fg263c_run_from_pairs` produced no hit on a dense model, which matches the expectation that those paths are MoE-only
rather than suggesting a mistake.

So the breakpoint probe is the witness substitute when a module has no printer, and only the two MoE-cache bridges remain unobserved --
because they need a MoE model, which means Apex.
