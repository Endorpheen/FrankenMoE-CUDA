# EXP-2026-11-24-1879 — Change dense Q4 tiles and CTA order

**INCONCLUSIVE for full-profile adoption.** EXP-1877 remains the
research checkpoint. The 128×64 tile was rejected at the kernel
stage. A grouped 128×256 traversal gave small full-profile gains
with inconsistent decode and client timings.

## Isolated tests

The control is the frozen SM89 Q4 128×256 pipeline. The first
variant starts with EXP-1864's 128×128 source, halves its column
tile and removes the unused accumulators. It uses 129 registers,
30 KiB shared memory and three resident CTAs per SM. The control
uses 226 registers and one CTA. Despite the extra occupancy,
128×64 loses to 128×128 on all six timed shapes. On 8192×4096,
it was slower than the 128×256 control as well.

The second variant preserves the full 128×256 calculation and
grid dimensions. It maps the existing CTA indices to tiles in
groups of four or eight rows across column tiles, then moves to
the next row group. This tests more local prepared-weight reuse.
Both variants preserve all output words on seven fixtures,
including partial groups, tail columns and nonzero route starts.
At rows/routes/extent 2048/3584/16, group4 improved median
0.81283 to 0.76242 ms. At 8192/4096/8 it improved 1.80688
to 1.73702 ms. See [raw tile timings](n64-probe.log) and
[raw traversal timings](swizzle-probe.log).

## Full Apex 32K/512

RTX 4070, context 262144, cache 6290 MiB, batch and ubatch 4096,
Q4_0 KV. A-B-B-A, one warm-up and one scored request per server
on the same binary. A sets `GOOSE_MMQ_Q4_GROUP4=0`.
Only B logs selection of group4. Matrix calculations, scratch
layout, graph and decode dispatch remain those of EXP-1877.

| Arm | Prefill tok/s | Decode tok/s | Client s | Sampled peak VRAM MiB |
| --- | ---: | ---: | ---: | ---: |
| A1 | 3476.42 | 81.03 | 15.823 | 8774 |
| B1 | 3480.81 | 82.06 | 15.736 | 8774 |
| B2 | 3492.25 | 81.13 | 15.772 | 8774 |
| A2 | 3488.59 | 81.96 | 15.721 | 8774 |

Mean prefill increased 3482.50 → 3486.53 tok/s (+0.12%, 10.87 ms).
Mean decode changed 81.50 → 81.59 tok/s (+0.12%).
Client mean shortened 18.20 ms. In the second pair, prefill
rose 0.11%, decode fell 1.01% and client latency rose 51 ms.
No direct decode benefit is established. All eight responses
matched all 512 IDs and text. Full properties and SLRU counters
matched byte-for-byte. Peak RSS was 21492248–21494356 KiB.
Process swap and system swap-out were zero. No scored request
reached 3500 prefill tok/s.

## Reproduce

For the narrow probe, apply the n64 patches in [source](source)
to EXP-1864's narrow device tree, emit its Goose fragment with the
frozen KeelWing toolchain, then use Clang 18 and CUDA 12.6 ptxas
on SM89 as in EXP-1864. Extract the same entry name.
Keep its control and narrow cubins beside `probe.py`.

For group4/group8, apply the swizzle patch to the frozen EXP-1786
device tree. Compile and extract the two entries
`goose_mmq_q4_dense_m128n256_group4` and
`goose_mmq_q4_dense_m128n256_group8` into `swizzle.cubin`.
Use the same archived control and `swizzle-probe.py`.
For Apex, extract group4 alone, embed it in `mmq-group4-image.h`
as `static const unsigned char mmq_group4_image[]`, apply the
driver patch to EXP-1877, and replace only cuda-driver.o.
Verify [SHA256SUMS](SHA256SUMS). Exact timings, run identifiers,
and memory gates are in [results.json](results.json).
