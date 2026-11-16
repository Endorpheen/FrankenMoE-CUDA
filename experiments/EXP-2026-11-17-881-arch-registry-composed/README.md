# EXP-2026-11-17-881 — the MoE architecture registry is now composed into the working runtime

P9-01 S2 (`goose/model/arch-registry.goose`) was written, typechecked and gated in its own experiment, and was
**not part of the shipped DSO at all**. `nm` on `libllama.so` returned none of `ar_find/ar_count/ar_match`. So
the module could not answer anything from the runtime a user runs, however good its own gate was.

This composes it into the working path, which is what Igor's direction asks: check ready ports, reuse them,
bring the one working runtime forward - not build a parallel one.

## What was needed

The module has **no FFI of its own** - every signature is `sl_u8` or `sl_i64` - so there was nothing to
translate. The bridge adds only what the runtime needs:

| piece | why |
|---|---|
| `fg_n01s_enabled` | default-on switch, `GGML_GOOSE_ARCH_REGISTRY=0` restores the previous behaviour |
| lazy init | the generated C defines its own `goose_init`; the SLRU policy module already defines one in this DSO, so it is compiled with `-Dgoose_init=fg_arch_registry_goose_init` and started on first use instead of at load |
| three wrappers | `fg_n01s_find/count/match` over `const char*` + length, so C++ hosts do not need the slice structs |

## Verified

The module now answers from the shipped library:

```
fg_n01s_enabled()          = 1
ar_count()                 = 9          (the nine recipes the module documents)
ar_find("qwen35moe")       = 2
ar_find("deepseek4")       = 6
ar_find("nope")            = -1        (refused, as the module specifies)
```

Apex with the pinned argv: reference hash `c2ff42cc1219`, decode 38.75 (38.65-39.23) - inside this session's
observed spread, no regression from composing the module.

## What this does and does not do

**Does**: moves P9-01 S2 from "gated in an experiment" to "reachable from the runtime the user runs". That is
the binding Igor asked for, and it was done by composing a ready module rather than re-porting anything.

**Does not**: call it from a host yet. `arch-registry` decides MoE streaming recipes, which live in
`runtime/core`'s streaming path, and that path is not driven by the shipped server. The module is now present
and correct in the DSO; the call site belongs to the streaming wiring, which is the next piece of the same
binding.

## Cost

One build, one Apex run of three repeats.
