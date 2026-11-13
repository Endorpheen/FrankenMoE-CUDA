# EXP262: MoE device-cache admission and grouping in Goose

Part of P1-10. The cache's SLRU policy is already Goose and gated (EXP-150). What the constructor still decided
natively is which layers enter the cache at all and how they share banks: this experiment moves that decision.

## What the native constructor decides

`llama-moe-cache.cpp`, `impl::impl`:

- a layer is admitted only when it has experts, all of them are host weights, and its device is one of the cache's
  GPU devices;
- admitted layers share one group (one bank set, one SLRU) when their device and their expert tensor layout are the
  same;
- host bytes are summed per group and per device;
- if no layer qualifies, the cache disables itself.

Tensor construction, buffer allocation, the upload copies and teardown remain outside this module.

## Module

`source/admission.goose` takes flat arrays (`n_experts`, `device`, `layout`, `nbytes`) and writes `group[layer]`
(-1 = not admitted), `group_dev`, `group_layout`, `group_bytes`, `group_count` and `n_groups`. No structs cross the
boundary; the emitter produces a fixed-name wrapper, as in EXP-258/259.

## Gate

`tests/admission_fixture.c` runs a direct transcription of the native loop and the Goose module on the same inputs and
compares every output plus the verdict:

    all skipped              OK
    three skip reasons       OK
    one group of three       OK
    layout split by device   OK
    layouts on one device    OK
    mixed admitted/skipped   OK
    capacity exceeded        OK
    no layers                OK
    checks: 8, failures: 0

The capacity case matters: the reference refuses *before* writing the second group's header, and the port must match
that, not merely return false.

## The gate was shown to fail

Changing the device comparison in the emitted code from `==` to `>=` makes the mixed case disagree on the group of
layer 3 and on both group headers (failures: 1). Restored, the fixture returns 8/0 again.

## Where this is live

Not on the accepted profile: the accepted configuration runs with `cache_n = 0`, so the constructor is never reached.
It is live in the 3072 MiB device-cache configuration, which is the optimisation candidate, and that is where an
integration run would have to prove it. Recorded as ported and gated, dormant on the accepted profile -- the same
category as EXP259, and not counted as an Apex route.

## What remains in P1-10's lifetime layer

Tensor construction and the bank views, the upload-copy sequence, and the teardown order (host slot maps, device
banks, views). Each needs the same treatment: a decision module, a failure case for the refusal path, and an
integration point that can be exercised by the cache configuration.
