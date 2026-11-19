# EXP-2026-11-19-1357: the door's short-buffer contract fixed, and the check that proves it

## The cause

`dt_piece_body` refused any call whose write offset was already past the end of the buffer:

```goose
if (token < 0 || ... || off > out.len) { return 0; }
```

The native loop does the opposite: once the buffer is full it keeps asking each remaining piece for its **size**, so
the total it reports is the whole needed length. Refusing the size made the door report where it stopped - `-18` at a
buffer of 8 and `-69` at 64, against the native's `-1015` for the same 129-token sequence.

## The fix

The guard now allows an offset past the end and clamps the room to zero instead; a piece that cannot be written still
returns its own size, exactly as the native `token_to_piece` does with `avail = 0`.

## The check, before and after

```
before:  len_max=8   goose=-18    native=-1015
         len_max=64  goose=-69    native=-1015
after:   len_max=8   goose=-1015  native=-1015
         len_max=64  goose=-1015  native=-1015
```

The harness from EXP-2026-11-19-1353 extended with five buffer sizes; the last two are at and one byte above the needed
length, where the full string is returned and compared against the native bytes.

## Why this was not visible

`llama_detokenize` and `llama_token_to_piece` both fell back to the native call whenever the door reported a short
buffer, so callers always received the native answer. The disagreement lived only on the resize path - the one no
successful-call comparison touches - and it was found by measuring that path deliberately rather than by reading.

## What it unblocks

The two short-buffer fallbacks can now go: the door reports the same negative the native does. That removal is the next
step, and it is the last piece of native code on the detokeniser path.
