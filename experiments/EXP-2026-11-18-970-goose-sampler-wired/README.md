# EXP-2026-11-18-970: the typical filter is wired into the shipping sampler

Status: wired, differentially equal to the accepted library, with a positive
witness. This is the first sampler filter that decides inside the shipping chain.

## Hypothesis fixed before changes

EXP969 put the three filters in the DSO but nothing called them. If the native
`llama_sampler_typical_apply` delegates its selection to `tp_apply` after the
softmax it already computes, the shipping sampler must keep exactly the tokens the
accepted library keeps - and the gate must be able to tell that Goose ran, not the
native body, because identical output is the point of the change.

## What was done

- `gates.c` gains `goose_sampler_enabled()` (`GGML_GOOSE_SAMPLER`, on unless `0`)
  and a call counter exported as `goose_sampler_calls()`.
- `llama_sampler_typical_apply` calls `tp_apply` after
  `llama_sampler_softmax_impl`. The softmax stays native because the chain needs
  the probabilities anyway; the filter works on them, not on logits. A refusal
  falls through to the native body.
- The gate loads **both** libraries with `dlopen`, constructs
  `llama_sampler_init_typical` from each, applies it to the same candidate array
  and compares the surviving sets and the written probabilities.

## Result

| check | flag on | flag off |
| --- | --- | --- |
| gate | **1307 checks, 0 failures** | **1307 checks, 0 failures** |
| witness, Goose hook calls | **150** | **0** |
| vs accepted library | same survivors, same bits | same |

So the arms are interchangeable in output and distinguishable in who decides.

## The ABI mistake, met for real

The first build linked and then segfaulted when the hook ran. Cause: I declared
`tp_apply` with `uint64_t` pointer arguments, but the generated header passes
slices **by value** as `{data, len}` pairs:

    int64_t tp_apply(sl_f64 probs, int64_t n, double p, int64_t min_keep,
                     sl_i64 order, sl_i64 keep, sl_f64 scratch);

The link succeeded because the symbol name matches; the call then read the pointer
as a length. This is exactly the failure the runtime map warns about
(`RUNTIME-MAP-20261117.md`: "the same symbol name can link despite an incompatible
old ABI", found in EXP905). It was caught only because the guarded arm ran - the
flag-off arm passed 1305 checks while the flag-on arm crashed, which is what made
the direction of the fault obvious.

The declaration now mirrors the header, with the struct definitions beside it and
a comment saying why.

## Not claimed

Only `typical` is wired. `tns_apply` and `dry_apply` are in the DSO and gated but
still not called by the chain, and XTC / mirostat v1/v2 remain native because Goose
has no RNG. So P6-10 stays PARTIAL.

Behavioural acceptance still needs a generation run under Igor's approval; what
this proves is that the wiring cannot change the tokens the sampler keeps.

No model was started.

## Reproduction

    python3 runtime/reconstructed/build.py --build-dir OUT --runtime-libs <accepted>
    g++-13 -O1 experiments/EXP-2026-11-18-970-goose-sampler-wired/tests/gate.cpp -ldl -o gate
    LD_LIBRARY_PATH=OUT/build:<accepted> gate OUT/build/libllama.so.0 <accepted>/libllama.so.0
    GGML_GOOSE_SAMPLER=0 LD_LIBRARY_PATH=... gate ...    # witness must be 0
