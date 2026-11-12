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

## Decision on the claim/barrier clause (checked, not assumed)

Searched every call site:

- `fg140_claim` / `fg141_claim` are **defined** inside the Goose modules (`ggml-cpu-apex-control.c:1946`,
  `ggml-cpu-apex-tiled.c:1947`) and referenced by their own test fixtures;
- there is **no call site in the flint tree at all** - neither `ggml-cpu/*.c` nor the backend calls them.

So the claim/barrier half is dormant **by construction of the current substrate**, exactly like the pinned-ring clause in
P1-10. The current thread backend partitions work differently and never asks for a per-expert chunk claim.

Consequence, stated the same way as for P1-10: the clause cannot be completed as written without reintroducing the old
coordination model. The honest options are

1. restate the clause for the current substrate: Goose owns the live pool/executor/partition lifecycle (FG143/147/149, all
   witnessed) and the atomic claim route is not part of it;
2. or deliberately reintroduce a claim-based route and port it - a design decision, not a porting task.

Determinism at 8/16/32 batch threads is already verified on the model (`e68df05b`), so the criterion's deterministic-output
requirement is met by evidence regardless of which option is chosen.

Recorded as a stale clause with proof, not as remaining work.
