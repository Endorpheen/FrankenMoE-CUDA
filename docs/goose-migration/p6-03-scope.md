# P6-03 scope: why the builder DSL is not more of EXP200

The plan says "port the complete shared graph-builder DSL". Checked what that would actually mean in
this profile, because the honest answer changes the shape of the work.

## The candidates and what they are

| function | lines | Apex relevance |
|---|---:|---|
| `build_cvec` | 4 | control-vector apply; `cvec` is null in Apex, so it is a passthrough |
| `build_hadamard_input` | 30 | `hdmd` is never set for qwen35moe; early `return cur` |
| `build_norm` | 25 | 13 call sites, **all** `LLM_NORM_RMS` with `mb == nullptr` |
| `build_inp_embd` | 171 | three embedding paths; `llm_arch_supports_mixed_batch` does **not** list QWEN35MOE, so `has_mixed` is false and one path is dead |

Every one of these is a **decision wrapper around `ggml_*` node creation**. Porting them to Goose moves
the `if`s, not the tensor operations: `ggml_get_rows`, `ggml_mul`, `ggml_reshape_4d` stay native
because they build graph nodes.

So "port the DSL" is not a continuation of EXP200's per-decision style at larger scale. It needs a
Goose representation of graph construction itself — nodes, edges, tensor lifetimes — and that is an
architecture question, not a function-by-function port.

## What is genuinely value here, and what is not

Measured against the profile:

- `build_inp_embd` at 171 lines and 55 graph operations is the largest single candidate, and two of
  its three paths are **dead in Apex**. Porting it as-is would put unexecuted branches behind a Goose
  call, which no gate would catch — the EXP219/220 lesson applied to a new layer.
- `build_norm`'s decision is fully determined (13/13 identical). Porting the choice would move one
  `switch` across a function-call boundary and add cost, like EXP212 and EXP218.
- `build_hadamard_input` and `build_cvec` do not execute meaningfully in this profile at all.

## Conclusion

I am not starting a port here, and the reason is not that it is hard — it is that **porting these
functions as written would move unexecuted code behind Goose calls and make the runtime harder to
reason about while measuring nothing**. That is the opposite of what the migration is for.

What P6-03 actually needs, before any code: a decision on whether graph construction is expressed in
Goose at all, and if so at what granularity (whole builders, or only the parts with real arithmetic).
That is a design question for Igor and Codex, and it is recorded rather than guessed at.

The smaller honest statement of remaining work in the model runtime: the builder stays native, the
per-layer decisions are Goose (EXP200), and the boundary is documented. Anyone reading the ledger
should know that `P6-04` cannot close by porting more helpers.
