# Decode record: what it is, and why the box reads lower today

Status: **INVESTIGATION, no code change.** It answers why the accepted arms read
75-76 tok/s in this session while the saved block reads 80-81.

## The record, exactly

- binary: `/home/end0/AI/claude-apex-100/cand/split/frankenmoe-infer`,
  SHA-256 `faebfd30df38480ddc51131a1b47b526caf2ee17a3e2a2fd50384c51193d5a13`
  (identical to `checkpoints/1806-split-candidate/frankenmoe-infer`);
- source: `cand/split/src/cuda-driver.c`, which is the EXP-1805 build plus the
  EXP-1806 early post-FFN region submission (`GOOSE_DECODE_SPLIT`);
- environment beyond `goose-options.txt`: `GOOSE_DECODE_SPLIT=4` and
  `GOOSE_DECODE_SPLIT_COUNT=1`;
- argv: `--ctx-size 262144 --batch-size 4096 --ubatch-size 4096
  --cache-type-k q4_0 --cache-type-v q4_0 --expert-cache-mib 6290 --threads 8
  --threads-batch 16 --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1
  --cpu-strict-batch 1 …` (full text in `runs/split-ab-B3-20261123-183735/argv.txt`);
- result (`runs/split-ab-B3-20261123-183735/r2.json`, 2026-11-23 18:37):
  prefill 3103.8 tok/s, decode 81.292 tok/s, `prompt_ms` 10557.5,
  `predicted_ms` 6286.0, client 16.933 s, text `f8664331bd30…`.

`checkpoints/exp1805-route-prefetch` (`844c5a16`) does **not** contain
`GOOSE_DECODE_SPLIT` at all (`grep -c` returns 0), so any arm run from it reads
about 1.2 % lower even with the variables set. This session's first A/B block was
built from that checkpoint without the split variables, which is why its baseline
read 75.9 instead of 80.5. That part was my error, not a regression.

## Why today's numbers are 5-10 % lower: the machine, not the binary

The same record binary with the same environment was re-measured today:

| when | binary | env | decode tok/s |
| --- | --- | --- | ---: |
| 18:37 (record) | faebfd30 | split 4 / 1 | 81.292 |
| 20:28 | faebfd30 | split 4 / 1 | 73.822 |
| 20:31 (block) | faebfd30 | split 4 / 1 | 71.159 |
| 20:33 (block) | faebfd30 | split 4 / 1 | 68.915 |

Identical binary, identical `env.txt`, identical `argv.txt`, identical SLRU
counters (`cuda_expert_hits` 1 995 360, `cuda_expert_fills` 95 076,
`cuda_expert_upload_bytes` 181 409 415 168, `cuda_pinned_expert_bytes`
19 504 005 120 in both blocks), identical GPU clocks after the run
(SM 2835 MHz, memory 10251 MHz), 8774 MiB peak, VmSwap 0.

What differs is the host:

```
10:20  load average 2.89 2.37 2.15
       igorek-factory 39 % CPU, gnome-shell 12 %, ai.opencode 9 % + 7 %,
       ChatGPT 6.5 %, python3 6.3 %, Telegram 5.5 %, opencode-cli 2.3 %
       /swapfile 32 GiB, 22.9 GiB used;  RAM 60 GiB, 15 used
       GPU compute apps: igorek-llama-server 314 MiB, ptyxis 149 MiB,
       nautilus 85 MiB, ai.opencode 274 MiB, Telegram 196 MiB
```

Decode keeps a serial host loop on eight masked CPUs, so a loaded box costs
processor time directly; the same effect is why the prefill axis reads 2800-2950
instead of 3104 in this window.

**Consequence for the protocol:** absolute figures from different blocks are not
comparable today. Only paired ratios inside one block mean anything, and a block
with 68.3-75.3 spread cannot certify a 1-4 % effect.

## What was fixed and what stands

- The prefill change was rebuilt on the cumulative baseline instead of the older
  checkpoint: `checkpoints/cand-prefill-jobpack-split/frankenmoe-infer`,
  SHA-256 `37ac0f1d570dbec705f82c3b51e2437e9eec655f0da6bb69fcdf61bb8a1cf5e8`,
  source = `cand/split` plus the asynchronous job pack. Nothing accepted is lost:
  route prefetch and the decode region split are both in it, and the decode path
  keeps the original upload for packs of 4096 bytes or less.
- On the cumulative base the last block read prefill 2839.8 -> 2881.1
  (+1.45 %, pairs +1.03 % and +1.86 %) with decode inside the noise, so the
  prefill gain is not certified there yet. The certified figure, +4.00 % (pairs
  +4.20 % and +4.17 %), was measured against `844c5a16` in a calmer block and is
  recorded in EXP-2026-11-23-1808.

## Next

1. Confirm the prefill gain on the cumulative base with three alternating pairs
   in a quiet window, and keep the +4 % claim only if the pairs agree.
2. Re-measure decode on `faebfd30` in the same quiet window; the record stands
   until a block reproduces or beats 81.292 with a clean spread.
3. Continue the prefill axis (host per-copy overhead, `gg_div`/`gg_mod` in the
   per-token job loop) with the cumulative candidate as the base.
