# EXP-2026-11-19-1366: the synthetic acceptance door was in a file the build does not compile

## The finding

The door and its counter were added to `vendor/flint/tools/server/server-context.cpp`. The object the build actually
compiles comes from a different file:

```
builds/flint-acc/tools/server/CMakeFiles/server-context.dir/home/end0/AI/FrankenMoE-CUDA/runtime/reconstructed/server-merge/server-context.cpp.o
```

So the comparison never existed in the shipped server. The evidence that settled it was not reading: the rebuild
reported `Built target server-context` without compiling anything, and `strings` on
`libllama-server-impl.so` did not contain the counter's text. This is the same class as the missing host registration
(EXP1298): the source was right and the artefact was empty.

## The measurement, after moving the door to the compiled copy

```
FG_SP_SYNTH goose calls=1 disagreements=0
FG_SP_SYNTH goose calls=2 disagreements=0
FG_SP_SYNTH goose calls=3 disagreements=0
FG_SP_SYNTH_DIFF goose=0 native=1 p=0.916667 u=0.024895 replay=0
```

One disagreement, and it was in the comparison rather than either side: the port folds the drafted-EOG veto into its
answer (`sp_synth_accept` returns 0 for an EOG draft unless replaying), while the native code applies that veto after
the acceptance decision. Comparing the port's effective decision with the native's raw one reports every drafted EOG.

With the native side taken as its effective decision - the same quantity the port answers - the run is clean:

```
FG_SP_SYNTH goose calls=3 disagreements=0      (no DIFF lines)
```

## Why the comparison is the right place to fix it

Moving the veto out of the port would change a module whose own gate tests the folded behaviour, and the port's comment
documents it. The native code's two-step structure is not something the port has to reproduce; the decision is what has
to agree, and the decision includes the veto.

## State

| item | state |
|---|---|
| synthetic acceptance reached on the real model | yes, with a live counter |
| equality | 0 disagreements over the positions exercised |
| dead edit in `vendor/.../server-context.cpp` | reverted; that file is not compiled by this build |
| server rebuild | unblocked: the complete asset tree from `tools/ui/ui-gzip/_gzip` satisfies stage 1 |

## Acceptance

```
decode   62.95 / 62.64 / 62.57
prefill  2948.3 / 2692.1 / 2950.7
hash     c2ff42cc1219
```
