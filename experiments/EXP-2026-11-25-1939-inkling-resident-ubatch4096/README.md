# EXP-2026-11-25-1939: Scale the fused-SiTU Inkling profile to ubatch 4096

**Verdict: ACCEPTED as a higher-memory prefill profile.** The
same EXP-1937 executable supports both launch profiles. Keep
ubatch 2048 as the lower-VRAM reference and the faster observed
decode profile. The full larger-ubatch result has one run per
configuration.

## Capacity screen

EXP-1923 eliminated the previous large vector fallback at ubatch
2048. Fused SiTU reduces the number of simultaneously live large
activations. The candidate doubles the prefill microbatch without
changing context, Q4_0 KV, request tokens or model arithmetic.
In the initial 4096/128 screen, ubatch 4096 with a 896 MiB arena
and 6720 MiB expert limit reached the MoE gate/up operation.
A CUDA allocation for 150994944 bytes failed despite the subsequent
memory query showing 220135424 bytes free. The output operation
failed and the server aborted. The exact allocator failure cause
was not isolated. Core dumps were disabled for the probe.

The same ubatch and arena worked with the automatic expert budget.
A 736 MiB arena subsequently restored the explicit 6720 MiB cache
within the tested board memory. Both successful profiles use the
existing resident wide vector path.

## Paired screen, identical cache sizes

EXP-1937 executable, RTX 4070, context 36864, batch 4096,
Q4_0 K/V, the first 4096 tokens of EXP-1927 and 128 outputs.
Both arms allocated 6275543040 expert bytes. Run B–A–A–B,
one first request and repeat per launch with prompt recomputation
and matched state hashing.

| Paired mean | ubatch 2048, arena 576 | ubatch 4096, arena 896 |
| --- | ---: | ---: |
| First prefill, tok/s | 205.81 | 212.79 |
| Repeat prefill, tok/s | 210.05 | 219.13 |
| First decode, tok/s | 17.174 | 17.096 |
| Repeat decode, tok/s | 17.142 | 17.160 |
| Repeat prefill, ms | 19500.2 | 18691.9 |
| Repeat expert upload, GB | 20.46 | 6.84 |
| Repeat full request, s | 27.125 | 26.309 |

Repeat prefill gained 4.32% and won both pairs. All 8702
fingerprints per launch, both sets of 128 output IDs and text
match. The larger microbatch changed expert traffic as expected.
Board maxima were 10631 and 11221 MiB, and peak RSS increased
by about 413 MiB. Both arms had zero swap and swap-out.

## Full workload

Same 32K/512 fixture and executable. Each new profile saw one
fresh request. The comparison includes the original recorded
EXP-1937 run with the 6720 MiB cache. All 512 IDs and text
match. All completed with no process swap or system swap-out.

| Configuration | Prefill, tok/s | Decode, tok/s | Full request, s | Prefill uploads, GB | Peak VRAM, MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| ubatch 2048, arena 576, cache 6720 | 197.14 | 17.897 | 195.230 | 175.19 | 11280 |
| ubatch 4096, arena 896, auto cache | 202.66 | 17.077 | 192.082 | 89.54 | 11159 |
| ubatch 4096, arena 736, cache 6720 | 202.72 | 17.784 | 190.835 | 89.54 | 11725 |

The final profile saves 4.39 s end to end and 4.57 s of prefill.
Prefill throughput gained 2.83% and decode fell 0.63%, which
passes the percentage tradeoff rule for a prefill profile.
Peak RSS was 14264240 KiB versus 13842912 KiB in EXP-1937.
The final configuration leaves less board headroom and should
retain its exact memory settings in reproduction. The 500
prefill and 25 decode milestones remain open.

The production launchers and Apex state were preserved. The
frozen Apex checksum manifest passed. Apex performance was
outside this profile experiment.

## Reproduction

Use the EXP-1937 binary and the existing environment in
`results.json`. Set `--ubatch-size 4096`,
`GOOSE_CUDA_ACTIVATION_MIB=736` and `--expert-cache-mib 6720`
for the higher-memory profile. The context remains 36864 and
the batch remains 4096. `inputs.sha256` identifies the
binary and requests. Raw runs are stored under
`build/inkling-exp-baseline/device-attention/inkling-situ-ubatch4096`.
