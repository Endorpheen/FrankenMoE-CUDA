# cumulative-baseline

**Requirement:** rule 31 - the cumulative baseline must carry every accepted Goose component.
**Produced by:** `tools/check-cumulative-baseline.py`, the other half of the release pre-flight.
**Tier:** fast - seconds, no model, no GPU.

## What it proves

Before any acceptance measurement: the P1-12 Goose Q4_K kernel dispatch and its reference,
the P4-13 allocator primitive and the `libmoe_mmid_tiled.so` kernel object are present.

Known packaging gap, recorded rather than hidden: `builds/flint-acc/bin` and every
`builds/checkpoint-*` carry the three libraries but not the kernel object, which until now lived
only in the experiment that built it. The test therefore searches, in order: the directory given
as an argument, `APEX_SERVER_DIR`, the newest checkpoint or release set that contains the object,
and finally `builds/flint-acc/bin` (which fails this check for that reason).

## Run

```
bash tests/regression/cumulative-baseline/run.sh                  # newest set that has the object
bash tests/regression/cumulative-baseline/run.sh <dir>            # a specific set
```

Expected: `CUMULATIVE-BASELINE PASS`, exit 0. Exit 77 when the directory is absent.
