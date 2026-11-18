# EXP-2026-11-17-909: Goose descriptors for the external Edge0 pack

Status: ACCEPTED as a standalone descriptor module, not linked into the server.
Base: cd67aa3f8622385969dec4577c145cb71d56fe2f.

Hypothesis: a stateless Goose module can own the external pack's validated
offset/shape/type decisions without adding C++ runtime logic or duplicating
weights. It is one dependency of the missing provider, not an end-to-end model.
No inference speedup is predicted. This does not change the Apex baseline.

Scope: mixed Q3_K gate/up and IQ4_NL down expert records, Q4_0/Q8_0 per-group
D/U, raw FP8 PLE row byte ranges, little-endian routing labels, and F32 column
scale descriptors. Source facts: pinned HF pack from EXP908 and the developer's
D/U layout and real-Q3_K reference at Edge0 commit
f6bfa5546bcf89919f86946ec669bcc7f4cd526d. The latter confirms gate rows precede
up rows, followed by down, rather than an interleaved gate/up layout.

Acceptance fixed before building: exact independent-oracle agreement for every
48x512 expert record and every layer/group D/U descriptor; file ranges never
escape the pinned files; invalid/INT64-extreme indices and short output slices
return failure without writes; real labels match all 24576 rows; independent
thread outputs agree. No heap/file/FFI ownership is added. Wrong-offset/type/
stride mutations must be rejected by the gate.

The local generated-C build is necessary to check the emitted slice ABI and
actual callable symbols, which `goose check` alone cannot validate. All build
outputs must live in a new isolated LlamaServer directory. No full runtime
rebuild, server/model launch or C++ removal belongs to this experiment.

## Results

The cheap Goose check passed. The pinned compiler generated the C and ABI
header in `/home/end0/AI/LlamaServer/exp909-edge0-layout-20261117`; GCC13 built
only that module at -O1. Generated slice declarations use pointer + int64 length;
the gate called the actual exports by value and verified output guard words.

**199850 checks passed**: 73728 expert matrices, 1536 D/U matrices, all 24576
real routing labels, native table agreement, scale/PLE boundary descriptors,
short output slices, invalid indices including both INT64 extremes, unchanged
borrowed label data, and 8192 concurrent calls with independent output arrays.
The oracle derives sizes from independent quant block shapes and confirms
every descriptor stays inside its pinned real file. It reads only labels and
text metadata; weight files are stat'ed, not mapped/loaded.

Six separate -O0 mutations were killed by explicit semantic assertions:
expert down offset +1, incorrect down type, gate row stride +1, D/U offset +1,
missing PLE header offset, and a group-label limit that rejects group 7. None
was credited merely for a compiler error or a crash. All four step logs end
in RESULT PASS and retain exact commands, output and return codes.

Source SHA256: `2b2a4d8611e95b13b21c1f11726b1c14c3a9ce4e5a716c67611cef270b65063e`.
Local module library: 44112 bytes, SHA256
`6f435e85b5dcff0f002b372abc9dde2a088bed8eb2a0dafead0ccf4666c15ffb`.
Raw evidence hashes are in `evidence.json`. One redundant-cast compiler warning
is retained; it does not affect the gate.

No model performance or memory measurement was run. Export bodies allocate no
storage, retain no pointers, perform no I/O and call no host FFI; the standalone
compiler runtime still has its own initialization. No production C++ was added,
replaced or removed. The library is an isolated gate artifact, not a server
runtime replacement and not proof of an Edge0 forward pass.

## Remaining dependency chain

1. Bind the descriptors to lifetime-owned external tensor providers, enforcing
   budgets and stream/event lifetime without duplicating all weights.
2. Decode FP8 E4M3 PLE rows and preserve PLE token/conv state. GGUF storage I8
   must not be interpreted as signed int8 values.
3. Compose and numerically verify latent D/U + routed/shared-expert arithmetic,
   labels and column scales against an independent reference.
4. Integrate the pack with the existing qwen4exp backbone and shared server;
   only after the required correctness/memory gates attempt the one controlled
   short Russian response. Apex remains immutable throughout.

Selective reproduction: run `tests/build.py --out <new-LlamaServer-directory>
--pack <pinned-pack> --step check`, then `build`, `gate` and `mutations` as
needed. Existing successful logs and binaries are deliberately never
overwritten; use a new directory when an input changes. The module is not yet
in the canonical Goose tree or its coverage count.
