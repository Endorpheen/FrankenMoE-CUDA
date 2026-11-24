# EXP-2026-11-24-1880 — Specialize the prefill KV rotations

**PROVEN and accepted as a research checkpoint.** EXP-1877 used one
dynamic FWHT for all rotation widths. Fixed width-64 and width-256
Goose entries remove the register array and dynamic loop bounds.
The same warp and cross-register butterflies retain their arithmetic
order. The Q4 writers call those entries and retain the original
quantization. All four specialized entries have zero local frame
versus 64 bytes for the generic variants. The driver selects them
for batched SM89 prefill only. Context, quantization, graph layout,
cache allocation, and decode dispatch are unchanged.

On RTX 4070 the float-output probes matched every FP32 word on
six shapes, including exact input/output aliasing and tail rows.
The Q4 probes matched every byte on six further shapes. For
width 64 and 262144 rows, float FWHT went from 1.055 to 0.320 ms.
Width 256 and 65536 rows went from 0.713 to 0.316 ms. The two
large Q4 cases improved from 0.154 to 0.078 ms and from 0.119
to 0.046 ms. See [float samples](probe.log) and [Q4 samples](probe-q4.log).

## Full Apex 32K/512

RTX 4070, context 262144, cache 6290 MiB, batch and ubatch 4096,
Q4_0 KV. One warm-up and one scored request per fresh server.
A-B-B-A on one binary with `GOOSE_FWHT_FIXED=0` in controls.
All candidate arms logged both specialized dispatches.

| Arm | Prefill tok/s | Decode tok/s | Client s | Peak sampled VRAM MiB | System swap-out pages |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 3488.47 | 81.03 | 15.787 | 8774 | 0 |
| B1 | 3515.98 | 81.82 | 15.653 | 8774 | 0 |
| B2 | 3494.87 | 82.38 | 15.666 | 8774 | 0 |
| A2 | 3489.95 | 83.28 | 15.612 | 8774 | 0 |
| A3 excluded | 3454.81 | 83.55 | See results | 8774 | 9849 |
| B3 independent | 3484.57 | 83.86 | See results | 8774 | 0 |

The clean paired means were 3489.21 → 3505.42 prefill tok/s
(+0.46%, 43.36 ms) and 82.15 → 82.10 decode tok/s (-0.07%).
Mean client time improved by 40 ms. A2/B2 gains prefill but
loses more decode in percentage terms. Adoption uses the
aggregate means. Prefill improved in both clean pairs.
The confirmed clean candidate range is 3484.57–3515.98, with
mean 3498.47 over all three scored B requests. One request exceeded
3500; the threshold is not sustained. A3 is excluded from the
comparison and B3 is reported independently.

All 512 IDs, text and SLRU properties matched the reference.
Runtime VmSwap was zero. Maximum observed RSS in the two paired
candidate arms was 21493960 KiB versus 21494328 KiB in controls.
Peak sampled VRAM stayed 8774 MiB. The next priority is decode.

## Reproduce

Apply the device and Goose fragment [patches](source) to the
frozen EXP-1786 source. Re-emit with historical KeelWing and
Clang 18 as in EXP-1859. Assemble on SM89 with CUDA 12.6 ptxas
and restrict `--entry` to the regular and Q4 names ending in
`_64` and `_256`. For control, extract the original entries
from archived MMQ PTX. Run the two Python probes with their
matching control and fixed cubins.

For the full model, apply the driver patch to EXP-1877.
Embed the four-entry cubin as `fwht_fixed_image` in
`fwht-fixed-image.h`, rebuild only the driver and relink
the accepted archive. The local checkpoint is
`codex-20261124-fwht-fixed-accepted`. Verify its
[SHA256SUMS](SHA256SUMS). [results.json](results.json) has exact
run IDs, conditions and timings.
