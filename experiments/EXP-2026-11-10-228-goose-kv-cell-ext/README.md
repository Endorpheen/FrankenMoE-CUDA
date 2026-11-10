# EXP228: `llama_kv_cell_ext` decision in Goose — ACCEPTED on correctness

The ext-construction block inside `llama_kv_cache::apply_ubatch` (llama-kv-cache.cpp:1155-1179). It runs
once per token per ubatch and decides the cell's spatial position and token id, so it is on the live path.

## What the port had to preserve

    if (is_pos_2d || token || ple_n_heads > 0) {
        llama_kv_cell_ext ext;                      // defaults x=0, y=0, tok = LLAMA_TOKEN_NULL (-1)
        if (is_pos_2d) { ext.x = pos[i + n_tokens*2]; ext.y = pos[i + n_tokens]; }
        is_embd = !token || (is_mixed && type[i]);
        if (!is_embd)                     ext.tok = token[i];
        else if (ple_n_heads > 0)         ext.tok = ple_image_token_id != 0 ? ple_image_token_id
                                                                          : ple_eos_token_id;
        cells.ext_set(idx, ext);
    }

Three traps, each covered by the gate:

- with token ids but no 2D positions and no PLE, **x and y stay empty** rather than being copied;
- `is_embd` is true when there is no token array **and** for a single row of a mixed batch whose type
  flags it as embedding;
- `tok` defaults to **-1**, not 0, so an embedding row without PLE leaves it -1.

## Gate

    1024 checks, 0 failures

**Exhaustive over the decision shape**: all 2^6 combinations of `has_ext`, `pos_2d`, `has_token`,
`is_mixed`, `type_i` and the PLE presence, each with 4 value sets for the positions, token id, image id
and PLE head count. Compares (x, y, tok) against a transcription of the native block, and asserts that
nothing is written at all when `has_ext` is 0 and that out[3] is never touched.

Three mutations, all caught:

| mutation | failures |
|---|---:|
| `is_mixed && type_i` dropped from the embedding test | 16 |
| `tok` default changed from -1 to 0 | 40 |
| x/y copied even when `!pos_2d` | 96 |

Note on the third: my first attempt at that mutation used `sed` with `\n` in the pattern, which silently
did not match, and the gate "passed". Only redoing it with a multi-line replace showed the 96 failures.
A mutation that does not apply looks exactly like a gate that does not work.

## Live — and it is a hot path

    FG228E_KV_CELL_EXT calls=65814
    FG227N_KV_N_KV     calls=146
    prefill 2467.0  decode 24.55
    tokens True  probabilities True  sha e68df05b

65,814 calls per request — 451× the `get_n_kv` port next to it. 128 token ids, text and all 128 saved
probabilities exact.

Witness superset vs `exp227` with `--baseline`: no missing, `new FG228E`, verdict OK. All seven routes
verified in one object with `nm -u` **before** linking:

    fg199k_ fg204x_ fg205g_ fg206d_ fg209i_ fg227n_ fg228e_

Built on EXP227's cumulative integration, not the pristine source.

## Integration note

My first attempt used `goto` to skip the native branch and gcc rejected it — jumping over the
initialization of `const bool is_embd`. Restructured to `if (goose) { ... } else { native }` with a single
`cells.ext_set(idx, ext)` after. Worth recording because the goto version never compiled, so it never
reached a test.

## Performance

Not measured as a pair. **Прогон выполнен при параллельной нагрузке. Показатели производительности могут
быть искажены.** The single run gives 2467.0 / 24.55, in the same depressed band as every other run today
(native reads 23.8-25.8 against 26.0-26.7 in clean sessions), so no comparison is possible from it.
Deferred, and not a condition for acceptance: correctness is established and the route is behind
`GGML_GOOSE_KV_CELL_EXT`, default enabled, `0` disables.

At 65,814 calls a per-call cost would matter, so this one is worth re-measuring when the host is quiet —
unlike the neighbouring 146-call port where no regression was ever plausible.
