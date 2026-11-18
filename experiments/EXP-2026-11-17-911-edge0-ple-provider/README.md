# EXP-2026-11-17-911: bounded Goose PLE row provider

Status: ACCEPTED as a standalone synchronous row provider, not server-integrated.
Base: 5a234a58028d9f72ac872d491def69d1d0fc4fc0.

Hypothesis: compose accepted EXP909 descriptors and EXP910 FP8 decoding with
a bounded positioned-read loop in Goose, so PLE row batches require neither a
resident 51.2 GB tensor nor new C++ runtime policy. No performance claim.

The existing Goose os API only reads entire files; no Goose-callable pread
binding exists in the audited integration paths. `source/pread.c` is one
temporary OS-ABI adapter: direct POSIX pread with negative errno translation.
It is explicitly **adapter debt**, not migrated coverage. It owns no policy,
file, buffer, thread or cache and duplicates no existing Goose binding.
The existing native pio::pread_at is C++-mangled and not Goose-callable.

The provider borrows a caller-validated pinned PLE fd, row IDs, raw scratch and
FP32 output plus an eight-int64 descriptor workspace. The caller keeps them alive and does not close/truncate/mutate the
file during the call. Each call handles at most 16 rows, 2560 bytes raw and
10240 bytes decoded plus 64 descriptor bytes (12864 total), with no heap
allocation in the exported bodies. Caller buffers must be disjoint and honor
Goose borrow rules when entering through the C ABI. Rows may
repeat or arrive unsorted. All row/buffer/scale validation precedes I/O. Reads
use explicit offsets, not a shared file cursor. Short reads and EINTR retry at
most 192 times per row; EOF and errors fail. Output decoding starts only once
all raw reads complete; on an I/O failure output must be unchanged, while
scratch is unspecified. A successful return is the number of complete rows.

Acceptance fixed before building: compose/check before the small ABI build;
real rows match the independent FP32 oracle; invalid row/batch/scale/buffer
arguments fail before I/O; caller fd position is unchanged; short reads/EINTR
complete correctly, permanent EINTR is bounded, EOF/error do not commit partial
output; concurrent calls have independent scratch/output. Fault injection is
test-only. Wrong-offset/retry/commit mutations must fail their semantic gate.

This is a synchronous CPU row-provider capability, not an async I/O cache,
PLE hash/history/conv integration, latent graph, model loader or working Edge0
server. Apex remains immutable; no full runtime/model/GPU launch or C++ deletion.

## Results and retained negative attempts

The three-module composition, cheap Goose check, -O1 local ABI/link build,
corrected I/O gate and six -O0 mutations passed. **26 scenarios plus 512
concurrent calls** passed: real first/last/duplicate/unsorted rows, full 16-row
batches and empty batches; invalid/INT64-extreme IDs, wrong lengths, wrong/NaN
scale and invalid descriptors fail before a syscall. Real file position stays
unchanged. Every output matches EXP910's independent mathematical FP32 oracle.

The syscall fault shim forces seven-byte short reads, three initial EINTRs,
permanent EINTR, EOF after one successful row, EIO and an invalid oversized read
result. The first two complete exactly; permanent EINTR stops at 192 attempts;
all failures leave the entire output unchanged. A closed real fd fails too.
Scratch/descriptor contents are unspecified on failure; they remain caller-owned.

Six semantic mutations were killed: offset +1, treating a short read as complete,
not retrying EINTR, changing the retry bound, decoding before all I/O completes,
and validating later row IDs only after reading earlier rows. Compiler errors
and crashes do not count as killed mutations.

Two cheap composition attempts failed before any native build: a fixed local
array, even sliced and annotated, causes the pinned compiler to give exported
e0_ple_desc multiple concrete signatures. Minimal slice/array/indexed-call probes
isolated the condition; caller-owned descriptor slices solve it without changing
the compiler or accepted modules. Those failed attempts and probes remain under
the original and `-v2` build directories.

The initial fault test failed honestly: `-Dpread=e0_test_pread` still left a real
pread reference after the system header's symbol aliasing. Object-symbol
inspection confirmed it. Only the test bridge/fault library was rebuilt with an
explicit test symbol; the successful composition and normal -O1 library were
reused. The corrected fault gate passed, then the mutations ran. The old failed
gate and fault library were preserved, not overwritten or counted as success.

## Artifacts and limits

Accepted local gate directory:
`/home/end0/AI/LlamaServer/exp911-edge0-ple-provider-20261117-v3`.
Normal library: 60712 B, SHA256
`188fd0380f659e986a357d84bd5e0a8c7cda42babd1854e4233f1a441761e2a5`.
Corrected test-only fault library: 62560 B, SHA256
`f96472a11ed9dbb6911521276c569e4e71719a3e19b91a16fbf805087b484786`.
Receipt/source hashes are in `evidence.json`; adapter debt is explicitly listed
in `adapter-debt.json`. No generated C, shared libraries or model data enter Git.

No model was loaded, no full runtime rebuilt, and no inference speed/peak-memory
measurement was taken. The fixed batch buffers prove a bound for this API, not
for a complete inference process or filesystem page cache. The compiler runtime
still initializes itself. No C++ was added/removed, and the accepted Apex
executable/libraries remain unchanged. The adapter is a future removal target,
not completed runtime migration. This capability is not canonical server
coverage and does not establish PLE sequence-state correctness.

Selective reproduction: `tests/build.py --step check`, `build`, `gate` and
`mutations`, with explicit `--out` in a new LlamaServer directory and `--pack`.
The retained attempt used `fault-build` and `gate-retry` to repeat only the failed
fault path. Individual commands and mutant artifacts in the logs can be rerun
separately; do not repeat successful unrelated steps.
