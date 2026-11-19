# EXP-2026-11-18-1006: 206 of 336 bridge exports are called in Apex

Status: the other half of the execution question. The unit's exports are uncalled, but
the frozen bridges run, and counting only the unit made Apex look like it ran no Goose at
all. 38 335 checks, 0 failures.

## What was measured

EXP1004 reported that none of the unit's 12 exports in the accepted library is called, and
the obvious reading - "Apex runs no Goose" - is wrong. `fg200g_is_recr` is the layer-kind
decision, and it is called:

    $ python3 tools/check-symbol-callers.py <accepted lib> --only fg200g_is_recr
    called: 1 | exported but called by nobody: 0
      CALLED fg200g_is_recr

So the whole bridge surface was counted:

| surface | exports | called |
| --- | --- | --- |
| canonical unit, in the accepted library | 12 | **0** |
| frozen bridges, in the accepted library | 336 | **206** |
| bridge exports nobody calls (fresh build) | 342 | 169 uncalled |

The report now prints both, and the two lines sit together on purpose: one without the
other is a half-truth in either direction.

## Why `is_recr` matters

`llama_hparams::is_recr` is how every layer lookup asks "is this a recurrent layer", and
in the reconstructed runtime it is already Goose:

    const int64_t r = fg200g_is_recr((int64_t) il, (int64_t) n_layer_all,
                                     (uint64_t)(uintptr_t) is_recr_impl.data());
    if (r >= 0) return r != 0;

The native lookup was deleted, not put behind a flag. `goose/model/goose-model-graph.goose`
owns the decision, and the array is handed over unmodified because `is_recr_impl` is
`std::array<uint32_t, N>` and the module reads with a 4-byte stride - the same stride, so
there is no copy and no reinterpretation.

That is the shape rule 17 asks for, and it is already in the accepted artifact.

## A gate correction

`apex-array-plan` derived `n_layer_all` as `(block_count - nextn) + nextn`. The loader
assigns `n_layer_all = block_count` directly (`llama-model.cpp:1396`) and derives
`n_layer() = n_layer_all - n_layer_nextn` (`llama-hparams.cpp:348`). The arithmetic is the
same; the statement is not. If the two keys ever disagreed, the round trip would hide it
and the loader would not. The gate now reads the keys the way the loader does and asserts
`nextn <= n_layer_all`, which is the loader's own precondition. 11 checks.

## Result

| check | result |
| --- | --- |
| whole suite | **38 335**, 0 failures (15 gates + 1 finding) |
| unit exports called in accepted lib | 0 of 12 |
| bridge exports called in accepted lib | **206 of 336** |
| apex-array-plan | 11 checks |

## Not claimed

"Called from inside this library" is what `objdump` sees. A call from another shared
object, an indirect call, or a `dlsym` lookup would not appear, and the reverse is
possible too - a bridge could be called on a path this build never takes at run time.

206 is not 206 decisions executed on Apex: the bridges cover many models and configurations,
and which of them run depends on the GGUF and the flags. The number says the Goose code is
reachable and wired, not that it was exercised on a particular request.

No model was started, no benchmark was run.
