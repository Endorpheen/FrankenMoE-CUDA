# EXP-2026-11-24-1910: Exact ordered sampler scan for Inkling

Goose retains the heap operations, tie ordering, probability cuts and RNG.
A native ordered scan skips runs with no candidate above the heap root.
The implementation uses AVX2 when available and a scalar fallback elsewhere.
The same ordered scan handles greedy selection. The companion operation
fills identity token IDs in bulk. The experimental gate checks Inkling and
`I2_SAMPLER_SCAN=1`.

The test used an RTX 4070 with Inkling-2-Mini AD-IQ4_XS, Q4_0 KV, context
256, 128 input tokens, 16 output IDs and 15 measured decode intervals.
Batch and ubatch were both 128. Both arms used the same executable and
differed only in the sampler flag.

| Paired mean | Baseline | Candidate |
| --- | ---: | ---: |
| Cold decode, tok/s | 11.74 | 14.83 |
| Warm decode, tok/s | 11.83 | 14.23 |
| CPU between warm backend calls, ms | 13.149 | 0.157 |

All 318 fingerprints per arm, generated IDs and text matched. Model-process
peak RSS varied by at most 56 KiB, with zero process swap or system swap-out.
Sampled board VRAM varied across the block and did not increase in a
candidate arm above the largest observed control. The primitive passed
203840 ordered comparisons including ties, NaN, infinities, signed zero and
unaligned bounds.

The receipts are `results.json` and `gap-results.json`. This is an isolated
short-workload candidate. The full combined-runtime Apex gate and the
long-context Inkling acceptance gate remain open.

## Reproduction

Start from the frozen Inkling completion composed Goose unit. The expected
SHA-256 is recorded in `composed-source.sha256`. Generate the changed unit
with `python3 source/apply-sampler.py <original.goose> <candidate.goose>`,
compile with the same KeelWing and GCC 13 profile, and link
`source/sampler-scan.c` at `-O3` with the generated header and original
host archive. Compile `source/scan-check.c` with the helper for the
model-free ordered-scan gate. Use `I2_SAMPLER_SCAN=0` for A and
`I2_SAMPLER_SCAN=1` for B.
