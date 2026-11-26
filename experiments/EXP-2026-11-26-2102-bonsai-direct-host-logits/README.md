# EXP-2026-11-26-2102 — Direct host logits readback for Bonsai

**Experimental alternative.** Correct on the tested requests, with no
measured decode gain. Keep EXP-2101 as the cumulative checkpoint.
No CUDA kernel, Goose object, or working release change.

EXP-2101's CPU profile found a host memory copy after full logit
readback. Bonsai normally reads the vocabulary into an exchange buffer
outside Goose's reentrant arena and copies it into the generation
output after evaluation. Test a narrower lifetime: pass the input ID
by value, run the graph without CPU readback, and write logits straight
into the restored caller buffer after the last Goose reentry. This
removes the host transfer without changing the order of graph work.
Scope it to single-token Bonsai decode with a full-width output.
Prefill and the other model paths retain the existing ABI. The GPU
choice path stays available, with direct readback possible only if it
falls back to the full logits.

The isolated binary uses the exact EXP-2101 main.o, CUBIN and frozen
Goose PTX. Four short HTTP scenarios with CPU choice match output IDs
and text, including EOS handling, requested probability rows and
seeded non-greedy output. The probability rows compare exactly.
A live nonfinite device-choice fallback was not generated in those
requests.

## RTX 4070, cold 32K/512

Both arms use the same binary and image, raw EXP-1977 IDs, context
36864, batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123, EOS masked,
MTP off and AVX2 host choice. GPU choice is disabled in both arms
for this diagnostic, so every continuation needs full logits.
Only `BONSAI_HTTP_DIRECT_LOGITS` changes. Both cold gates pass
and 512 returned IDs and text match PrismML.

| Arm | Prefill, tok/s | Decode, tok/s | Decode, s | Client, s | Post-request VRAM, MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A · Exchange buffer | 1101.91 | 38.85 | 13.155 | 42.954 | 8932 |
| B · Direct output | 1086.80 | 38.74 | 13.190 | 43.405 | 8932 |

Decode takes 35 ms longer and the request 452 ms longer in B, with
413 ms of the difference in prefill. The experiment only routes
decode, so the prefill variation is not assigned to the change.
Both have zero process swap. This one pair does not establish a
decode regression or support promotion. There is no separate trace
for B. Reuse the trace in EXP-2101 for the AVX2-only reference
dispatch. Source and artifact checks establish that B changes host
copy placement with identical kernel calls and geometry.

Continue the CPU audit on the retained combined profile, where
ordinary greedy decode does not move full logits. Distinguish CUDA
submission and synchronization, host allocations and the remaining
application work before selecting a broader CPU optimization. The
short correctness checks in this experiment do not characterize
non-greedy CPU throughput.

## Reproduce

Use the EXP-2101 build directory and artifact tree. Scope the two
flags consistently: AVX2 on, device choice off in both diagnostic arms.

```bash
exp=experiments/EXP-2026-11-26-2102-bonsai-direct-host-logits
out=build/bonsai-direct-host-logits-2102
bash "$exp/source/build-http.sh" "$out" build/bonsai-http-avx2-greedy-2101
```

Use the EXP-2101 profile with the rebuilt binary and set
`BONSAI_HTTP_DIRECT_LOGITS=0` or `=1` for the two arms. Run
the [exact request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
and the cold validator before and after each arm.

[Full pair and effective settings](data/results.json).
