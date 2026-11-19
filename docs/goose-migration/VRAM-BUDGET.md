# The VRAM budget, measured rather than assumed

## What the GPU holds right now

```
nvidia-smi --query-gpu=memory.used,memory.total  ->  3191 MiB, 12282 MiB
```

```
igorek-llama-server ......... 314 MiB
Chrome GPU process .......... 653 MiB
OpenCode GPU process ........ 351 MiB
ptyxis ...................... 240 MiB
nautilus .................... 113 MiB
```

## The arithmetic, and why the record profile cannot fit

The record build needs **9511 MiB** of the 12282 MiB card (`APEX-BASELINE.md`, EXP1298). Today 3191 MiB is already in use:

```
9511 + 3191 = 12702 MiB   >   12282 MiB available
```

The deficit is **420 MiB before anything grows**, so an OOM under the record profile is what this arithmetic predicts, with
no code change involved. The failing requests of 1376-1567 MiB are the pool asking for a new block when the room left is
smaller than the block - a consequence of the same deficit, not a separate fault.

This also explains the lean configuration working: `--moe-cache-mib 4500` and `-b 2048 -ub 2048` give back roughly 600 MiB,
which is the order of the deficit, at the cost of decode.

## What changed in the code, checked

Everything committed since the record build touches host memory only:

- `llama-moe-cache.cpp`: every added allocation is a `std::vector` on the host (state snapshot, maps, seen/scal);
- `llama-vocab.cpp`: the served encoder and the piece-length contract - host buffers;
- `server-context.cpp`: guards and the AWT call-site markers - host;
- no change to KV allocation, graph reservation, ubatch sizing or the expert cache's VRAM size.

The expert cache stays 4992 MiB as configured: it is allocated once from the same size parameter, and the policy work does
not allocate on the device.

## What must be measured to settle it

The claim "the code is unchanged in VRAM" needs one number: the peak of the record profile on a quiet GPU.

1. stop the desktop GPU consumers (Chrome, OpenCode renderer) - measured above at 653 and 351 MiB;
2. run `bash tools/awt/run.sh CHECK vram-quiet` with the record profile;
3. read `results/awt/vram-quiet-*/vram_trace.txt`:
   - `this server peak` must land at 9511 +- 100 MiB, the record;
   - `everything else` must stay flat;
   - `growth of this server over the run` must stay near zero; a growth above 1024 MiB is reported as a warning by the
     runner and would be the leak this investigation is looking for.

A peak above the record by more than 100 MiB would be a real regression, and the first commit of a bisect is then the
subject. A peak equal to the record means the OOM was the budget, and the answer is the quiet GPU, not a smaller cache.

## The gate, and its baseline

`tools/awt/check-vram.py` is now part of every AWT run's verdict: a peak more than 300 MiB above the baseline, or growth
across the run above 512 MiB, fails the run the way a wrong answer does. The baseline comes from a real run, not from a
number chosen to pass:

```
check-vram: samples=28 seconds=15
check-vram: this server start=6596 peak=7994 growth=1398 MiB
check-vram: everything else start=2415 peak=2420 MiB
```

Baseline file: `docs/goose-migration/VRAM-BASELINE.json`.

The gate was checked against a doctored profile 1000 MiB above the baseline and rejected it on both counts, so it is a
check that can fail. The 1398 MiB growth it records is the graph reserve growing to the largest shape the session reaches;
that is the current behaviour, and it is what a future change is measured against.

## What the numbers say about the investigation

During the run the server grew by 1398 MiB while everything else on the card stayed flat (2415 -> 2420). The same run
logged `graph reallocs = 6`, so the growth is graph reservation as new batch shapes appear, not a per-request leak. A leak
would keep growing in the same shape; this is the reserve finding its size.

The failing requests in the report were 1376-1567 MiB, which is the same order as this growth - which is why the budget
matters: the reserve needs that room once, and the card had 3191 MiB of desktop load. Whether the reserve is larger than it
was in the record build is a question for a paired run on a quiet card, and the gate above is what answers it.
