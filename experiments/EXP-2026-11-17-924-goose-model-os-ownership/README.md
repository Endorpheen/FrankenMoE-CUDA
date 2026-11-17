# EXP-2026-11-17-924: model and OS ownership in Goose

Status: in progress.
Base: EXP923, 7484dbab.

## Why this is the next area

Every accepted component so far borrows its bytes: the GGUF mapping, the token
spans, the store tensors. EXP919 recorded the boundary explicitly ("mapping is
the caller's responsibility") because the pinned compiler refuses
`extern fn ... -> const u8[:]` with "nested references and slices do not cross".
The shipping runtime therefore owns nothing, and a runtime that cannot acquire
and release its own file is not an independent runtime.

## Hypothesis fixed before changes

Goose can own the model's OS resources itself, without a C++ wrapper, by
declaring libc primitives as externs and keeping their results in a caller-visible
owner record: `open`, `fstat`, `mmap`, `munmap`, `close`. The compiler accepts
scalar externs (it already accepts `expf`/`logf`), so the pointer and length
travel as `u64`/`i64`, and the bytes are then passed back into the existing
modules as slices exactly as they are today.

Ownership means, concretely:

- Goose opens the file, asks the kernel for its size, maps it read-only, and
  records fd, pointer, length and a generation counter in one owner record;
- every consumer takes the mapping from that record, so there is a single place
  that knows when it is valid;
- close refuses while any frame is live (`qr_frame_open` already tracks that),
  refuses a double close, and refuses a stale generation;
- close releases in the right order: frames, then the mapping, then the fd, and
  records that it happened.

## Contract and gate

The gate must show the ownership is real, not declared:

1. before open, `/proc/self/maps` has no entry for the model file and the fd
   count is the baseline;
2. after open, exactly one mapping exists for the file, its length equals the
   file size, and the fd is present;
3. a frame can be opened over that mapping and model close is refused while it
   is live;
4. after close, the mapping is gone from `/proc/self/maps`, the fd is gone from
   `/proc/self/fd`, and a second close and any read through the stale pointer
   are refused;
5. the existing reader/tokenizer/trunk gates still pass with the Goose-acquired
   mapping instead of the caller's.

No new C/C++ body: libc externs only, no adapter. If the compiler refuses a
needed signature, that is recorded as a compiler blocker and the work continues
on the parts that do not need it.

## Boundaries

No weights are copied; the mapping stays read-only and shared. The protected
Apex bundle, the baseline and the shipping server are untouched. Nothing here is
a speed result.

## Results

### Feasibility of the OS path from Goose

A probe with `extern "open" fn ...` typechecked and generated C, but the C
compile failed on a **name clash**: the generated translation unit includes libc
headers that already declare `open` (`int(const char *, int, ...)`) and `close`
(`int(int)`), so the Goose declaration `int64_t open(sl_u8, int64_t, int64_t)`
conflicts.

Renaming the Goose-side symbol (`extern "gopen"`) compiles, and the intended
binding at link time does not work directly either:
`-Wl,--defsym=gopen=open` fails with "expression refers to an unresolvable
symbol 'close'", because the linker only aliases symbols that are already
defined or undefined in that link.

So the blocker is *not* "Goose cannot call the OS" - it can declare and call
scalar libc externs - but "Goose cannot name a libc symbol that the generated C
already declares with a different prototype, and there is no clean link-time
alias".

Candidate resolutions, none of them a C body:

1. a compiler capability: declare an extern with an explicit C prototype
   (`extern "open" fn open_c(path: *const u8, flags: i32, ...) -> i32`), so the
   declaration matches the header;
2. a linker-level alias that works for a versioned libc symbol;
3. an existing Goose capability for the OS surface that this session has not
   used yet - to be searched before writing anything.

Recorded as the specific blocker, per the repository rule; the remaining parts
of the ownership work (owner record, generation, close ordering, live-frame
refusal) do not depend on it and are unaffected.

## Next step

Search the compiler's surface for a way to declare a libc-compatible prototype
(1) and for a working alias (2) before choosing anything; if neither exists,
write the requirement down for the compiler and continue with the owner record
and its failure semantics using a caller-provided handle, which is still a real
improvement over today's borrowed mapping.
