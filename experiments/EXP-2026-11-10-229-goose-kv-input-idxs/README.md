# EXP229: graph-input index fill in Goose — ACCEPTED on correctness

`llama_kv_cache::set_input_k_idxs`, and the identical non-transposed branch of `set_input_v_idxs`
(llama-kv-cache.cpp:1496 and 1512). They fill a graph input tensor with `offs + idxs[s][i]`, stream-major.

## What the port preserves

    for (s = 0; s < n_stream; ++s) {
        const int64_t offs = strm[s] * get_size();     // stream id times cache size, NOT the position
        for (i = 0; i < size; ++i)
            data[s*size + i] = offs + idxs[s][i];      // stream-major row order
    }

Two orderings matter and both are gated: `offs` uses the **stream id**, not `s`, and the row index is
`s*size + i`, not `i*n_stream + s`.

`idxs` is `vector<vector<uint32_t>>` on the C++ side and each stream's array is its own allocation, so
the bridge passes an array of pointers instead of flattening — flattening would add a copy on a per-batch
path. The Goose side reads them with a **uint32** loader, not int32, so nothing depends on the sign.

## Gate

    25893 checks, 0 failures

6 named cases chosen to catch the two orderings (`two-streams` distinguishes stream-major from
token-major, `reordered-strm` uses `strm = {2, 0}` so using `s` instead of `strm[s]` fails), plus 4000
random slot_infos with 0-3 streams and sizes 1-5. Asserts the returned count, every written value, and
that nothing is written past it.

Mutations, both caught heavily:

| mutation | failures |
|---|---:|
| row index `s*size+i` → `i*n_stream+s` | 10115 |
| `offs = strm[s]*kv` → `offs = s*kv` | 13431 |

## Live

    FG229I_KV_INPUT_IDXS calls=292
    FG228E_KV_CELL_EXT  calls=65814
    prefill 2419.7  decode 24.25
    tokens True  probabilities True  sha e68df05b

292 calls per request — one per ubatch, as the shape implies, and each writes `size` entries. Witness
superset vs `exp228` with `--baseline`: no missing, `new FG229I`. All eight routes present in one object
with `nm -u` before linking:

    fg199k_ fg204x_ fg205g_ fg206d_ fg209i_ fg227n_ fg228e_ fg229i_

## Integration notes

- The helper takes `const llama_kv_cache::slot_info &`. My first attempt used an invented type name and
  the build failed; had it compiled it would have been the wrong signature, so the compiler error was the
  cheaper outcome.
- Guarded with `ns <= 64 && size <= 4096` so the stack arrays cannot overflow; above that the native loop
  runs. Apex shapes are far inside the bound.
- `set_input_v_idxs`'s **transposed** branch (non-flash-attention) computes a different offset and is left
  native.

## Performance

Not measured as a pair. **Прогон выполнен при параллельной нагрузке. Показатели производительности могут
быть искажены.** 2419.7 / 24.25 sits in the same depressed band as every run today (native 23.8-25.8
against 26.0-26.7 in clean sessions). Deferred; not a condition for acceptance, since correctness is
established and the route is behind `GGML_GOOSE_KV_INPUT_IDXS` (default enabled, `0` disables).

292 calls each doing `size` writes is a shape where a per-call boundary is amortised, unlike EXP228's
65,814 calls, so no regression is expected — but that is reasoning, not a measurement.
