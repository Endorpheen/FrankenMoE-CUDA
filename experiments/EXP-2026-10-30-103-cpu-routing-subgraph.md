# EXP-2026-10-30-103: move the small routing subgraph to the CPU

Status: REJECTED by measured upper bound. No code was built.

## Hypothesis

In single-token decode, keep the router projection on the GPU but run the small
top-k/softmax/selection subgraph on the CPU before the CPU expert matmuls, to
remove GPU launches and a CPU/GPU bounce.

## Measured upper bound (EXP084 decode trace, unchanged decode kernels)

- Cold request decode window: 4.418923 s.
- GPU kernel union in that window: 0.971574 s.
- Sum of the ten reported top kernels: ~0.828 s; everything else, including
  routing/selection kernels, is at most ~0.143 s = 3.24% of the decode window.
- The router projection itself is one of the GPU `mul_mat_vec_q` entries; the
  top-k/softmax part is inside the <=3.24% remainder.

Moving the subgraph to the CPU cannot save more than that remainder and adds a
device-to-host copy plus a synchronization for the logits/ids before the CPU
experts can start. The pre-stated 5% decode gate is above the entire upper bound.

## Caveat

The trace predates the EXP089 mmap registration, but decode keeps the routed
experts on the CPU and the attention/GDN kernels are unchanged by registration,
so the decode kernel set and its share remain applicable. The routing operation
is not moved by EXP089.

## Verdict

REJECTED. Eliminating every non-top-10 decode kernel would still be below 5%.
No implementation was attempted; weakening the gate after the result is not
allowed.
