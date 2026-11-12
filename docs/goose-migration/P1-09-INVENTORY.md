# P1-09 inventory: expert work distribution and synchronisation ownership

Status: inventory step, item stays IN_PROGRESS.

## Live in Goose on the accepted profile

Symbols present in `libggml-cpu` and observed in the accepted run log:

| module | symbol family | seen |
|---|---|---|
| FG143 | control, cpu, executor | yes (1/2/1 lines) |
| FG147 | backend | 21 lines |
| FG148 | quant | 1 line |
| FG149 | pool | 7 lines |

## Present but not executed

| module | symbols | status |
|---|---|---|
| FG140 | `barrier`, `claim` | in the library, **no witness in any recent run** |
| FG141 | `barrier`, `claim` | same |

So the job/pool/executor lifecycle is Goose and live, but the **claim/barrier** half - the part the requirement calls "expert
chunk claims ... only primitive OS atomics/thread waits imported" - is linked and dormant on this profile.

## What that means for the requirement

The criterion asks for Goose ownership of partitioning, job queues, claims, barrier state and cancellation, with
deterministic outputs at 1/2/8/16 threads. Current evidence:

- partitioning/pool/executor: Goose and live;
- claims/barrier: present as code, not on the executed path, so ownership cannot be claimed;
- determinism at 8/16/32 batch threads: verified on the model (all produce `e68df05b`), which covers the "deterministic
  outputs" clause;
- physical allocation, OS placement and the non-OpenMP implementation: still native, as the existing evidence states.

## Next step

Establish where FG140/FG141 would be reached: either the current thread backend does not use the claim/barrier route (in
which case the clause should be restated for this substrate, like P1-10's ring clause), or it is a genuine dormant port that
needs wiring. That decision is cheap to make by reading the call sites; finishing the port is not.
