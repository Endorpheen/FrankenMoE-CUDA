# EXP-2026-11-24-1830 — Plan pipeline: hand-off verified, one over-read keeps it blocked

Status: **BLOCKED, not accepted.** Baseline `37ac0f1d` untouched (prefill 3122.4,
decode 78.225). Candidate preserved, not adopted:
`checkpoints/exp-pipe-blocked/frankenmoe-infer` = `ede1571c9a614942de56d523…`.

## Why this mechanism

EXP-1828 split the 4.9 s of "synchronising plan copies" into **5.954 s of GPU
queue wait** and **0.011 s of copy**. The request therefore alternates: about
6.4 s of host work with an idle GPU, then about 6.0 s of GPU work with a spinning
host. Overlapping a quarter of the GPU phase would pay the 1.13 s the 3500 target
needs. That is the only measured reserve of sufficient size.

## What was built

Two device slots for `cuda.mmq_tiles`, a dedicated copy stream, one pinned staging
area per slot, and a per-slot free event recorded **after** the launches that read
the slot. `GOOSE_PLAN_PIPE=0` restores the old synchronising copy.

Four defects were found and fixed while bringing it up, each with a witness:

| defect | evidence | fixed |
| --- | --- | --- |
| slot stride rounded to 1 MiB while the reservation was `2 x tile_bytes` | 5319 of 5320 copies fell back (`runs/pipe-070820`) | reservation aligned to the same stride |
| `launch_mmq_stream` and its fixup read `cuda.mmq_tiles.address` instead of the slot | stream-workers path always used slot 0 | the slot is passed in |
| one shared pinned staging area for both slots | DMA still reads its host source after the call returns | one staging area per slot |
| the jobs ring's event marked the copy, not the kernels reading the jobs | ring slot reused while a kernel still read it | the ring is fenced after the launches |

## What the evidence now shows

1. **The plan hand-off itself is correct.** With `GOOSE_PLAN_VERIFY=1` every slot
   is read back and compared with the host array:

   ```
   Goose plan verify: slot 0 3072 bytes ok
   Goose plan verify: slot 1 3072 bytes ok     (20 consecutive projections, no MISMATCH)
   ```

2. **The pipeline is the sole cause of the failure.** One binary, one request,
   one flag apart:

   | arm | result |
   | --- | --- |
   | `GOOSE_PLAN_PIPE=1` | prefill dies, `CUDA error 700` (`ILLEGAL_ADDRESS`) |
   | `GOOSE_PLAN_PIPE=0` | prefill passes, 1223.0 ms for the 1024-token probe |

3. **`compute-sanitizer` names the fault deterministically**, identical in two
   different builds and two runs:

   ```
   Invalid __global__ read of size 4 bytes
       at goose_mmq_q4_reference+0x2ad0
       by thread (0,0,0) in block (1,20,0)
       Address 0x7844c7fdebd0 is out of bounds
       and is 912 337 bytes after the nearest allocation at 0x7844c7e00000 of size 1 048 576 bytes
   ```

   4-byte reads with the same offset (0x1DEBD0 past a 1 MiB allocation) in every
   run mean the kernel is reading a 1 MiB staging buffer *past its end*, by a fixed
   amount — not a torn plan, not a lost copy.

4. **`CUDA_LAUNCH_BLOCKING=1` makes the same binary pass** (`runs/pipe6-071447`),
   so the surviving defect is an ordering assumption that the old synchronising
   copy satisfied silently: something the matrix kernel reads is still being
   written when it reads it.

## What is not yet established

Which 1 MiB buffer is over-read. The candidates are the jobs ring staging, the
packet staging (`cuda.packet_host`, 1 MiB, used by `upload_packet`), and an
expert host slot; the plan staging and the device slots are excluded because the
hand-off is verified byte-for-byte and the sanitizer reports the fault *outside*
every allocation, not inside a slot.

The old synchronous `cuMemcpyHtoD` per projection was not only a copy: it was an
implicit **barrier** for at least four separately-managed staging rings (plan,
jobs, packets, expert slots). Every one of them has to be fenced explicitly before
the serialisation can be removed. Four are addressed; at least one is not, and
finding it needs a watch of the actual launch parameters at the failing
projection rather than another blind rebuild.

## Consequence for the target

The lever stays real and measured (6.4 s host / 6.0 s GPU, zero overlap, quarter
overlap pays 1.13 s), but it is not delivered: the only pipeline built so far
breaks correctness. **Prefill remains 3122.4 tok/s, 8 % short of 3500**, and no
other measured segment has the required size (EXP-1825, 1826, 1827).

## Artefacts

- `checkpoints/exp-pipe-blocked/` — candidate binary and source, `SHA256SUMS`,
  explicitly **not** an accepted reference.
- `runs/san-071650`, `runs/san2-072109` — `compute-sanitizer memcheck` output.
- `runs/ver-071919` — plan readback verification (20 × ok), request still fails.
- `runs/pipe7-072042` — jobs-ring fence build, still fails.
- `runs/pipeoff-072156` — same binary with `GOOSE_PLAN_PIPE=0`, passes.
- `runs/pipe6-071447` — `CUDA_LAUNCH_BLOCKING=1`, passes.
- `runs/pipe4-071300`, `runs/pipe5-071416` — earlier failures with their logs.
