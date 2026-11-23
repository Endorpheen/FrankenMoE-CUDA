# Apex checkpoint and run inventory — 2026-11-23

Written while recording the state of `claude/apex-100-prefetch` for the next stage
(prefill). Nothing here is a new measurement: every number is read from the saved
responses under `/home/end0/AI/claude-apex-100/runs/`.

Profile of every run below: Apex-35B-V1.0-Q4_K_M, context 262144, batch and ubatch
4096, KV q4_0, expert cache 6290 MiB, threads 8/16, masks 0f0f/ffff, request with
32768 prompt tokens and 512 generated tokens.

## Accepted checkpoint

`checkpoints/exp1805-route-prefetch/frankenmoe-infer`,
SHA-256 `844c5a161965463ad21fd5f6fa926c6ee7c3a2db0d43dfb5cb315b4bcb657f4c`.

| arm | decode tok/s | prefill tok/s | client s |
| --- | ---: | ---: | ---: |
| A1 | 79.665 | 3081.0 | 17.143 |
| A2 | 80.533 | 3083.3 | 17.068 |
| A3 | 80.036 | 3085.2 | 17.095 |

## Best measured candidate — EXP-1806, not accepted yet

`checkpoints/1806-split-candidate/frankenmoe-infer`,
SHA-256 `faebfd30df38480ddc51131a1b47b526caf2ee17a3e2a2fd50384c51193d5a13`,
byte-identical copy of the measured `/home/end0/AI/claude-apex-100/cand/split/`
artefact, with `SHA256SUMS` covering the binary, `cuda-driver.o`, the archive and
the source.

| arm | decode tok/s | prefill tok/s | client s | pswpout |
| --- | ---: | ---: | ---: | ---: |
| B1 | 80.699 | 3083.3 | 17.048 | 1002 |
| B2 | 81.200 | 3081.0 | 17.019 | 0 |
| B3 | 81.292 | 3103.8 | 16.933 | 3411 |

All three pairs favour B (+1.03 / +0.83 / +1.57 % decode), prefill is unchanged,
peak VRAM 8774 against 8770 MiB. The card records the blocker: two of the three
pairs had system swap-out, so a clean confirmation pair is still outstanding
before `GOOSE_DECODE_SPLIT=4` with count 1 becomes the default.

## Exactness

53 of 53 saved runs reproduce the 512 token IDs and the text
`f8664331bd30cac8124f48807211ec41b6d0b2b77e301839d8a605e8e2a6e4ce`.
The per-run table is `docs/APEX-RUN-INVENTORY.tsv` (run, binary SHA-12, exactness,
prefill, decode, prompt and prediction milliseconds, client seconds).

## Where prefill stands

Best prefill in the inventory is 3103.8 tok/s (the B3 run above); the usual band
is 3081-3104. The next objective asks for 3500 tok/s, which at 32768 prompt
tokens means `prompt_ms` falling from about 10557 to about 9362, that is 1.2 s
less inside the request. No prefill change is in the tree yet, and the only
prefill profile saved with this runtime covers decode only
(`runs/nsys-pf2-*/prof.sqlite` starts at the first decode step), so the first
prefill step is a profile of the prefill window itself.

## Current baseline checkpoint (owner-accepted, 2026-11-23)

`checkpoints/cand-prefill-jobpack-split/frankenmoe-infer`,
SHA-256 `37ac0f1d570dbec705f82c3b51e2437e9eec655f0da6bb69fcdf61bb8a1cf5e8`,
built from the EXP-1805 checkpoint plus the EXP-1806 decode region split plus the
asynchronous job-pack upload of EXP-1808. Owner's words: 3122.4 / 78.225 is the
baseline for this checkpoint.

| metric | value | source |
| --- | ---: | --- |
| prefill | **3122.4 tok/s** | `runs/cf-B3-20261123-205433` |
| decode | **78.225 tok/s** | same run |
| `prompt_ms` | 10494.5 | same run |
| client | 17.118 s | same run |
| exactness | 512 IDs and `f8664331bd30…` | all six arms of the block |
| peak VRAM | 8774 MiB | both arms |
| process swap | 0, pswpout 0 | both arms |

Conditions of that block: `GOOSE_DECODE_SPLIT=4`, `GOOSE_DECODE_SPLIT_COUNT=1`,
load average 1.16-2.74, three alternating pairs, medians A 2982.0 / B 3121.4
(+4.67 %) with decode medians 73.443 / 78.185.

Reference still quoted for a quiet machine: `faebfd30` with the same split
environment reads 3103.8 prefill and 81.292 decode (18:37), and the same change
applied to it is worth +4.67 %, about 3248 tok/s prefill.

## Targets and the comparison protocol (owner-confirmed)

Two targets: **prefill 3500 tok/s** and **decode 100 tok/s**.

Three figures that must never be confused:

| figure | value | binary | conditions |
| --- | ---: | --- | --- |
| current baseline | prefill 3122.4 / decode 78.225 | `37ac0f1d` | block of 2026-11-23, load 1.16-2.74 |
| historical record | prefill 3103.8 / decode 81.292 | `faebfd30` | 18:37, quiet machine, `GOOSE_DECODE_SPLIT=4` + `GOOSE_DECODE_SPLIT_COUNT=1` |
| certified prefill gain | +4.67 % | `37ac0f1d` vs `faebfd30` base | cf block, three alternating pairs |

Rules for every further measurement:

1. A candidate is compared with the arm measured in the same block, on the same
   machine state; the block records `ambient-start.txt`.
2. A single cold request is never a verdict: warm-up plus measured request.
3. Decode parity is judged inside the block, never against the 81.292 record
   taken on a quiet machine.
4. Exactness, peak VRAM, VmHWM and process swap accompany every accepted number.
