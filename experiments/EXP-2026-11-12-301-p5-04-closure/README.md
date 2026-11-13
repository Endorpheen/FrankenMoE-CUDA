# EXP301 (record): P5-04 closure -- recurrent memory plan complete, live since the accepted profile

Criterion: "State create/update/copy/remove/rollback and stream ordering match multi-step recurrent models; actual Apex
recurrent witnesses."

## Plan level: complete, item by item

| area | experiment | gate |
|---|---|---|
| find_slot whole (head reset, shared-sequence sweep, ring search, tail copy, re-order pass, final write, staging, recount) | EXP293 | 131 200 checks, re-order 1 233 times, 8/8 |
| seq_cp / seq_keep / seq_add / seq_div | EXP294 | 1 170 000 checks, 8/8 |
| seq_rm / set_rs_idx / clear / state_clear incl. partial rollback | EXP295 | 951 126 checks, 8/8 |
| state_write_meta and whole-cache state_read_meta | EXP296 | 44 480 checks, 8/8, streams compared byte for byte |
| payload plan of state_write_data / state_read_data (records, order, every header/type/row check) | EXP297 | 16 692 checks, 120 rejected streams, 8/8 |
| init_batch, prepare, init_update | EXP337 | 30 022 checks, 8/8 |
| state_read_meta / state_read_data refinements | EXP338 | 43 689 checks, 9/9 |
| state_clear and context next/apply/get_status | EXP339 | 31 380 checks, 8/8 |
| seq_pos_min/max, set_rs_idx, breakdown, can_shift, is_empty, r/s/p sums | EXP340 | 53 886 checks, 9/9 |
| state_write two range lists and state_read transaction | EXP346 | 44 871 checks, 9/9 |

Two defects were found by the port itself rather than by luck: EXP293 caught a scratch-register choice that clobbered cells
outside the range (native leaves them at -1), and EXP295 found that native compares `seq_id` as unsigned against
`n_seq_max`, so the `seq_id < 0` branch is dead and negative ids are rejected earlier.

## Live Apex witnesses, from the accepted-profile run already on record

No new model run was needed, so nothing here depends on a fresh approval:

    FG232R_RECR_STATE_RANGES calls=6
    FG382S_RECURRENT_IO_PLAN first call calls=1
    FG396S_RECURRENT_PAYLOAD_SPAN first call calls=1
    FG200G_MODEL_GRAPH is_recr=800 recurrent=600 native=0
    slot create_check: RAM recurrent prefix saved: tokens = 28672, size = 62.813 MiB, retained = 1
    slot create_check: RAM recurrent prefix saved: tokens = 32764, size = 62.813 MiB, retained = 2

So the model builds 800 recurrent layers' worth of graph with 600 Goose-side recurrent decisions and zero native
fallback for them, the state-range planner runs six times, and the recurrent prefix checkpoints are actually written and
retained during the run.

## Stream ordering

Ordering is covered twice: EXP296 compares each stream byte for byte in both directions and EXP307/EXP338 add the
header-validation order and the stream-range resolution with distinct rejection codes, while the live run exercises the
write path through those checkpoint saves.

## Boundary

The tensor payload bytes stay native by design -- the port owns the plan (records, order, checks), not the memcpy. That is
the same split the item prescribes, and it is why the payload work appears as a plan gate rather than a byte-identical
comparison.
