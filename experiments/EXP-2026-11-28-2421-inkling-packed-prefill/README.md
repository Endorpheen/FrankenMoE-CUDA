# EXP-2026-11-28-2421 — Unroll Inkling packed prefill value accumulation

Expand the four tensor-core V accumulators in the packed Inkling prefill
path. Each accumulator uses the same value loads, MMA operands and
accumulation order. The shared native device fragment retains its inline
annotation when the build extracts it for the Inkling CUDA image wrappers.

The extracted SM89 prefill cubin uses 72 registers with no local memory.
The EXP-2420 kernel used 55 registers. All four component cases matched
every output and metadata word, including incomplete query and key tiles.
The 128-query, 32768-key launch went from 7529.85 to 5923.08 µs in the
component profiler. The device build used Fried Goose v0.3.0-preview
Keelwing, fgoose v0.3.1-dev.5 Keelwing, Clang 18 and ptxas 12.6.

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each clean run started in a new process after warming the model file cache
and evaluated the entire prompt. The candidate was measured before and
after the immediate EXP-2420 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 443.35 | 33.60 | 89.447 | 13570140 | 9822 |
| EXP-2420 control | 434.59 | 34.06 | 90.748 | 13571456 | 9822 |
| Candidate 2 | 444.88 | 33.30 | 89.344 | 13570896 | 9822 |

The immediate pair improved prefill by 2.3681% and reduced decode by
2.2197%. The two candidate observations averaged 444.11 prefill and
33.45 decode tok/s. The prefill gain exceeds the decode loss under the
current experiment acceptance rule. All 512 generated token IDs, the
text and the stop type match EXP-2128 in all three clean runs. No scored
disk reads, process swap or system swap-out were observed. Qualification
against the simultaneous 25% targets remains open.

An earlier full candidate request also matched all 512 IDs. It incurred
3823923200 scored disk-read bytes and 443 additional system swap-out
pages and was excluded from the throughput comparison.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2420 control executable | `eb36020f88e0701e93c55c0ae9f0574d1d9d9598e899cec137623d55807151d3` |
| Candidate executable | `e65609d28f60c7e59fe0d30300c09e051c6c5785d869a8c19acc22a98b2e0fbe` |
| Candidate Flash tiles cubin | `81d9b9c3f37fe2ba254eb197a5c73dbe15031e07b174b60ed7174333f46c1b4a` |
| Inkling GGUF | `dbedbc18d04fc667df5c17772f0091cf21c763c896f504c088f976f19bbe75c0` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `8377b79782885fce9a18fd06ce6fdaddbdfb9bbfe7fee8e647dffcfe95f1ed70` |
| EXP-2420 control response | `8cd28fa1b775e86ec1b8722e114a17981f75258e6ae639ca1052bc84535e65e8` |
| Candidate 2 response | `89cf0ac0748d587253739df5d9c6b606ca97fd6be44100bc072d1ef4412cd3b0` |
