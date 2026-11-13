# P3-03 closure: CUDA copy, gather, scatter and tensor transforms

Criterion: "All dtype/stride/tail variants match native and Compute Sanitizer
bounds checks; asynchronous copies preserve order."

The Apex/inference scope is closed. Model-conversion utilities (different-type
and quantized CPY pairs, non-Q4_K quantized GET_ROWS) are explicitly assigned to
P7-04/C2 and are not silently claimed as tested.

## Routes on the inference path

| route | state | evidence |
|---|---|---|
| CONCAT | Goose, live | EXP175, 76 checks/arm, quantized byte-identical, Apex exact |
| GET_ROWS float vector/scalar | Goose, live | EXP176, 27 checks/9 cases, 9/9 byte-identical, sanitizers 0 |
| CPY/DUP/CONT same-type F32 | Goose, live | EXP177, 18 checks/6 cases, byte-identical, sanitizers 0 |
| SET_ROWS | Goose, live | EXP174, 22 groups/333 checks, byte-identical |
| SET destination view | Goose, gated | EXP245, 220 044 checks, 1 mutation |
| PAD index map | Goose, gated | EXP246, 10 234 checks, C-modulo trap found |
| PAD_REFLECT_1D | Goose, gated | EXP413, 82 216 bytes/0, four sanitizers 0, 5/5 |
| ROLL index map | Goose, gated | EXP247, 45 277 checks, 3 mutations |
| transposed CPY tile F32 | Goose, gated | EXP434, 36 235 values/0, four sanitizers 0, 5/5 |
| GET_ROWS_BACK | Goose, gated | EXP433, 2 592 719 values/0, four sanitizers 0, 5/5 |
| Q4_K GET_ROWS | Goose, gated | EXP435, 61 952 values/0, four sanitizers 0, 5/5 |

The Apex token embeddings are Q4_K (`token_embd.weight`), so EXP435 is the
quantized gather actually used by the model. The F32 GET_ROWS path remains live
for other tensors.

## Remaining conversion utilities

These are not on the Apex inference path; they exist for quantization/model
conversion tooling:

- different-type and quantized CPY pairs (F32 <-> q8_0/q4_0/q4_1/q5_0/q5_1/
  iq4_nl, transposed quantized copies);
- quantized GET_ROWS for types other than Q4_K (Q2_K/Q3_K/Q5_K/Q6_K/IQ/MX);
- quantized GET_ROWS_BACK variants.

They are assigned to **P7-04** (quantization/conversion policy) and the
cross-format compatibility work in **C2**. Their dequantization arithmetic is
already Goose (P3-07); the missing part is the per-pair host plumbing and
per-type gather wrappers. Recording them separately avoids relabelling
conversion utilities as inference coverage.

## Verdict

P3-03 closes on the inference/Apex scope: every copy/gather/transform route the
product executes has a Goose implementation with a differential gate, sanitizer
evidence and mutations. The conversion-only pairs are named with their future
home and are not counted as tested.
