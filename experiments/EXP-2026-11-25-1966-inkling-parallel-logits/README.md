# EXP-2026-11-25-1966: Parallelize exact Inkling decode logit softcap

**Verdict: confirmed decode improvement on 8192/256.**
The first 32K/512 candidate request also exceeds the saved upstream
observation in both phases. A fresh paired cross-engine result remains open.

Inkling applies the same scalar FP32 scale, `tanhf`, and output cap to
200058 logits after every output projection. The prior CUDA `tanhf`
probe did not match all host bits. This candidate partitions the
vocabulary among the existing worker team instead. It uses the same
Goose numeric function for each chunk on at most four threads with
the existing decode affinity, validates finite results in each chunk,
and waits before returning logits or hashing state. The worker team
persists with the decoder. Prefill continues through the old
single-worker logit softcap, and padded logits are left intact.
An unconfigured flag or fewer than two configured workers uses the
scalar path. Two 128/16 requests on one server matched all 318
fingerprints and responses from the prior bridge checkpoint.

## Inkling 8192/256, first request in each process

Use the same binary in A–B–B–A. LFU and the joined CUDA bridge are
enabled in every arm. Context 36864, batch/ubatch 4096, Q4_0 KV,
expert limit 6720 MiB. Only `I2_PARALLEL_LOGITS` changes.

| Arm | Prefill, tok/s | Decode, tok/s | Decode, ms | Total client, s |
| --- | ---: | ---: | ---: | ---: |
| Scalar A1 | 277.673 | 21.710 | 11745.797 | 41.542 |
| Parallel B1 | 278.447 | 22.289 | 11440.603 | 41.157 |
| Parallel B2 | 278.761 | 22.178 | 11498.136 | 41.174 |
| Scalar A2 | 278.133 | 22.041 | 11569.560 | 41.309 |

Mean decode increases 21.875 → 22.233 tok/s (+1.64%). Decode
saves 305 and 71 ms in the two pairs. Mean client time is
41.426 → 41.166 s. All 256 IDs, text, 8703 state fingerprints and
final `/props` match. The maximum sampled VRAM is 11789 MiB for
both modes, with no increase in maximum VmHWM or swap-out.

## Inkling 32K/512, first request

The candidate yields 267.180 prefill and 21.527 decode tok/s in
146.686 s. EXP-1965 previously measured 267.379 and 21.222 on
the same fixture before the parallel softcap. All output IDs,
text, 33791 state fingerprints and props match. The candidate peak
is 11722 MiB board VRAM and 14229428 KiB VmHWM. There is no
process swap, system swap-out, or process storage I/O during
the request. The saved upstream observation from EXP-1927 is
138.436 prefill and 21.369 decode. These 32K figures come from
separate blocks, not the 8192/256 A/B.

The changed code is in the Inkling backend. The CUDA driver,
graph executor and LFU object reproduce the EXP-1962 objects
byte for byte. No new Apex runtime measurement or deployment
change was made in this experiment.

## Reproduction

Build from the repository with `bash source/build.sh /absolute/new/scratch`.
Set `ASSERT_SOURCE_DIR` to the absolute original
`build/inkling-apex-gate/parallel-logits-1966` directory for
byte-identical GCC assertion paths. The normalized rebuild
matched the executable hash. See `results.json` for exact
argv and env. Use `I2_PARALLEL_LOGITS=0` for control and
`1` for the candidate. Raw evidence is under
`build/inkling-apex-gate/parallel-logits-1966`.
