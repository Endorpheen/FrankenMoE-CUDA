# EXP301: DSV4 KV cache policy in Goose

The policy half of `llama-kv-cache-dsv4.cpp` — the compressed-state cache that
sits beside the ordinary KV cache (P5-03).

## Verdict

**GATED, NOT INTEGRATED** — the helpers are exact; the source tree is read-only.
No performance claim. The compressor's planning and state I/O
(`dsv4_build_comp_plan` and friends, 300+ lines) stay native; this covers the
decision helpers they are built from.

## Scope transferred

| helper | rule |
|---|---|
| `dsv4_comp_size` | `max(1, ceil(kv_size / ratio))` |
| `dsv4_state_n_used_k_rows` | 0 for an empty sequence, else `min(kv_size, (pos_max+1)/ratio)` with truncating division |
| `dsv4_stream_offset` | 0 for one stream, `seq_id*size` otherwise, rejection outside the stream range |
| `dsv4_ubatch_has_coupled` | any token with more than one sequence id |
| `dsv4_token_has_seq` | membership over the token's own id list |
| `dsv4_comp_graph_n_stream` | 1 when there is nothing to fan out or a set is coupled, else `n_seqs_unq` |
| `dsv4_make_k_only` | both MLA head widths become the key head width |
| state planes | `n_stream * (1 + n_rs_seq)` |

The native stream-offset function **throws** on an out-of-range sequence. An FFI
entry cannot throw, so the contract here is `-1`, stated in the code and pinned by
the gate rather than silently different.

## Gate

| check | result |
|---|---|
| `comp_size`, 2 000 parameter pairs | 2 000 checks |
| `n_used_k_rows`, 2 000 triples including empty and clamped cases | 2 000 checks |
| `stream_offset`, 1 000 cases including both rejection directions | 1 000 checks |
| coupled detection and token membership | 2 000 checks |
| graph stream count | 1 000 checks |
| K-only tweak and plane count | 1 000 checks |
| total | 9 000 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| `comp_size` rounds down | 1 899 |
| `comp_size` may be zero | 1 |
| stream offset accepts the first id past the range | 75 |
| coupled means any sequence id at all | 98 |
| token membership skips the last id | 139 |
| single-unique-sequence fan-out is allowed | 65 |
| K-only tweak leaves the value width alone | 500 |
| used rows ignore the cache size clamp | 24 |

All eight are caught. The one-value case is the floor at zero: it only differs
when `ceil(kv/ratio)` is 0, which needs `kv == 0` — one draw in two thousand, and
that is exactly why the floor is probed with a zero-size cache.

A toolchain note, not a code note: a comparison written as `x>(1 as i64)` fails to
parse — the `<`/`>` is taken as a generic bracket and the parser then expects a
closing `>`. Writing `x > 1 as i64` avoids it. This cost a bisection through the
module and is worth knowing before the next one.

## Files

- `source/host.goose` — the helpers plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
