# EXP293: per-instance sequence bounds -- P5-08 fix and closure

EXP292 traced the +780-byte state-file difference to an empty checkpoint, and then to its cause: the incremental
sequence-position bounds from EXP209 lived in one process-wide arena (`static inline int64_t * goose_bounds`) that
`reset()` never cleared, so a freshly reset slot reported `seq_pos_min == 0` instead of `-1` and the server's
"nothing to checkpoint yet" guard passed.

## Fix

The arena is now per cells object. The class layout cannot change -- the variant caches (dsa, iswa, msa, dsv4) are flint
objects compiled against the stock header -- so ownership is resolved through a small fixed table keyed by `this`
(8 slots, fall back to the native set path when exhausted), and `reset()` clears the bounds to `-1`.

Files: `llama-kv-cells.h` (occlusion table, `goose_bounds_ptr()`, `goose_bounds_clear()`), and the three translation
units that include it (`llama-kv-cache.cpp`, `llama-graph.cpp`, `llama-model.cpp`) recompiled against it and substituted
in the relinked `libllama`.

## Verification

| check | result |
|---|---|
| checkpoint appendix | `count=1` (was 2 with the empty entry) |
| state file size, 12-token prompt | 131 975 288 -- exactly native's |
| accepted profile | `e68df05b...`, 128 tokens, clean shutdown |
| native -> goose restore | `n_restored = 12`, continuation `1da0f499...` = exact |
| goose -> native restore | `n_restored = 12`, file parsed, but continuation `0b7d5391...` differs |

## The one remaining difference, stated rather than hidden

The two lines compute attention with different kernels and their KV values differ by one ULP (measured at offset 480 in
EXP292). At temperature 0 that is enough to flip a token when the native line continues from our KV. It is a property of
the two compute lines, not of the serialization: the format, the section offsets, the checkpoint appendix and the file
size now match exactly, both readers parse both files, and each line continues deterministically from its own or the
other line's file where the restored numbers are identical.

Error behaviour is unchanged and previously measured: a truncated restore is rejected (EXP363/364, with the clear route
firing per EXP367/368), and the read-header checks are gated in EXP366.
