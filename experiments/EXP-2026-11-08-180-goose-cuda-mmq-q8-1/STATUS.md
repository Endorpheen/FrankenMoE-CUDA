# EXP180: MMQ Q8_1 quantization — correct, but 12-14% slower per launch

Status: FUNCTIONAL PASS, PERFORMANCE REGRESSION NOT RESOLVED. The transfer is
byte-exact against native on all cases and runs in Apex with an exact response.
The kernel itself is measurably slower, so the candidate is not promoted and this
document records the diagnosis rather than a claim.

## What is transferred

`quantize_mmq_q8_1_cuda` (direct) and `quantize_scatter_mmq_q8_1_cuda` (scatter),
DS4 layout only, which is what `mmq_get_q8_1_ds_layout` returns for Q4_0/Q4_1/Q5_1
— the types the Apex profile uses. Goose owns the 4-value thread stride, the
guarded float4 load, the partial absolute max, both shuffle reductions, `d_inv`,
the four rounded int8 values, the block index for both the direct and the scatter
form, the packed `char4` write and the `half2` scale/sum store.

The D4 and D2S6 layouts and the FP4 scatter are a different data layout; the host
module returns false for them so the parent keeps them native.

## Correctness

| check | result |
|---|---|
| `gpu-fixture` Goose | 8 checks / 0 failures |
| `gpu-fixture` native rollback | 8 / 0 |
| byte-for-byte `block_q8_1_mmq` arrays | 4 of 4 (direct 128/512/2048 and scatter) |
| compute-sanitizer memcheck | 0 errors |
| Apex 32K/128 response | exact 128 ids, text and all top-5, sha256 `e68df05b` |
| Apex witness | `FG180Q_CUDA_MMQ_Q8_1 host=0x23, paths=0x3` — both paths executed |

## The regression

A kernel-level timing fixture (`tests/timing.cu`) launches the real entry point
2000 times on a fixed 4096x8 input and reports ns per launch, five samples per
process, medians taken, arms in separate processes:

| run | Goose ns | native ns | delta |
|---|---:|---:|---:|
| 1 | 2068.0 | 1817.5 | +13.8% |
| 2 | 2057.7 | 1803.2 | +14.1% |
| 3 | 2065.4 | 1837.5 | +12.4% |

Consistent about +13%, well outside noise. End-to-end Apex shows a smaller and
noisier effect because quantization is a fraction of the request: prefill was
+0.46% and +0.71% in the two opposite-order pairs, decode +0.15% and -4.97%, so
the decode delta is not stable and no decode claim is made.

## Diagnosis, with the attempts that did not help

Instruction counts from the SASS of the DS4-direct kernel:

| | instructions |
|---|---:|
| Goose | 824 |
| native | 160 |

The Goose body is about five times longer. The instruction mix explains it:
206 `IMAD` variants against roughly 20 in native, and 24 `BRA` where native has
almost none. That is integer address arithmetic and dynamic loops, not
quantization math.

Four changes were tried and none recovered the gap:

1. unrolling the shuffle ladders to compile-time offsets (4/2/1 and 8/4/2/1)
   instead of a data-dependent loop — no measurable change;
2. six thin wrappers pinning `ds` and `scatter` so the layout branches stop being
   runtime branches — registers rose from 30 to 34 and it got no faster;
3. loading the float4 through four typed accessors from one aligned base so the
   backend can fold them into a single `LDG.128` — no measurable change;
4. splitting the kernel into two per-`SCATTER` specialisations, which did bring the
   direct kernel from 30 to 26 registers (native 20), yet the launch time stayed at
   about 2055 ns against 1810 ns;
5. computing the hot index chain (`k_block`, `iqs`, `blocks_per_channel`) in 32-bit
   where the native code does the equivalent, keeping 64-bit only for the final byte
   offset — still about 2035-2079 ns against 1812-1848 ns.

So register pressure is part of it but not the whole story: the remaining cost is
the emitted integer address arithmetic (`i0`/`k_block`/`iqs`/`ib` chains built from
64-bit multiplies) where the native code keeps pointer arithmetic and a
compile-time constant `ne1` stride.

## Next step for this candidate (not attempted: five tries exhausted the local budget)

Not a re-tune. The useful next move is to stop passing 64-bit extents into the
device body and instead precompute the byte strides and the per-launch constants in
the host module, publishing ready-to-use offsets so the kernel does the same
`base + i0` arithmetic the native code does. If that does not close the gap, the
honest conclusion is that the producer's 64-bit integer lowering costs about 13%
on this memory-bound kernel and the transfer should stay un-activated.

## Defects in my own work, found and fixed

1. The host wrote the launch geometry into plan slot 1, which also held the device
   scalars, so the kernel saw `ne00=1, s01=1`. The inputs moved to slot 6.
2. `n_expert_used` was read after the slot-2 clear, so the scatter path saw 0 and
   wrote nothing.
3. The fixture first used Q8_0, which maps to layout D4 and was correctly refused;
   DS4 needs Q4_0/Q4_1/Q5_1. With the wrong type both arms silently took the native
   path and the byte comparison was native against itself — the witness
   `paths=0x0` is what exposed it.
