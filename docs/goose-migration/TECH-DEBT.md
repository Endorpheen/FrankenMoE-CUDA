# Technical debt ledger

Working notes for the Goose migration. Each item is a reproducible observation
with a location and a reproduction, so it can be handed to whoever owns the
relevant component without a conversation.

## TD-S1: a function result stored in an f64 local reads as zero

Observed while porting `dist` and `temp_ext`.

| case | result |
|---|---|
| `let x: f64 = 1.5; p[0] = x as f32` | 1.5 (correct) |
| `let y: f64 = lv[0] as f64 * 2.0` | 4 (correct) |
| f64 accumulator over a loop | 9 (correct) |
| `(mt_canonical(id) * 4294967296.0) as i64` in one expression | correct |
| `let u: f64 = mt_canonical(id); p[0] = u as f32` | **0** |
| the same with `* 1.0` or `+ 0.0` in the initialiser | **0** |

Reproduction: dispatch op 22 of `experiments/EXP-2026-11-18-1031-p6-10-typical`
with `sm_mt_seed(state, 1)` first; prints `bare=0 *1.0=0 +0.0=0` where the
native values are 0.997184808, 0.932557361 and 0.128124448.

Impact: `dist` (draws `mt_canonical`) and `temp_ext` (stores an extern `exp`
result for the double renormalisation) cannot be completed bit-exactly in f32,
so both are unported. Nothing else depends on it.

## TD-S2: generated function ids shift whenever the module changes

A caller that hardcodes `gg_function_N` silently calls a different function
after an unrelated edit (this produced a false filter defect during top-p).
Mitigated: `tools/build-sampler-chain.sh` derives the ids into a header, and the
module exposes one stable entry point per stage.

## TD-S3: module constants require an explicit init call

`gg_init_globals()` is called by the generated `main`, not by the module's
functions. A host that embeds the module must call it; otherwise every module
constant reads as 0 (`x & M32` was 0, so the MT stream returned zeros).
Mitigated: the shim runs it from a constructor.

## TD-S4: no export mechanism, and unused functions are dropped

`export fn`, `pub fn`, `@export` and attribute forms are all rejected and there
is no export flag. A module without `main` cannot be compiled, and functions not
reachable from `main` are removed. Mitigated: the build appends a root that
calls the entry points.

## TD-S5: an edit to a module signature did not reach the generated C

Observed while making the top-p entry's signature unique.

```
grep -c marker build/sampler-chain/sampler-chain-root.goose      -> 2
grep -c 171    build/sampler-chain/sampler_chain.c               -> 0
prototype stays: gg_function_2(gg_slice, gg_slice, gg_slice, int64_t, float, int64_t, int64_t base)
```

The source signature gained a trailing `marker: i64` that the body compares
against 171, the root file the compiler consumes contains it, a direct compile
of that same root file reports rc=0 and still produces no `171` and no
seven-argument prototype. A different edit to the same file (a constant value)
did change the output earlier, so this is not a blanket cache.

Impact: an entry point cannot be made uniquely identifiable by signature until
this is understood, which is what the top-p hook needs. Reproduction is the two
commands above. The top-p stage stays behind its own opt-in flag meanwhile.
