# EXP-2026-11-19-1418: composing modules without their providers broke the library

## What happened

Composing `slru-policy-goose.goose` into the sampler-chain unit made `libllama` **unloadable**:

```
llama-server: symbol lookup error: libllama.so.0: undefined symbol: slru_ld_i32
```

The gate named three missing providers at once - `slru_ld_i32`, `slru_st_i32` and `fg637_fmt2` - and they have different
origins:

| symbol | declared by | provider |
|---|---|---|
| `slru_ld_i32`, `slru_st_i32` | `slru-policy-goose.goose` and `goose-moe-cache-slru.goose` | the CPU cache host adapter, a different unit |
| `fg637_fmt2` | `llama-bench-report.goose` | `integration/m1/host/goose_host.c`, linked into the standalone runtime, not into libllama |

This is the `mt_fmt_g6` lesson repeating: **an extern needs a provider in the same artefact**, and a module that compiles
is not a module that loads.

## What was done

1. the SLRU composition was **reverted** - its providers belong to a different unit and inventing them in this shim would
   be a second adapter for the same symbol, which rule 17 forbids;
2. `fg637_fmt2` was **provided** in the unit's own shim, because this shim is where that unit's providers live and the
   function is a `%.2f` conversion with no logic - the same shape as the `mt_fmt_g6` provider already there;
3. the gate entry for `goose-slru-policy` was removed together with the composition.

Afterwards:

```
check-unresolved-symbols: ok, every undefined symbol is provided
check-accepted-components: ok
acceptance: decode 62.73 / 62.87 / 63.32, prefill 2701.4 / 2953.7 / 2953.3, hash c2ff42cc1219
```

## What this costs and what it teaches

The SLRU shadow comparison built in EXP-1417 is **no longer in the shipped library** - it cannot be, because the module
needs a host adapter this build set does not have. The comparison code stays in the source behind `FG_SLRU_DIAG`, inert,
and the composition is undone until its provider is ported or the adapter is placed where it belongs.

Two lessons, both already written down and both repeated:

- a module in the list is not a capability until the artefact loads;
- the gate that caught it was `check-unresolved-symbols`, not the build: the build succeeded twice with an unusable library.

## State of P9-03

Unchanged in substance: the policy is not composed, its comparison has never run, and `plan()` is not on the measured
path anyway (EXP-1417). The honest summary is that this item is blocked on its host adapter, not on the port.
