# EXP-2026-11-26-2093 — Direct Q4 FP16 MMA for long Bonsai decode

**Retained in the cumulative experimental checkpoint.** The stable
runtime and other architecture artifacts are unchanged. Expanded
quality acceptance remains open.

Specialize the existing direct-Q4 Flash tile for one query and six
query heads per KV head. Stage one Q4 K/V tile per group and unpack
to FP16 in shared memory. Use FP16 MMA with FP32 accumulators for
QK and PV. Keep the 256-row partitions, original head-major
partial buffer, FP32 mass and existing ordered merge. FP16
operand and probability rounding, as well as within-tile
reduction order, differ from GQA3. The HTTP preset uses the new
kernel only for a single query with at least 32768 visible keys.
This includes the final prefill evaluation at 32768 and the 511
continuation evaluations for the scored workload. No new resident
allocation. The kernel uses 72 registers, 47,872 B shared memory,
and no spills. The prior 95 CUDA text sections are byte-identical.

## Component and targeted quality

RTX 4070, seeded queries and Q4_0 K/V encoded by the frozen Goose
image. Ten observations per arm in balanced order, including
the existing merge. All outputs are finite.

| Visible keys | GQA3, ms | Direct MMA, ms | Relative output L2 |
| ---: | ---: | ---: | ---: |
| 32768 | 0.53038 | 0.45050 | 0.02924% |
| 32769 | 0.53296 | 0.45005 | 0.02887% |
| 33279 | 0.53594 | 0.45517 | 0.02961% |

All cases clear the 10% time-reduction threshold. The maximum
absolute output delta is 5.24e-6.

The short diagnostic explicitly forces the candidate for shorter
single-query contexts. Its control uses normal short-context GQA.
Both use ubatch 128. CodeTrace is 22/63 versus 16/63, with the
first free-generation divergence at token 60. All 11 sampled
fixed-history top-1 IDs agree. Minimum top-10 overlap is nine,
and maximum absolute logit difference is 0.233290. Both runs
pass the independent fact extraction task with identical 47 IDs.
All nine sampled top-1 IDs agree there, with maximum absolute
logit difference 0.387548. The short diagnostic does not establish
quality at the production context threshold.

## RTX 4070, HTTP 32K/512

All four valid runs are listed. Same exact GGUF, raw request,
binary, image, frozen PTX, effective argv and affinity. Context
36864, batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123, EOS masked
and MTP off. Only `BONSAI_DECODE_DIRECT_MMA` changes. All four
arms pass the cold gate, with state reset, 32,768 evaluated input
tokens and no reused prompt tokens. Prefill slowed in the first
pair, so the second pair reversed the order.

| Run | Prefill, tok/s | Decode, tok/s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A | 1026.91 | 32.84 | 47.491 | 8932 | 7457784 |
| B | 1012.15 | 34.56 | 47.182 | 8932 | 7457812 |
| B2 | 1024.51 | 34.71 | 46.727 | 8932 | 7457988 |
| A2 | 1020.89 | 32.75 | 47.720 | 8932 | 7457668 |
| Mean control | 1023.90 | 32.80 | 47.605 | 8932 | — |
| Mean candidate | 1018.33 | 34.64 | 46.954 | 8932 | — |
| Saved PrismML | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Decode gains 5.61% and saves 827 ms on average, with both pairs
faster. Mean prefill takes 176 ms longer, with opposite-signed
differences across the pairs. Full request saves 651 ms on
average. All four outputs match all 512 PrismML IDs and text.
Process swap is zero and VRAM is unchanged. The mean prefill
deficit to PrismML is 1.743 s. Decode parity and the two +70%
targets remain open. No new runs were made on Apex or Inkling.

## Executed graph

One separate cold-gated Nsight Systems capture on B. The capture
retains all 512 vocabulary readbacks and matches all IDs and text.
The executed graph shows 16 direct-MMA calls in final prefill
and 8,176 in decode, replacing equally many GQA3 launches. The
existing merge still runs 8,176 times in decode. Kernel inventories,
phase boundaries, copies, graph-node counts and trace hashes are
in the receipt.

| Decode trace measure | EXP-2086 | EXP-2093 |
| --- | ---: | ---: |
| GQA3 or direct MMA | 3.248 s | 2.339 s |
| Ordered Q4 merge | 1.081 s | 1.109 s |
| Combined attention | 4.329 s | 3.447 s |
| GPU gaps | 1.024 s | 0.960 s |

Post-request VRAM under tracing is 8,944 MiB. Traced timings
46.823 s client and 1025.96 / 34.38 tok/s are diagnostic only.
PrismML's measured attention plus fixup is 0.950 s in EXP-2091.
The next substantial decode target is direct attention scheduling
and reducing the ordered merge cost. Prefill priority remains PQ2.

## Reproduce

Use the EXP-2086 build directory and profile. Set
`FRANKENMOE_CHECKPOINTS` to the artifact tree. Build with Clang 18,
CUDA ptxas for SM89 and GCC 13.

```bash
exp=experiments/EXP-2026-11-26-2093-bonsai-decode-direct-mma
out=build/bonsai-decode-direct-mma-2093
base=build/bonsai-q8-paired-selector-2086
bash "$exp/source/build.sh" "$out" "$base"
python3 "$exp/source/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin"
FRANKENMOE_BENCH_COLD_LOG=1 bash "$exp/source/build-http.sh" "$out" "$base"
```

Use EXP-2005's `source/check.py CORE_DIRECTORY INVENTORY FROZEN_PTX
CUBIN --decode-mma` for component checks. For HTTP set the EXP-2086
overrides with `BONSAI_DECODE_DIRECT_MMA=0` for A and `=1` for B.
Send the [exact EXP-1977 raw request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
and use the cold-prefill validator before and after each arm.
The raw trace is at `build/bonsai-decode-direct-mma-2093/trace/trace.sqlite`.
The supplied quality adapter selects the documented short diagnostic
scope and uses EXP-1994's fixture and logits replay.

[Component evidence](data/component.json), [CUDA identity](data/device-code.json),
[CodeTrace](data/quality.json), [fact extraction](data/incident.json),
[full pairs and cold receipts](data/results.json), and [executed graph](data/attribution.json).
