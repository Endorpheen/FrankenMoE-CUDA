# EXP-2026-11-24-1887 — Simplify Q4_0 decode Attention loads

**Rejected.** The accepted four-head kernel shares K and dequantized V
within each CTA. The first variant simplifies signed divisions and remainders
for nonnegative lane and V-slot indexes in functions 43 and 30 of the frozen
`flash-vec-device.h`. The second also substitutes a single 32-bit load for a
pair of 16-bit payload loads when aligned. Its unaligned path preserves the
two original 16-bit loads. No KV format change, extra allocations, launch
changes or FP32 reassociation.

The address-only cubin is byte-identical to the rebuilt control. The
rebuild and archived EXP-1765 manual2 cubins differ by one byte in the ELF
section alignment for shared memory (16 vs 4); the kernel text and the
remaining bytes match. The benchmark uses the original archived control.
The aligned variant uses 131 registers/thread compared with 142. Both use
4608 bytes of shared memory, have zero spills, and allow three CTAs per SM.

## RTX 4070 probe

EXP-1872's CUDA Graph fixture uses ten independent layers, 16 query and two
KV heads, exact Q4_0 query pack, and the original merge. Warm up eight graph
replays and measure 96 consecutive replays in each A-B-B-A arm. All 40,960
FP32 output words per shape, plus the final layer's partials and metadata,
matched bit-for-bit. Mean microseconds for a ten-layer graph:

| KV length | Partitions | Control | Aligned loads | Time change |
| ---: | ---: | ---: | ---: | ---: |
| 17 | 2 | 116.40 | 142.23 | +22.2% |
| 129 | 2 | 175.73 | 215.74 | +22.8% |
| 32769 | 64 | 1694.90 | 2103.83 | +24.1% |
| 33279 | 64 | 1708.01 | 2126.84 | +24.5% |
| 65537 | 64 | 3300.93 | 4120.26 | +24.8% |

A conditional aligned load slows the graph at every length. This probe
does not distinguish predication, instruction scheduling or memory traffic
effects. The accepted EXP-1880 checkpoint remains the model control. No
model was launched, so full Apex prefill, decode and memory were not measured.

## Reproduction

Restore EXP-1765 manual2 `flash-vec-device.h`, `flash-vec-types.h` and
CUDA glue over a copy of EXP-1786's device tree. From the directory containing
that `device/` directory, run `python3 edit.py "$manual2/flash-vec-device.h" address`
or `python3 edit.py "$manual2/flash-vec-device.h" aligned`. Compile with
the Clang 18 command in [EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md).
Use CUDA 12.6 `ptxas -arch=sm_89 --entry goose_flash_vec_q4_packed_quad`.
Run the [EXP-1872 probe](../EXP-2026-11-24-1872-attention-regcap/README.md)
with the archived MMQ PTX, manual2 cubin, and aligned cubin.

See [raw output](aligned.log), [compiler report](aligned-ptxas.log) and
[structured results](results.json).
