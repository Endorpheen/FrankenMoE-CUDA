# EXP292: session-state interop between the native and Goose lines

P5-08 asks for "Native <-> Goose session/state read/write where format permits; version/endian/truncation/error corpus
and deterministic continuation." This is the read/write interop probe.

## Method

Both servers run the same model with a minimal, identical argv (`-c 2048 -ngl 0 -np 1 --slot-save-path`), the same
request (`temperature 0`, `seed 123`, 8 tokens), and answer with the same content (`1da0f499...`).

1. **Write**: each line saves slot 0 to `state.bin`.
2. **Cross-restore**: the file is copied into the other line's slot directory, restored, and the same completion is run.

## Results

| direction | restore | continuation content |
|---|---|---|
| native -> goose | `n_restored = 12` | `1da0f499...` -- **identical** to the original native answer |
| goose -> native | `n_restored = 12` | `0b7d5391...` -- **different** from the original answer |

File sizes: native `131 975 288` bytes, goose `131 976 068` (+780). The 64-byte headers are identical; the first
differences appear at byte 605 and onwards, inside the per-cell metadata, not in the header.

## Reading

- **Our reader is compatible**: a native-written state restores and continues deterministically.
- **Our writer is not**: the native reader accepts the file (it reports the same 12 restored tokens) but the KV it
  reconstructs does not reproduce the answer. The +780 bytes and the per-cell differences suggest our writer emits a
  different cell set or order than native, which the native reader then maps differently.

Byte-identical files were never the criterion: two implementations may place the same sequence in different cells. The
criterion is deterministic continuation, and it fails in the write direction.

## Status

P5-08 stays IN_PROGRESS with this defect located: compare `state_write` cell selection and ordering against native for a
small prompt, find where the +780 bytes come from, and make a goose-written file restore identically on the native
reader. Truncation and error paths are already covered (EXP363/364 rejected a truncated restore; EXP366 gates the header
checks), so this write-direction mismatch is the remaining gap.

## Narrowing the +780 bytes (second probe)

Two more measurements, both cheap:

1. **All 61 `GGML_GOOSE_*` switches off** -- the server does not even start ("not ready"), matching the earlier lesson
   that a half-disabled staging is visible immediately. The blanket bisect is therefore not available; the KV-only
   switches (`KV_STATE_WRITE`, `KV_META_PLAN`, `KV_LAYOUT_SPAN`, `KV_STATE_CLEAR`) were tried earlier and left the file
   byte-identical, so the difference is not one of those four.
2. **Delta versus prompt length**: a 2-token prompt gives native `65 946 744` / goose `65 947 536` (+792), the 12-token
   prompt gives +780. The delta is essentially constant and does not scale with the number of cells.

So the +780/792 is a **fixed-size section** that our writer emits and the native file does not (or emits differently), not
a per-cell ordering effect. That rules out the cell-selection hypothesis and points at the data-section header: layout
flag, layer count, rotation sizes and the per-layer descriptors.

Next: parse both files' `state_write_data` headers with the writer's own layout and name the extra section, then make our
writer match it. Only then can the goose-written file restore identically on the native reader.
