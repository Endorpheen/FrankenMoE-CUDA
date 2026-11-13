# EXP408: graph input planning is Goose-owned (APEX.MODEL_GRAPH)

## Transferred

| op | rule |
|---|---|
| 0 | a mixed batch is available only when the architecture supports it and the context is the default type |
| 1 | the token-row count is the mixed-batch requirement, and zero otherwise |
| 2 | the input width is padded up to the model width when they differ |

## Gate

| check | result |
|---|---|
| 23 checks over arch × context type, mixed × rows, both widths | 0 failures |
| mutations | **5/5 caught** |

## Run

`FG408S_GRAPH_INPUT_PLAN` live during graph construction, next to the accepted `FG158` (builders) and
`FG200G` (hparams); completion `e68df05b`; restore exact.

## What remains for the item, stated plainly

Node creation itself is not ported and cannot be with this bridge: `build_moe_ffn` alone is 504 lines with
104 `ggml_*` calls that create and connect tensors. What is Goose-owned on this path today is the decision
layer around it — hparams and geometry (FG200G), the builders' choices (FG158), the fused-op resolution
(FG391S) and now the input planning (here). A tensor API on the Goose side is the dependency that would move
the creation itself, and that is an architecture decision, not a transcription.
