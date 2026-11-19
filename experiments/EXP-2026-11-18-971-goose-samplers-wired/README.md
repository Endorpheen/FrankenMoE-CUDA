# EXP-2026-11-18-971: top-n-sigma and DRY are wired too

Status: all three ported sampler filters now decide inside the shipping chain,
each differentially equal to the accepted library with a positive witness.

## Hypothesis fixed before changes

EXP970 wired `typical` only. `tns_apply` and `dry_apply` were in the DSO and gated
but never called. If the same delegation is applied to
`llama_sampler_top_n_sigma_apply` and `llama_sampler_dry_apply`, the shipping
samplers must keep exactly what the accepted library keeps - and the witness must
show which path decided.

## What was done

- `llama_sampler_top_n_sigma_apply` copies the strided logits into a flat slice,
  calls `tns_apply`, and maps the module's most-negative-finite sentinel back to
  `-INFINITY`. The sentinel is a representation detail; keeping native's makes the
  two paths byte-equal instead of "equal apart from a sentinel".
- `llama_sampler_dry_apply` flattens the ring buffer with `to_vector()` - which is
  chronological, the order the engine expects after the EXP968 fix - packs the
  breaker multimap into parallel arrays, and calls `dry_apply`.
- Both are guarded by `goose_sampler_enabled()` and both fall through to the native
  body on refusal, so the switch cannot make behaviour worse.

The DRY testing initialiser is a C++ symbol, so it is mangled
(`_Z30llama_sampler_init_dry_testingffii...`); the gate resolves it by that name.

## Result

Both arms run 100 cases of each sampler against the accepted library.

| check | flag on | flag off |
| --- | --- | --- |
| gate | **5070 checks, 0 failures** | **5070 checks, 0 failures** |
| witness, Goose hook calls | **250** | **0** |
| symbols removed from the DSO | **0** | - |
| symbols added | 94 | - |

250 = 100 typical + 100 top-n-sigma + 50 DRY (the DRY arm skips the cases whose
window is shorter than `dry_allowed_length`, where native returns before the hook).

`typical` compares survivor sets and probability bits, `top-n-sigma` and DRY compare
every logit bit, all against the accepted `libllama.so.0` loaded beside the
candidate in one process.

## Not claimed

Behavioural acceptance of a generation run is still outstanding: what this proves
is that wiring the filters cannot change what the sampler keeps, not what the model
outputs end to end. That needs Igor's approval, and P6-10 stays PARTIAL.

XTC and mirostat v1/v2 remain native: Goose has no RNG, so they cannot be ported as
decisions the way these three were. That is the honest remaining gap in P6-10, and
it is a language capability, not a port to write.

No model was started.

## Reproduction

    python3 runtime/reconstructed/build.py --build-dir OUT --runtime-libs <accepted>
    g++-13 -O1 experiments/EXP-2026-11-18-971-goose-samplers-wired/tests/gate.cpp -ldl -o gate
    LD_LIBRARY_PATH=OUT/build:<accepted> gate OUT/build/libllama.so.0 <accepted>/libllama.so.0
    GGML_GOOSE_SAMPLER=0 ... gate ...    # witness must be 0
