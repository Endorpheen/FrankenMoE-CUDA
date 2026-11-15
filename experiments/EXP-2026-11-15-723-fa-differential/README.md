# EXP-2026-11-15-723 — Goose flash attention vs native: the growth is inherent, not a defect

Base: `492090ef`. Two profiled runs of the same 32K/512 request, differing only in
`GGML_GOOSE_CUDA_FA`.

## 1. The end-to-end result first

| arm | decode | range | runs |
|---|---:|---|---:|
| Goose FA (default) | **39.86** | 39.83-40.22 | 3 |
| native FA (`GGML_GOOSE_CUDA_FA=0`) | 38.99 | 38.43-39.32 | 6 |

**Goose is 2.2% faster end to end than native.** So the premise that needed checking - "fg175m is
slow, make it faster" - is wrong at the level of the whole request.

## 2. At the kernel level it is the other way round

| arm | kernel | calls | total s | avg ms | min | max |
|---|---|---:|---:|---:|---:|---:|
| Goose | `fg175m_kernel` | 90 | 1.419 | 15.77 | 0.2 | 34.5 |
| native | `flash_attn_ext_f16<256,256,8>` | 80 | 1.357 | 16.96 | 3.6 | 32.6 |

Plus three kernels Goose does not have: `flash_attn_mask_to_KV_max` (0.004 s),
`flash_attn_stream_k_fixup_general` (0.003 s), `flash_attn_ext_vec` (0.005 s). Native FA total:
**1.369 s against Goose 1.419 s**, i.e. Goose's kernel is 3.7% slower.

So Goose wins the request by being 3.7% slower in one kernel but skipping three surrounding ones -
the net is +2.2% decode. That is a legitimate architectural advantage, not luck.

## 3. The growth is inherent

The 4.4 -> 34.5 ms growth was the headline finding of EXP-722b. The native kernel grows the same way:

| arm | first calls | last calls | factor |
|---|---|---|---|
| Goose | 4.4 ms | 34.5 ms | 7.8x |
| native | 3.6 ms | 32.6 ms | 9.1x |

**The native kernel grows slightly MORE than Goose.** So the growth is not a Goose defect and not
something to "fix": flash attention must touch every key up to the current position, so cost per
call rising with context is the arithmetic of attention, not an inefficiency.

This is worth stating plainly because it was the working hypothesis for two experiments: **there is
no 30% win hiding in fg175m's scaling.** The 22%-of-calls-taking-43%-of-time distribution is the same
shape in both arms.

## 4. Where the real remaining headroom is

Since Goose already beats native on the request, and the kernel-level gap is 3.7%, the remaining
4.5% to the historical reference is **not** in flash attention. Candidates left, in order of
measured size:

| candidate | decode share | note |
|---|---:|---|
| `mul_mat_q<type12>` | 2.363 s of 7.933 s (29.8%) | native ggml MMQ, substrate's tuned path |
| `mul_mat_q<type14>` | 0.744 s (9.4%) | same |
| `fg175m` | 1.419 s (17.9%) | Goose, already faster than native |
| `fg174m` (GDN MMA) | 0.641 s (8.1%) | Goose |
| `quantize_mmq_q8_1` | 0.413 s (5.2%) | converts activations before each MMQ |

The matmul family together is **39.2% of decode** and is the largest single block. That is where the
next measurement belongs - specifically whether the Q8_1 quantisation step (`quantize_mmq_q8_1`,
2,928 calls, one per matmul) can be fused or avoided.

## 5. What this changes

The instruction was "optimise fg175m first". The measurement says fg175m is already better than the
native path it replaces, and that its scaling is arithmetic rather than waste. Optimising it further
would be reworking a component that is winning.

The honest next target is the matmul block, and within it the question is concrete: 2,928
`quantize_mmq_q8_1` calls accompany 2,928 `mul_mat_q` calls - one activation conversion per matmul.
Whether that conversion is necessary every time, or could be reused or fused, is measurable.

## 6. Reproducibility

Both profiles via `nsys profile --trace cuda --sample none` on the pinned configuration, then
`sqlite3` against `CUPTI_ACTIVITY_KIND_KERNEL` joined to `StringIds`. `*-kernels.txt` in this
directory holds the queries. The `.nsys-rep` files are not committed.
