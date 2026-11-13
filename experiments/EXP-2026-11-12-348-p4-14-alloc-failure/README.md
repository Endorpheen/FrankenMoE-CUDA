# EXP348: allocation-failure injection -- identical behaviour on both lines

P4-14 asks for allocation-failure injection among other error paths. This experiment injects malloc/calloc failures through an
`LD_PRELOAD` shim (`FAIL_AT=N` fails the N-th allocation) and runs the same probe on both lines.

## Results

| FAIL_AT | native | ours |
|---|---|---|
| 1 | `init=ok`, all tensors ok, rc 0 | identical |
| 2 | `libgomp: Out of memory allocating 8 bytes`, rc 134 | identical |
| 30 | `memory allocation of 416 bytes failed`, rc 134 | identical |
| 60 | same message, rc 134 | identical |
| 120 | `memory allocation of 22 bytes failed`, rc 134 | identical |
| 300 | `memory allocation of 416 bytes failed`, rc 134 | identical |
| 1000-20000 | no failure fires (the allocation count never reaches the index), rc 0 | identical |

Same exit codes, same messages, at every point that actually fires.

## What the sweep also shows

Tensor creation does **not** call malloc: the probe creates 200 tensors and the injection points past a few hundred never fire, because
tensors are carved out of the context's pre-allocated arena. So allocation-failure injection exercises **context creation**, not the tensor
loop -- and that is the honest scope of this slice, not a gap in the probe.

## Method note

The first version lost all stdout on abort because it was block-buffered -- the same trap as EXP339's graph trace. The probe now sets
`_IONBF`. It is the second time this session that a buffered pipe hid the evidence.

## Status within P4-14

Allocation failure: covered, identical. Invalid size/view/copy decisions: covered in P4-03 (six of seven identical, the seventh being the
known block-alignment divergence). Still open in P4-14: abort/reset/reuse ordering and concurrent independent contexts.
