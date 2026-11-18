# EXP-2026-11-18-962: the three sampler filters join the canonical unit

Status: composition change accepted. The modules compile inside the runtime
unit; nothing is wired into the shipping chain yet.

## Hypothesis fixed before changes

EXP959-961 wrote `sampler-typical`, `sampler-top-n-sigma` and `sampler-dry`, each
gated on its own. A check of `tools/build-main-runtime.sh` then showed all three
**were not in the canonical unit** - they were exactly the dead code the tokenizer
engine was before EXP949. A module that is gated in an experiment but absent from
the composed runtime is a gate, not a capability.

## What was done

1. All three modules were added to `MODULES` in `tools/build-main-runtime.sh`;
2. composing them with the unit reported a real conflict:
   `extern sqrt32 ... binds a different signature`. `sqrt32` is already bound to
   `qz_sqrtf` by the quantization modules, and `log32`/`pow32` likewise by the
   CUDA modules. The typecheck does not see this, only the compose step does;
3. the three modules' externs were renamed with their own prefixes:
   `tns_sqrt32`, `dry_pow32`, `dry_log32`, `tp_ln64`, `tp_abs64`;
4. all three original gates were re-run after the rename, because a rename that
   breaks a binding would be invisible until a gate ran.

## Result

| check | before | after |
| --- | --- | --- |
| modules in the canonical unit | 136 | **139** |
| `goose check` specializations | 4219 | **4235** |
| compose conflicts | 1 (`sqrt32`) | **0** |
| generated C compiles | - | **exit 0** |
| global definitions | - | **1139** |
| duplicate globals | - | **0** |
| typical gate | 2535 / 0 | **2535 / 0** |
| top-n-sigma gate | 17112 / 0 | **17112 / 0** |
| dry gate | 2440 / 0 | **2440 / 0** |

The duplicate-global count is the check the typechecker cannot do: a composed unit
that maps to one object must define each symbol once.

## Not claimed

The filters are in the unit, not in the shipping sampler chain. Wiring them needs
a generation run to compare output, which needs Igor's approval. So P6-10 stays
PARTIAL at 1/4.

XTC and mirostat v1/v2 remain native: Goose has no RNG, so they cannot be ported
as decisions the way these three were.

No model was started.

## Reproduction

    mapfile -t MODS < <(sed -n '/^MODULES=(/,/^)/p' tools/build-main-runtime.sh \
        | grep -oE 'goose/[a-z0-9/_-]+\.goose')
    python3 tools/compose-goose.py OUT/unit.goose "${MODS[@]}"
    <goose> check OUT/unit.goose
    <goose> --standalone -o OUT/runtime.c --header OUT/runtime.h OUT/unit.goose
    gcc-13 -O1 -fPIC -c OUT/runtime.c -o OUT/unit.o
    nm --defined-only --extern-only OUT/unit.o | awk '{print $3}' | sort | uniq -d
