# EXP-2026-11-18-1201: where the ngram-map key is looked up

`common_ngram_map_send` does not simply take the last occurrence of the key. It scans **downward**
inside the lower range first (`size_last_begin - n - m - 1` down to `key_map_last_idx + 1`), and only
then inside the upper range (`cur_len - n - m - 1` down to `size_last_begin + 1`). So the largest
index wins *within* a range, but a match in the lower range beats any match in the upper one.

```
match pos: trials=2000 fails=0
```

The gate compares against the native scan transcribed line by line, over 2000 random inputs with
varied `n`, `m`, `size_last_begin` and `key_map_last_idx`.

## My defect on the way

The first version of the port dropped `m`: it used `size_last_begin - n - 1` where the native uses
`size_last_begin - n - m - 1`. Both offsets are off by the value length, so the port would look at
the wrong token window whenever `m != 1`. It compiled, it ran, and only the transcription in the
gate exposed it - which is why the gate is a transcription and not a re-derivation.

## What this gate does not cover

The native code first tries a hash shortcut (`key_map`), and that path can return a *different but
equally valid* matching position. This port models the scan that runs when the shortcut misses, and
the gate says so; a claim about equality with the hash path would need a different test, and the
position only matters through the statistics it updates.
