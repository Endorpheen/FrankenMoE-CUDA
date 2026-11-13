# EXP367: KV state clearing live on the failed-restore path

The clear that repairs a half-restored cache, exercised by feeding the server a truncated state file.

## Verdict

**LIVE, EXACT** — the Goose plan zeroes the tensors, and the completion before the failure is
unchanged.

## What runs

The module decides which tensors to zero and at which offsets: contiguous cells get one call per
tensor, scattered cells one call per cell; keys for every layer; values only where a value tensor
exists, and a transposed value cache per embedding row. The bridge returns `(kind, layer, offset,
size)` quads and llama performs the `llama_clear_tensor_data` calls.

| check | result |
|---|---|
| 4 000 draws over contiguous and scattered layouts, both V layouts | 51 077 checks / 0 failures |

## Run

1. completion on the fixed profile → hash `e68df05b…` (unchanged);
2. save slot 0 → 387 989 920-byte file;
3. restore a **truncated copy** (1 MiB) → the server rejects it (`HTTPError`);
4. the rejection runs `state_clear` on the touched cells.

| check | result |
|---|---|
| `FG367S_KV_STATE_CLEAR` | **live** — first call |
| batch witnesses | both live |
| completion unchanged | yes |

## Why the failure path

`state_clear` is only reached when a restore fails — the successful path never calls it. That makes
this the honest way to witness it: a corrupted file is exactly the input the branch exists for. The
server's own answer (an error response) is unchanged by the port.

## Remaining

`state_clear(seq_id)` with `-1` (whole cache) and the hybrid drop path are not exercised by this
run; they need a cancellation or a model unload, which is a separate scenario.
