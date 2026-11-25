# EXP-2026-11-25-2004 — Bonsai 32K/512 shared HTTP reference

**Full fixed-request correctness passed.** The EXP-2003 executable
completed the exact EXP-1977 token request through the normal
`/completion` path. All 512 generated IDs and the UTF-8 completion text
SHA-256 match the PrismML llama.cpp reference. No source, model, CUDA
image or release launcher changed. This single measurement sets the
native HTTP diagnostic reference. The shared binary still needs the
Apex and Inkling acceptance gates and the Bonsai chat integration.

| Metric | EXP-2003 HTTP | EXP-1977 PrismML |
| --- | ---: | ---: |
| Prefill, tok/s | 613.18 | 1093.21 |
| Decode, tok/s | 11.258 | 47.966 |
| Prefill, s | 53.439 | 29.974 |
| Decode, s | 45.390 | 10.653 |
| Client elapsed, s | 98.848 | 40.636 |
| Peak process VRAM, MiB | 8932 | 8288 |
| Peak board VRAM, MiB | 11379 | 10223 |
| Peak process RSS, KiB | 7,457,116 | 7,653,328 |
| Process RSS after request, KiB | 160,488 | 1,069,660 |
| Process swap, KiB | 0 | 0 |
| System swap-out during request, pages | 0 | 0 |

Both decode rates divide 511 intervals by the reported decode duration.
Relative to the historical reference, prefill and decode are 43.91% and
76.53% lower. The drivers have different weight placement: native
keeps unused MTP weights resident, while the reference skips them.
Board VRAM includes unrelated allocations. The ambient start was
2443 MiB for this run and 1935 MiB for the reference. Startup may dominate
high-water RSS because weight upload touches the file mapping.

## Conditions

RTX 4070 with driver 580.178.04, the same local PQ2_0 GGUF, context
36864, batch/ubatch 4096/1024, Q4_0 KV, greedy, seed 123,
ignore EOS, no prefix reuse and no MTP. The request and binary hashes
are saved in `data/inputs.sha256`. Exact process arguments and the
explicit environment are in `data/argv.txt` and `data/env.txt`.
The host exposes CPU-mask options, but sampled main-thread affinity
outside requests was 0–31. No claim relies on equality of effective
CPU masks or equivalence to a contemporaneous paired run.

One warm-ready server received one scored request from
[EXP-1977](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json).
No extra inference warmup or profiling was used. Board and per-process
GPU readings were sampled every 250 ms via `nvidia-smi`. RSS high-water
and swap were read from procfs before and after. The complete HTTP
response, memory samples and input hashes are in `data/`.

## Reproduction

From the repository root, after rebuilding or verifying the EXP-2003
binary, start it on a free loopback port using the recorded arguments:

```bash
ex=experiments/EXP-2026-11-25-2004-bonsai-http-baseline
mapfile -t argv < "$ex/data/argv.txt"
mapfile -t opts < "$ex/data/env.txt"
env -i "${opts[@]}" "${argv[@]}"
```

After health returns ready, submit the exact reference request:

```bash
curl --fail-with-body --silent --show-error --max-time 170 \
    -H 'Content-Type: application/json' \
    --data-binary @experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json \
    -w '%{time_total}\n' -o build/bonsai-http-response.json http://127.0.0.1:19104/completion
```

## Remaining loss map

The adjacent K64 diagnostic CUDA capture (32K/16, same graph and
images, standalone launcher) attributes 32.024 s of prefill to PQ2 MMQ
and 15.515 s to the main Flash chain, including 2.809 s for the ordered
merge. Those are CUDA times from a separate run. Nsight reported an
event-processing error, so the CPU timeline from that capture is
unqualified. The older full CPU/CUDA breakdown is in EXP-1995.

Within that capture's ~1.336 s decode GPU span, ordered PQ2 matvec takes
624.716 ms, while Q4 attention tiles and merge take 525.545 ms. These
are the largest measured decode groups. The native decode fallback
launches one tile CTA per query head and KV tile. For the Bonsai 6:1
case, a bounded diagnostic is to load a Q4 K/V tile once for multiple
query heads and preserve the original independent numerical order and
merge. First compare emitted partials and final attention against the
frozen image at representative lengths and tails. Any ceiling from
eliminating this group is an upper bound, not an expected gain. Measure
full decode only after component correctness and timings justify it.

The long native request needs another 46.886 s of prefill savings to
reach 5000 tok/s. The original match targets of 1858.46 prefill and
81.55 decode tok/s also remain open. This reference does not
promote an INT8 path or change the multi-request quality gate.
