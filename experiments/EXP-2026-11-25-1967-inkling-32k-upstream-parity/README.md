# EXP-2026-11-25-1967: Confirm Inkling 32K/512 against upstream

**Verdict: both paired comparisons favor Goose in prefill and decode.**
This is the starting baseline for the next Inkling optimization stage.
See the [current performance profile](../../docs/INKLING_PERFORMANCE.md).

One first request per fresh server on RTX 4070, in the order
llama.cpp → Goose → Goose → llama.cpp. The weights were cached by the
OS and neither engine reused prompt tokens. Both used the same GGUF,
32K input IDs, 512-output budget, sampler, context 36864, batch 4096
and Q4_0 K/V. Goose uses ubatch 4096, and the upstream run uses
ubatch 128 with the fixed EXP-1955 placement (7675.91 MiB weights and
968.43 MiB compute on CUDA). Goose diagnostic FNV state logging was
disabled for the performance comparisons. The exact internal state
gate at this size was recorded in EXP-1966, with logging enabled.

| First request | Prefill tok/s | Decode tok/s | Client s | Board peak MiB | VmHWM KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| llama.cpp A1 | 136.696 | 21.385 | 263.620 | 11369 | 15095136 |
| Goose B1 | 267.226 | 21.788 | 146.396 | 11745 | 14229348 |
| Goose B2 | 268.278 | 22.016 | 145.660 | 11742 | 14229232 |
| llama.cpp A2 | 137.300 | 21.303 | 262.644 | 11268 | 15095832 |

Goose averages **267.752 prefill and 21.902 decode tok/s** against
136.998 and 21.344 for llama.cpp. The throughput deltas are +95.44%
and +2.62%. Mean full request time is 146.028 s versus 263.132 s.
All four runs produced the same 512 IDs and text. Goose final
properties match, including expert cache counters. No process swap
or system swap-out. The storage I/O counters are in `results.json`.
No new Apex run or runtime change in this block.

Use `results.json` for exact argv, explicit env, unset env variables,
hashes and raw directories. The Goose baseline is the EXP-1966
executable with LFU, CUDA bridge and parallel softcap enabled. The
next full-request targets are 500 prefill and 25 decode tok/s.
