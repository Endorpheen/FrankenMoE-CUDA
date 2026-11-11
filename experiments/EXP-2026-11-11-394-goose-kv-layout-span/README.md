# EXP394: KV tensor-layout arithmetic is Goose-owned (APEX.KV, part 3)

## Transferred

| op | span |
|---|---|
| 0 | contiguous run: offset = first * row, size = count * row |
| 1 | transposed values: offset = (first + j * size) * el, size = count * el |
| 2 | element rows visited by the transposed layout |

Applied on both sides, write and read, for K, V and transposed V.

## Gate

| check | result |
|---|---|
| 875 checks over first 0..8, counts 0..8, rows {1,4,64}, j 0..4, sizes {1,16,128} | 0 failures |
| mutations | **5/5 caught** |

## Run

`FG394S_KV_LAYOUT_SPAN` live; completion `e68df05b`; restore exact; slot file byte size unchanged. A wrong
offset would move payload bytes inside the slot file, so an identical answer and an identical file size
are the right observation.

## Boundary

The bytes themselves still move through llama's io primitive (`write_tensor`/`read_tensor`), which is
declared as the buffer primitive the criterion allows. The addresses and sizes are now Goose's.
