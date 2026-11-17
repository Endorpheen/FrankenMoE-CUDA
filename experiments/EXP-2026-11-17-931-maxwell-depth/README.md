# EXP-2026-11-17-931 — Maxwell at full depth

Status: ACCEPTED (synthetic weights)

## Hypothesis

The mHC graph and the fused HC path stay within the runtime's graph budget and
produce finite logits at the real layer count, not only at two layers.

## Result

A synthetic 45-layer Maxwell (hidden 64, 8 experts, dense layers 0-2) converts,
loads, reserves and decodes: `decode rc=0`, `finite=1`, `argmax=69`,
`l1=31.59`, reservation 20 ms. Gate:
`python3 experiments/EXP-2026-11-17-931-maxwell-depth/tests/depth.py` →
`MAXWELL-DEPTH OK`.

## What this does not prove

Real weights, Q4_0 accuracy at depth, speed, memory, or the >4096 block-sparse
path.
