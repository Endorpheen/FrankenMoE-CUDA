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

### The blocker is definitive for this compiler

The remaining idea - name the Goosе-side function differently from the libc
symbol - does not exist: the extern *string* is the C symbol name, and the
generated translation unit declares that name with the Goose prototype. So
`extern "open" fn goose_open(...)` still emits `int64_t open(sl_u8, int64_t,
int64_t)` and conflicts with `<fcntl.h>`. Same for `close` (verified by compiling
the generated C).

With that, the three candidate resolutions shrink to one that does not need a C
body: **no libc name that the generated C already declares can be called from
Goose today**. What remains possible without touching the compiler:

- the host keeps passing the mapping (today's state), and Goose owns everything
  else: the owner record, the generation counter, the close ordering and the
  live-frame refusal;
- or the compiler gains an explicit C prototype (a recorded requirement, not a
  change this experiment may make).

### What already exists, and what is actually missing

Checking before writing anything: `qr_model_open` (EXP919,
`goose/model/qwen35-model-loader.goose`) already takes the mapping pointer, the
slice and the file length, and maintains a twelve-word owner record with a
generation counter, while `qr_frame_open`/`qr_frame_close` refuse a model close
with a live frame and a double close. EXP919 gated 79 lifecycle/failure checks
over exactly that.

So the ownership semantics are implemented; the only missing piece is that the
*pointer, fd and length* come from the caller. That piece is precisely what the
compiler currently cannot express. There is therefore no separate module to write
here: EXP924's remaining scope is the OS acquisition, and it is blocked on the
recorded compiler requirement.

A C adapter is not a permitted answer - the repository rule counts every new
adapter as debt, and this one would exist only to work around a compiler
limitation.

### Verified: the shipped library already calls Goose for the tokenizer

`libllama.so.0` in the accepted Apex bundle (sha256 4eba8242…) exports 15
`fg202t*`/`fg213p*` symbols and contains the counter strings
`FG202T_TOKENIZER calls=%llu` and `FG213P_PIECE_COPY calls=%llu`. The shipping
runtime therefore already routes tokenizer and piece-copy decisions through Goose
bridges (the EXP202/EXP213 slices, visible in every native run's output), with
the C++ side still holding the strings and containers.

So the wire-up is not from zero: what remains is to replace the decisions behind
those bridges with the complete EXP920 modules (`pt_*`/`bt_*`) and delete the C++
bodies once the gates pass - the order rule 27 prescribes. No new adapter is
needed for that, because the bridge surface already exists.

### Where the bridges actually live

The bridge implementations are in `integration/m1/host/goose_host.c` (for example
`fg202t_st_i64` at line 294, with the comment "EXP683: goose-tokenizer.goose binds
its i64 store to fg202t_*"). The vendored `vendor/flint/src` does not reference
`fg202t`/`fg213p` at all, and neither do `patches/` or `work/`, so the *call
sites* inside the shipped binary come from the staged build tree produced by
`tools/stage-m1-tree.sh`, not from the sources in this checkout.

That matters for the replacement: before touching anything, the staged tree has
to be inspected to see where the C++ calls the bridge, because that is the seam
the complete EXP920 modules will occupy and the code that has to leave afterwards.
Blaming or editing the vendored copy would be editing the wrong file.

### The seam is in the reconstructed runtime tree

Searching the runtime store for the bridge symbols located the call sites: they
are in

    /home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/src/llama-vocab.cpp

lines 34-39 declare the Goose entry points (`fg202t_enabled`,
`fg202t_clamp_symbol_len`, `fg202t_bigram_valid`, `fg202t_try_add_allowed`,
`fg202t_resegment_kind`, `fg202t_bpe_add_allowed`), i.e. the EXP202 slice's
decision helpers are called from the BPE tokeniser in that file.

That file is the seam: it is the C++ that the complete EXP920 implementation
(`pt_*`/`bt_*`) has to replace, and it is where the deletion happens afterwards.
It is not in this repository, and it is not `vendor/flint` - editing either would
be editing the wrong file.

### Guard rail before touching that file

The seam is inside `franken-goose-exp150-restored-20261108`, which is preserved
runtime material used by accepted builds. Two rules apply and neither allows an
in-place edit:

- accepted executable and library sets are immutable (rule 22), and this tree is
  what produced them;
- a candidate is built in a separate directory and switched to only after the
  required checks.

So the wire-up does **not** edit that file. The next step copies the tree into a
new candidate directory, applies the replacement there, builds, and runs the
tokenizer gate - the EXP920 differential gate, which already passes byte-exactly
against the native tokenizer - before anything is adopted. Recording this before
the edit is the point: the seam being found is not permission to change accepted
material in place.

### Next step

EXP924 stops here, blocked on the compiler capability, with the blocker stated
precisely in this card. The next experiment does not have to wait for it.

The wire-up starts from what is already true: the shipped library calls Goose for
tokenizer decisions (`fg202t*`) and piece copies (`fg213p*`). The work is to put
the complete EXP920 implementations behind that existing surface, gate them
against native (EXP920's differential gate already passes), and then remove the
C++ bodies - in that order, with the measured tolerance question still open for
the trunk but not for the tokenizer, which is byte-exact.
