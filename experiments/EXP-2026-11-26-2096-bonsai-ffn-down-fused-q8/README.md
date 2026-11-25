# EXP-2026-11-26-2096 — Fused Q8 Bonsai FFN down

**Retained in the cumulative experimental checkpoint.** Switch
full Bonsai FFN down tiles to the existing paired-selector,
panel-major INT8 MMQ strategy. The stable runtime and the
other architectures' artifacts are unchanged. Expanded combined
quality acceptance remains open.

The producer runs the existing SwiGLU and signed FWHT, then
quantizes each 128 activation values directly to Q8 without a
global F32 intermediate. It writes the bytes and one FP32 scale
in the layout consumed by the new 17408-to-5120 M64 × N128 entry.
The producer and consumer are paired only for complete 128-token
tiles, with a scratch-capacity check and binding to the same down
projection input. Remaining prefill tiles and count-one decode
retain their original paths. The allocation size is unchanged.
Compared with EXP-2040's earlier down path, this experiment keeps
fused activation preparation and uses panel-major staging and
paired weight selectors. It changes arithmetic relative to FP16.

SM89 uses 79 registers and 28,416 B shared memory for MMQ and
25 registers and 4,096 B for the producer, without spills.
All 98 existing CUDA text sections are byte-identical.

## Component and targeted quality

RTX 4070, four real PQ2_0 down matrices, 1024 seeded activation
rows and real Hadamard signs. Eight observations per arm in
balanced order, including SwiGLU, FWHT, pack and projection.

| Tensor | FP16, ms | Fused Q8, ms | Time saved | Relative L2 |
| --- | ---: | ---: | ---: | ---: |
| Layer 0 down | 3.09962 | 2.07302 | 33.12% | 0.6420% |
| Layer 21 down | 3.08917 | 2.06746 | 33.07% | 0.6426% |
| Layer 42 down | 3.10208 | 2.07664 | 33.06% | 0.6422% |
| Layer 63 down | 3.08680 | 2.07198 | 32.88% | 0.6434% |

Both paths produce finite values and exact zeros for the zero-input
row. All cases exceed the 10% component time-reduction threshold.

With ubatch 128 in both short diagnostics, CodeTrace gives 22/63
versus 21/63. The first free-generation divergence is at token 60.
All 11 fixed-history top-1 samples agree, with minimum top-10
overlap nine and maximum absolute logit difference 0.451069.
Both runs answer the independent fact-extraction task correctly
and produce the same 47 IDs. All nine sampled top-1 IDs agree,
with maximum absolute logit difference 0.546594. These observations
cover the two specified tasks. Broader quality is pending.

## RTX 4070 HTTP 32K/512

All four valid runs are listed. Same exact GGUF, raw request,
binary, CUBIN, frozen PTX, effective argv and affinity. Context
36864, batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123, EOS masked
and MTP off. Only `BONSAI_FFN_DOWN_Q8_FUSED` changes. All four
pass the cold prefill verification, with reset model state,
32,768 evaluated input tokens and zero reuse. Prelaunch clocks and
temperature were observed, with unlocked clocks. The second pair
reversed the order after decode slowed in the first.

| Run | Prefill, tok/s | Decode, tok/s | Prefill, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 1026.19 | 37.94 | 31.932 | 45.424 | 8932 | 7457460 |
| B | 1088.99 | 37.67 | 30.090 | 43.678 | 8932 | 7457520 |
| B2 | 1100.04 | 37.99 | 29.788 | 43.262 | 8932 | 7457140 |
| A2 | 1022.56 | 37.95 | 32.045 | 45.535 | 8932 | 7457896 |
| Mean control | 1024.37 | 37.94 | 31.988 | 45.479 | 8932 | — |
| Mean candidate | 1094.51 | 37.83 | 29.939 | 43.470 | 8932 | — |
| Saved PrismML control | 1076.60 | 47.63 | 30.436 | 41.174 | 8288 | 7651032 |

Prefill saves 1.842 and 2.257 s. Mean prefill throughput rises
6.85% and mean full request saves 2.009 s. Mean decode throughput
drops 0.29%, with differences of opposite sign across the pairs.
All four match PrismML on 512 output IDs and text. VRAM and peak
RAM are comparable between A and B, and swap is zero.

The mean exceeds the saved 1076.60 tok/s PrismML control by 1.66%.
The original EXP-1977 card also reports a separate 1093.21 tok/s
reference. Contemporary paired parity with PrismML remains to
be established. Decode and +70% goals remain open. No new Apex
or Inkling model runs were made, their packaged artifacts match
the stable package byte-for-byte.

## Executed graph

One separate cold-gated Nsight Systems capture of B, with 512
vocabulary readbacks, complete output comparison and 8,944 MiB
post-request VRAM. It is not part of the unprofiled pairs.
Compare it with EXP-2094, the last full control trace. The
intervening EXP-2095 changed the long decode tile unpack; the
full-tile prefill FFN down and producer are unchanged between
EXP-2094 and the EXP-2096 A arm.

| Prefill kernel family | EXP-2094 | EXP-2096 |
| --- | ---: | ---: |
| Full down | 1,984 FP16 launches, 5.343 s | 1,984 INT8 launches, 3.436 s |
| Fused activation producer | 2,048 FP16 launches, 0.854 s | 1,984 Q8 and 64 FP16 launches, 0.801 s |
| Sum of listed kernels | 6.197 s | 4.238 s |

The 1.960 s kernel delta is diagnostic. The scored paired
prefill and client gains are above. The traced request gives
1101.40 / 37.55 tok/s and 43.384 s client time. All kernel
inventories, copy activity, graph nodes, dispatch geometries
and trace SHA-256 values are saved in the attribution receipt.

## Reproduce

Use the EXP-2095 build directory and profile. Set
`FRANKENMOE_CHECKPOINTS` to the artifact tree. Build with Clang 18,
CUDA ptxas for SM89 and GCC 13.

```bash
exp=experiments/EXP-2026-11-26-2096-bonsai-ffn-down-fused-q8
out=build/bonsai-ffn-down-fused-q8-2096
base=build/bonsai-decode-q4-half2-unpack-2095
bash "$exp/source/build.sh" "$out" "$base"
python3 "$exp/source/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin"
FRANKENMOE_BENCH_COLD_LOG=1 bash "$exp/source/build-http.sh" "$out" "$base"
```

Use EXP-2085's `source/check.py CORE_DIRECTORY INVENTORY CUBIN q8`
for the component probe. For HTTP load the EXP-2095 profile and
set `BONSAI_FFN_DOWN_Q8_FUSED=0` for A and `=1` for B. Send the
[exact EXP-1977 raw request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
and use the cold-prefill validator before and after each arm.
Use the supplied quality fixture and adapter for the two short
cases. The raw diagnostic trace is at
`build/bonsai-ffn-down-fused-q8-2096/trace/trace.sqlite`.

[Component evidence](data/component.json), [CUDA identity](data/device-code.json),
[CodeTrace](data/quality.json), [fact extraction](data/incident.json),
[full pairs and cold receipts](data/results.json), and [executed graph](data/attribution.json).
