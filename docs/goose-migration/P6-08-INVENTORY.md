# P6-08 inventory: context construction, evaluation, graph reuse and output APIs

The criterion: "Goose owns context construction, warmup, evaluation, graph reuse, output/embedding/logit APIs,
performance counters and error propagation."

Evidence source: one accepted-profile run (EXP-281 log), where **91 distinct `FG*` witnesses** fired. The
context-orchestration ones are listed below.

## Clause by clause

| clause | owner | evidence from the live run |
|---|---|---|
| context construction | Goose | `FG155_GEOMETRY`, `FG156_CONSTRUCTORS`, `FG157_CONTEXT_CORE`, `FG159_GRAPH_LIFECYCLE`, `FG161_ALLOCATOR`, `FG162_BUFFERS`, `FG163_STREAM`, `FG164_SCHEDULER` |
| warmup | Goose at the decision level | `FG158_GRAPH_BUILDERS`, `FG160_SHARED`, `FG391S_FUSED_OPS_RESOLVE`; the first build is the warmup, there is no separate warmup decision |
| evaluation | Goose | `FG405S_SCHED_DECISIONS`, `FG408S_GRAPH_INPUT_PLAN`, `FG404S_WEIGHT_PLACEMENT`, `FG200G_MODEL_GRAPH`, `FG200G_MOE`, `FG200G_RECR` |
| graph reuse | split | the scheduler's decision layer is Goose (`FG405S_SCHED_DECISIONS`); the CUDA-graph capture/replay automaton is ggml-cuda's, recorded as native since EXP-050 |
| output/embedding/logit APIs | Goose | APEX.OUTPUT closed: `FG384S_SOFTMAX`, `FG385S_SAMPLER_GUARDS`, `FG387S_SAMPLER_CHAIN`, `FG386S_DIST_PICK`, `FG242O_UBATCH_OUTIDS`, `FG401S_DETOK_DECISIONS` |
| performance counters | native | `timings` in the response are `ggml_time_us` deltas in `server_slot`; a counter is a measurement primitive, not a decision |
| error propagation | native | `GGML_ABORT`, exceptions and `llama_context` return codes; propagating an error is the language boundary |

## What the split means

Six of the seven clauses are already Goose and witnessed on the accepted run. The two that are not are mechanisms rather
than decisions:

- a performance counter needs a clock and an accumulator; putting the subtraction behind a module would add a call per
  token for no ownership gain;
- error propagation is `throw`/abort/return-code plumbing across the C++ boundary.

Graph reuse is the one genuinely mixed clause: Goose decides *what* to schedule, ggml-cuda decides *whether the captured
graph is still valid*. The second half is the native automaton recorded in EXP-049/050.

## What would close the item

1. An owner decision that counters and error propagation are primitives (the same treatment as cuMemMap and memcpy), and
   that the CUDA-graph automaton stays native while `FG405S` owns the scheduling decision.
2. Or a decision to move the capture/replay decision into Goose, which needs a module that can see node properties and
   the warmup counters -- a real design change.

Nothing else in the criterion has an unwitnessed gap on this profile.
