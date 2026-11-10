# EXP227: `llama_kv_cache::get_n_kv` in Goose — ACCEPTED on correctness, performance deferred

`get_n_kv` (llama-kv-cache.cpp:1285) computes the padded KV length handed to the graph builder, once per
ubatch. It is pure arithmetic over the per-stream cell objects, which makes it a clean port: no GGML type
crosses the boundary, the bridge hands Goose two flat arrays.

## What the port had to preserve

    n_pad_cur = max(n_pad, 256)                       // a 256 floor, not n_pad
    for each stream:
        result = max(min(cells.size(),
                         max(n_pad_cur, GGML_PAD(used_max_p1, n_pad_cur))),
                     result)                          // max over streams, each clamped to its cells

Two traps, both covered by the gate:

- the floor is **256**, not `n_pad` — the graph must keep the same n_kv across batches to be reusable;
- the result is the **max over streams** and each stream's contribution is **clamped to its own cell
  count**, so an all-free stream contributes 0 and cannot raise the result.

## Gate

    5011 checks, 0 failures

5000 random shapes (0-4 streams, sizes and `used_max_p1` up to 70,000, `n_pad` sometimes in 0-8191 and
sometimes in 0-299) plus 11 fixed edge cases: empty stream list, all-free cells, `used` far beyond
capacity, `n_pad` of 0 / 1 / 256 / 4096, multiple streams where one dominates.

Compared against a transcription of the native body, not hand-picked expectations.

Two mutations, both caught:

| mutation | failures |
|---|---:|
| 256 floor lowered to 128 | 919 |
| max over streams replaced by last-writer | 1900 |

## Live correctness — exact

    FG227N_KV_N_KV calls=146
    prefill 2512.6  decode 25.90
    tokens True  probabilities True  sha e68df05b

146 calls per request. 128 token ids, text and all 128 saved probabilities equal to the reference.

Witness superset against `exp220` with `--baseline`: no missing, `new FG227N`, verdict OK — so the six
existing KV routes (199K, 204X, 205G, 206D, 209I) are all still present in the same object. Verified with
`nm -u` before linking, per the rule that cost two stages earlier:

    fg199k_ fg204x_ fg205g_ fg206d_ fg209i_ fg227n_

The integration was built on **EXP209's cumulative** `llama-kv-cache.cpp`, not on the pristine source.

## Performance — deferred, not claimed

    module off: 2450.8 / 24.54
    module on : 2512.6 / 25.90

One run each, in different rounds. That is not a pair and the difference (+2.5% prefill, +5.5% decode) is
far larger than this component can plausibly account for at 146 calls per request.

**Прогон выполнен при параллельной нагрузке. Показатели производительности могут быть искажены.**

The module's cost is negligible by construction (146 calls, a handful of arithmetic ops each), so no
regression is expected and none is claimed either way. A proper paired measurement is deferred; it is not
a condition for accepting this change, as correctness is established and the change is behind a switch
that defaults to enabled and can be disabled with `GGML_GOOSE_KV_N_KV=0`.

## Status

Accepted on correctness. `runtime/exp227/run` is the candidate; `exp220` remains the promoted checkpoint
until Codex reviews the experimental branch.
