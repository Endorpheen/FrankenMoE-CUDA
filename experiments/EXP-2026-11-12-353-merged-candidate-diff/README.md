# EXP353: the merged candidate against native, and whether the merged bridges are live

Two questions: does the merged build change behaviour, and are the eleven merged bridges actually executing rather than present-but-dead.

## Behaviour

The same eight sampler cases used in EXP351, run against the merged candidate instead of the older stage: **eight cases, zero differences**
from native, with the same distinctness pattern as before (six distinct outputs among eight, DRY and top-n-sigma equal to baseline at those
settings). So the merge did not disturb the sampled path on this model.

## Liveness, from the witnesses

One `/completion` request and the first 256-token context produce these witness lines from the merged builds:

    FG274B_BATCH_COMPAT calls=7 yes=7          <- the predicate decided seven times, all "yes"
    FG272S_SLOT_SELECT similar=1 lru=1         <- both slot paths executed
    FG270Q_SERVER_QUEUE pick=0 cleanup=0       <- queue bridge entered
    FG276T_STOP_STRINGS full=0 hits=0          <- stop-string bridge entered, no stop in this request
    FG268P_CTX_CHECKPOINTS near=0 room=0 supersede=0

These are the four server ports, and `FG274B` is the strongest of them: a witness that counts decisions and reports `yes` seven times is a
bridge that ran, not one that linked and sat still.

`fg291b`, `fg262a`, `fg263c` and `fg286m` emitted no witness in this run. That is expected rather than suspicious -- batch rejection needs a
rejected batch, the MoE cache decisions need a MoE model, and the mmap bookkeeping prints on its own path -- but it means this run proves those
four are linked, not that they execute.
