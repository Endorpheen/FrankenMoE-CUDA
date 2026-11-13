# EXP389: GGUF inventory decisions are live (APEX.LOAD)

## Verdict

**LIVE on the reconstructed tree.**

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | a tensor name may appear once; a second occurrence invalidates the model | `llama_model_loader::load_arch` tensor walk |
| 1 | the file type is the most frequent tensor type, the first one keeping a tie | same, the `n_type_max` loop |
| 2 | a type's running count after one more tensor | same |

## Gate

| check | result |
|---|---|
| 31 117 checks, including 200 simulated inventories of up to 300 tensors compared against the loader's own loop, and every dominant-type combination | 0 failures |
| mutations | **5/5 caught** |

Mutations: duplicate never fires, duplicate fires always, a tie replaces the first type, dominant ignores
the count, the step does not advance.

## Run

| check | result |
|---|---|
| completion | `e68df05b…`, equal |
| restore | succeeds |
| `FG389S_GGUF_INVENTORY` | **live** (first-call line; the first 5 000 calls are otherwise silent by design) |
| `FG386S` / `FG345W` / `FG241U` | live |

## Note on the 5 000-call print interval

The inventory fires once per tensor and once per type, which is a few thousand calls for this model, so
only the first-call line appears. Liveness is proven by that line; a call count would need a smaller
interval, not a different test.
