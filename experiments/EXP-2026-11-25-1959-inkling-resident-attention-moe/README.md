# EXP-2026-11-25-1959: Keep the Inkling attention-to-MoE bridge on CUDA

**Verdict: confirmed Inkling improvement in the measured block.**
Keep as an opt-in candidate pending a stronger Apex acceptance block
and qualification on other host math libraries.

Join attention and MoE in a retained graph for each Inkling decode
visit. The CUDA bridge consumes the resident attention output, computes
post-convolution, residual, RMS and DepthMix, and publishes FFN input
and residual as views of one allocation. Import the last three prefill
convolution rows on the first decode visit of each request. Maintain
separate device history and position per visit. The layer loop still
uploads the current residual and normalized depth history and reads
back the MoE output. The matching CPU executor remains available.

The node uses the EXP-1958 exact exponential on this RTX 4070 and
glibc 2.43 FMA host. Use `I2_JOIN_ATTENTION_MOE=1` only when that host
math path has been validated. The flag is experimental and disabled
by default. It selects only the Inkling decode path with retained
graphs, resident attention/MoE and normalized history enabled.
The first short probe completed two consecutive 128/16 requests
on one server and produced identical states and responses after reset.

An initial build mistakenly searched the host source directory before
the CUDA headers, replacing the accepted 32-row tile with older code
and cutting prefill to about 256 tok/s in both arms. The final build
separates the header search order and restores the EXP-1953 tile.
Exclude those earlier performance runs.

## Inkling 8192/256, sequential A–B–B–A

Same final binary, only the bridge flag differs. Context 36864,
batch/ubatch 4096, Q4_0 K/V, and 6720 MiB expert cache.

| Arm | Prefill tok/s | Decode tok/s | Decode ms | Client s | VRAM peak MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 277.597 | 19.848 | 12847.879 | 42.661 | 11720 |
| B1 | 277.300 | 20.453 | 12467.341 | 42.305 | 11722 |
| B2 | 277.099 | 20.313 | 12553.352 | 42.408 | 11722 |
| A2 | 277.183 | 19.895 | 12817.403 | 42.669 | 11706 |

The matched means are 277.390 → 277.199 tok/s prefill and
19.871 → 20.383 tok/s decode. Decode saves 381 and 264 ms in
the two pairs, or 322 ms on average. Mean client time is
42.665 → 42.356 s. All 256 output IDs, text, 8703 state
fingerprints and expert counters match. The candidate board
peak is 2 MiB above the maximum control and peak process RSS
does not increase. System swap-out and process swap are zero.
There is no new Nsight measurement, so the measured response
reduction does not separate the eliminated readback from other
launch and synchronization changes.

## Apex 32K/512

Use the EXP-1954 pinned argv/env and context 262144. The first
candidate request is disqualified by system swap-out. After
prewarming missing model pages, a fresh EXP-1953/EXP-1959 pair
returned all 512 IDs, text and final `/props` identically with
no swap-out during either request. Board peaks are 10726 MiB
in both arms. Control and candidate prefill are 2901.282 and
2863.528 tok/s, and decode 80.320 and 80.089 tok/s. Both had
startup swap-out outside the request. This is one pair and
does not qualify a non-regression claim. Production stays on
the frozen Apex and accepted Inkling checkpoints.

## Reproduction

Build with `bash source/build.sh /absolute/new/scratch` using
the local EXP-1953 archive and EXP-1958 CUDA image. To reproduce
the exact executable hash, set `ASSERT_SOURCE_DIR` to the
absolute original `build/inkling-apex-gate/resident-bridge-1959`
directory before building. GCC embeds that path in assertions.
The normalized scratch build is byte-identical.

Use the matching 8192/256 fixture from EXP-1955 or the frozen
Apex request from EXP-1880. Launch one process per arm with the
full argv/env from `results.json`, wait for `/health`, and
POST the fixture to `/completion`. Raw evidence is in
`build/inkling-apex-gate/resident-bridge-1959`.
