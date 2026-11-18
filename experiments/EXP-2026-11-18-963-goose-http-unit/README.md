# EXP-2026-11-18-963: the HTTP response module joins the canonical unit

Status: composition change accepted. The module compiles inside the runtime unit;
the wiring into a listener is still blocked.

## Hypothesis fixed before changes

EXP962 found three sampler modules missing from the canonical unit. The obvious
follow-up was to check the rest of what this series produced, and one more was
missing: `goose/server/http-response.goose` from EXP958. The same failure mode -
gated in an experiment, absent from the composed runtime - so the same check had
to be applied to the whole set rather than to the one module that prompted it.

## What was done

1. Every module this series created was checked against `MODULES` in
   `tools/build-main-runtime.sh`. Result: `http-response.goose` was the only one
   still missing, so it was added;
2. the composed unit was built end to end: compose, typecheck, generate C,
   compile to an object, and count globals and duplicates;
3. the EXP958 gate was re-run, this time against a composition that includes the
   module in a unit with the rest of the repository's modules around it, not only
   in isolation.

## Result

| check | before | after |
| --- | --- | --- |
| modules in the canonical unit | 139 | **140** |
| `goose check` specializations | 4235 | **4246** |
| compose conflicts | 0 | **0** |
| generated C compiles | - | **exit 0** |
| global definitions | 1139 | **1145** |
| duplicate globals | 0 | **0** |
| `rs_*` exports present in the object | - | **18** |
| EXP958 gate | 140 / 0 | **140 / 0** |

The gate is run on the isolated composition (`http-response.goose` alone), because
the full unit needs host primitives at link time - `fg158_fail` and friends - which
the real build supplies. The full unit is therefore verified by compile and symbol
count, and the module's behaviour by its own gate. Both are recorded, neither is
substituted for the other.

## Not claimed

The listener is still absent: the socket half remains blocked by compiler blocker
6, so `runtime/` has no HTTP transport and P11-01's "no HTTP transport" line
stands. Being in the unit means the formatter travels with the runtime; it does
not mean a request can arrive.

No model and no server were started.

## Reproduction

    mapfile -t MODS < <(sed -n '/^MODULES=(/,/^)/p' tools/build-main-runtime.sh \
        | grep -oE 'goose/[a-z0-9/_-]+\.goose')
    python3 tools/compose-goose.py OUT/unit.goose "${MODS[@]}"
    <goose> check OUT/unit.goose
    <goose> --standalone -o OUT/runtime.c --header OUT/runtime.h OUT/unit.goose
    gcc-13 -O1 -fPIC -c OUT/runtime.c -o OUT/unit.o
    nm --defined-only --extern-only OUT/unit.o | grep -cE ' T rs_'
    # module behaviour, isolated:
    python3 tools/compose-goose.py OUT/iso/unit.goose goose/server/http-response.goose
    ... build OUT/iso/lib.so, link tests/gate.cpp from EXP958, run it
