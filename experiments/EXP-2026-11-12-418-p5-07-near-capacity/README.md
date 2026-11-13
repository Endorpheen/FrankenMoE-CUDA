# EXP418: P5-07 near-capacity, save/restore and rewind witness

P5-07 was open on one runtime clause: the existing witnesses used a 32K prompt
in a 256K context, so near-capacity reuse was never exercised. This experiment
runs the accepted runtime with only the context size changed, then exercises
save/restore and rewind at 86 percent occupancy.

## Configuration

Accepted runtime `runtime/exp244/run`, unmodified libraries. The argv is the
recorded fixed profile with two changes only:

- `-c 262144` -> `-c 512`, so a 441-token prompt is near capacity;
- a temporary `--slot-save-path`.

Everything else is the accepted profile: `-ngl all`, `--cpu-moe`, `-fa on`,
KV `q4_0`, batch/ubatch 4096, 8/16 threads, `-np 1`, `--cache-ram 0`,
`--ctx-checkpoints 32`, `--spec-type none`, temperature 1, seed 123.

## Result

| check | result |
|---|---|
| prompt tokens / context | **441 / 512 = 86.1 percent** |
| save | ok |
| restore | ok |
| completion A before and after restore | tokens equal, all 5 saved probabilities equal, content `ef797c8118f02dfb...` equal |
| erase slot (rewind) | ok |
| short completion B before the long prompt and after erase | tokens equal, probabilities equal, content `02ec2f94ed87f74b...` equal |
| server shutdown | SIGINT, rc 0 |

The long completion stopped at 8 tokens with `stop_type = limit`; the short one
stopped at EOS with 3 tokens. The slot file is 134 322 996 bytes. Raw receipts:
`runtime/exp418/build/{a1,a2,b1,b2,save,restore,erase,summary}.json`.

## Integrity finding

The first attempt used the argv path recorded in
`results-apex-or244-or-goose/argv.json`, which points at
`measured-apex-or244-or-goose/run`. That directory is **not self-contained**:
`libggml-cuda.so.0` fails with `undefined symbol: fg160_fusion_enabled`. The
preserved `runtime/exp244/run` starts normally and is the accepted runtime, so
the test uses it. This is recorded because a path in the accepted receipts is
not directly runnable; the working copy is the self-contained checkpoint.

## How this closes P5-07

- save/restore exact: this run and EXP293/EXP364;
- rewind exact: this run and EXP308;
- branch: the state round trip restores into a fresh process with an identical
  continuation (EXP293), which is the branch contract available in this server;
- near-capacity: this run at 86.1 percent;
- server prompt cache: scoped to P8-03/P8-04 by the owner closure decision of
  2026-11-12, because it is server-side prefix reuse, not KV state ownership.

## Reproduce

    python3 experiments/EXP-2026-11-12-418-p5-07-near-capacity/run_near_capacity.py

Requires the Apex model and one free GPU; the script refuses a busy port and
always stops its own server with SIGINT.
