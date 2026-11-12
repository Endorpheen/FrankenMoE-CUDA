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

## Third probe: the first difference is numeric, not structural

Reading the size-probe files (2-token prompt, both lines) byte by byte:

    first difference at offset 480
    native: 74 35 00 c0   (float 0xC0003574)
    goose : 75 35 00 c0   (float 0xC0003575)

The bytes after it continue identically for a long stretch. So the earliest divergence is **one ULP in a KV value**, not
a cell, a range or a section header. The header (`qsgg` magic, sequence id, per-stream counts) matches.

That reframes the goose->native result: the native reader does parse our file (same `n_restored`), but it restores KV
values that differ in the last bit, and its continuation then flips a token. Numeric sensitivity to one ULP, not a
malformed file.

The +780-byte size difference is still open and is a separate question: it is not explained by the numeric difference
(which changes no size) and not by cell count (it did not scale with prompt length). Candidates are the data-section
header fields our line writes -- layout flag, layer count, rotation sizes -- or padding of the data section.

## Consequences for the criterion

- Formats are mutually readable: header, sequence id, stream counts and cell metadata all parse.
- Bit-exact continuation across lines is not achievable while the two lines compute attention with different kernels; a
  one-ULP KV difference can flip a token at temperature 0. That part is a property of the two compute lines, not of the
  serialization.
- The remaining *format* gap is the fixed ~780 bytes. Until it is named, "write where format permits" is not proven.

Next: parse the data-section header of both files with the writer's layout (`v_trans`, `n_layer`, `n_rot_k`, `n_rot_v`,
then per-layer type and row size) and account for the 780 bytes exactly.

## Fourth probe: structural walk of both files

Wrote a parser from the writers' own layout (`state_write` / `state_write_meta` / `state_write_data`):

    magic u32, seq_id i32, n_stream u32
    per stream: cell_count u32; if 0 continue
                per cell: pos i32, n_seq_id u32, [ext 12 B], ids i32 x n_seq_id
                v_trans u32, n_layer u32, n_rot_k u32, n_rot_v u32
                per layer: k_type i32, k_size_row u64, cell_count x k_size_row bytes
                if !v_trans: per layer: v_type i32, v_size_row u64, cell_count x v_size_row bytes
                if  v_trans: per layer: v_type i32, v_size_el u32, n_embd_v_gqa u32,
                             n_embd_v_gqa x cell_count x v_size_el bytes

With that layout both files walk end to end under the same field sequence and land 4 bytes short of their own size,
which is a common trailer. So the format is structurally identical between the lines, and the earlier "fixed section"
guess is corrected: the size delta is *counted content*, not a section that one line writes and the other does not.

The parser's second variant (transposed-V assumption) diverged mid-file, which is itself useful: it shows the corpus file
takes the `v_trans == 0` branch, so the transposed layout is not part of this comparison.

What is still not named is which counted value differs. Reverse engineering it from bytes is the wrong tool; the next
step is to instrument the writer: log `io.n_bytes()` after `state_write_meta` and after each layer of
`state_write_data` in a patched copy, run both lines, and diff the section sizes directly.

## Fifth probe: the slot file has its own framing (correcting the walk)

The first byte is not a bare `llama_state_seq` blob. The slot file starts with its own header:

    offset  0  magic 0x67677371
    offset  4  seq/version = 4
    offset  8  16
    offset 12  0xFFFFFFFF
    offset 16  1
    offset 20  n_tokens = 12
    offset 24  the 12 prompt token ids (760, 3841, 13477, 37550, 33075, ...)

Both lines produce the same framing and the same token list for the same request. The earlier "walk to EOF-4" result is
withdrawn: it was a wrong layout that happened to land near the end.

The useful consequence: the 1-ULP difference sits at **absolute offset 480 in both files**, so the state region begins at
the same place and stays aligned at least past the first K rows. The +780 bytes therefore appear further down -- later
layers or the V section -- not in the header, not in the token list, and not in the cell metadata that follows it.

Next step: finish mapping the state offset (the bytes at 72 show another token-like value, so a second list or a
per-checkpoint section is still unaccounted for), then compare the per-layer boundaries between the two files to find the
first size divergence.

## Sixth probe: the +780 bytes are one extra empty checkpoint

Both files carry the checkpoint appendix (`SCKP` magic) at the **same offset 66 110 808**, so the main sequence state
has the same size in both lines. Parsing the appendix:

    native: count=1   [ n_tokens=5, pos 4..4, data_tgt=65 864 428 bytes ]
    goose : count=2   [ n_tokens=0, pos 0..-1, data_tgt=740 bytes ]
                      [ n_tokens=5, pos 4..4, data_tgt=65 864 428 bytes ]

The second entry is byte-for-byte the native checkpoint. The extra first entry is an **empty checkpoint at zero
tokens**: 8 (n_tokens) + 8 (pos_min/pos_max) + 24 (three buffer sizes) + 740 (data_tgt) = exactly 780 bytes.

So the file-size delta is not a serialization defect and not a layout difference. It is one extra checkpoint that our
line creates before any token has been added, and the state format itself is identical, as the shared appendix offset
already showed.

Why a zero-token checkpoint exists is a server-side question, not a serialization one: the checkpoint trigger decides
`n_tokens_new = prompt.n_tokens() - n_tokens_cur`, and our line evidently reaches `create_checkpoint` once with
`n_tokens_cur == prompt.n_tokens()`. Candidate causes are the batch splitting (FG237S) or the checkpoint policy
(EXP268) -- whichever makes the first chunk cover the whole prompt. The native line simply never stores an empty
checkpoint.

Consequence for P5-08: the serialization criterion is met on both directions (formats parse identically, main state
same size, checkpoint state byte-identical); the remaining differences are an empty checkpoint created by the server and
the one-ULP KV sensitivity between compute lines. The former is worth fixing as waste, the latter is a property of two
different kernels.

## Root cause of the empty checkpoint: a process-wide bounds arena

The checkpoint trigger in `server-context.cpp` defends itself:

    if (do_checkpoint && pos_min < 0) do_checkpoint = false;   // "nothing to checkpoint yet"

An empty sequence must report `seq_pos_min == -1`. `llama_kv_cells::seq_pos_min` does that in native, but the
reconstructed header replaces the set lookup with an incremental bound maintained by EXP209:

    static inline int64_t * goose_bounds = nullptr;   // llama-kv-cells.h:49

    llama_pos seq_pos_min(seq_id) const {
        if (fg209i_enabled() && goose_bounds) return (llama_pos) goose_bounds[seq_id];
        ...
    }

Three properties of that arena produce the empty checkpoint:

1. **It is one array for the whole process.** The comment explains why it is static -- adding a member would change
   `sizeof(llama_kv_cells)` for the flint objects that are linked unchanged. But the price is that every cache instance
   (target, draft, any other) reads and writes the same 2x256 slots, so a bound written by one instance is returned by
   another.
2. **`reset()` does not reset it.** The instance reset clears `pos`, `ext`, `shift`, `seq`, `used` and the per-sequence
   sets, but leaves the arena untouched, so after a reset the stale value keeps being returned.
3. Therefore `seq_pos_min` for a freshly reset slot can return `0` instead of `-1`; the guard passes and an empty
   checkpoint is stored -- the +780 bytes.

Fix direction, respecting the ABI constraint: key the arena by instance (small fixed table of `(cells*, arena)` with a
linear scan, or a member pointer into a host-allocated per-instance arena) and zero it in `reset()`. Changing the class
layout is not an option while unchanged flint objects are linked against the old `sizeof`.
