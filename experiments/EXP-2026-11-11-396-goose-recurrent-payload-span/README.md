# EXP396: recurrent tensor-payload layout is Goose-owned (APEX.RECURRENT_MEMORY)

## Transferred

The span arithmetic of `llama_memory_recurrent::state_write_data` / `state_read_data`, on both sides:

| op | span |
|---|---|
| 0 | contiguous run: offset = first * row, size = count * row |
| 1 | transposed values: offset = (first + j * size) * el, size = count * el |
| 2 | element rows visited by the transposed layout |

Applied to R, to the PLE conv row and to S, in write and in read.

## Gate

| check | result |
|---|---|
| 875 checks over first 0..8, counts 0..8, rows {1,4,64}, j 0..4, sizes {1,16,128} | 0 failures |
| mutations | **5/5 caught** |

## Run

`FG396S_RECURRENT_PAYLOAD_SPAN` live next to the accepted `FG232R`; completion `e68df05b`; restore exact.

## Boundary

The bytes still move through llama's io primitive, declared as the buffer primitive. The addresses and
sizes are Goose's. Payload placement is therefore Goose-owned on the used path.
