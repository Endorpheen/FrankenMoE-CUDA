# EXP404: weight placement is Goose-owned (APEX.LOAD)

## Transferred

| op | rule |
|---|---|
| 0 | with no operation (`GGML_OP_NONE`) the first buffer is taken without probing |
| 1 | otherwise candidates are probed in order and the first that supports the operation is accepted |
| 2 | an exhausted list yields no buffer |

## Gate

| check | result |
|---|---|
| 31 checks over fast path, probe results × indices, exhaustion | 0 failures |
| mutations | **5/5 caught** |

## Run

`FG404S_WEIGHT_PLACEMENT` live (15 report lines) during model load; completion `e68df05b`; restore exact.

## Primitive boundary, stated for APEX.LOAD

`gguf_init_from_file` (the GGUF parser), `mmap`/file I/O and the ggml tensor metadata are the loader's
system primitives: they read bytes and build descriptors. What Goose owns is every decision on that path
today — shape acceptance (`FG201L`), duplicate names and the dominant file type (`FG389S`), and placement
across buffers (here).
