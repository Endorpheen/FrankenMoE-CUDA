# Fried Goose compiler: confirmed limitations blocking the FrankenMoE port

Audit date: 2026-11-08. Compiler under test: `subset-84f31960f9f3031e` (current supported build).
Prior build `stage3-f849233b999c` noted where behaviour differs.

## Re-audit 2026-11-09 against `subset-09897d96febb01b5`

A newer qualified self-hosted build was checked against every item below. Two are now closed and
one changed shape. All four of this repo's module gates were regenerated with the new compiler and
pass: 74123 + 6342 + 1965 + 593237 checks, zero failures.

| # | Limitation | Status on `0989` |
|---|---|---|
| 1 | no pointer types | **open** — `ptr` still `unknown type`; typed CUDA pointers explicitly not implemented |
| 2 | `const` globals not initialised for direct callers | **CLOSED** — `gg_init_globals(argc,argv)` now has external linkage and returns the temporary frame base; verified `K=7` through it |
| 3 | no SIMD / vector types | **open** — unchanged |
| 4 | no thread/warp/shared/barrier | **open** — `thread_id()` still unknown |
| 5 | no array types | **CLOSED** — `T[k]`, `T[..N]` with `push`/`append`/`resize`/`clear`/`pop`/`len`/indexing; real implementation via `gg_append` and an arena |
| 6 | no string type | **open** — `string` still `unknown type` |
| 7 | no ordering primitive | **narrowed** — no builtin `sort`, but orderings are now *writable*: an insertion sort over `(first,last)` pairs was written in Goose and wired into the loader, replacing the `std::sort` that item 7 forced to stay in C++ |
| 8 | `pad`/`block` reserved | **open** — unchanged |
| 9 | no inlining / constant-argument specialisation | **open** — unchanged |
| 10 | checked division | **open** — `gg_div` guards unchanged |
| 11 | body deduplication | **open** — two identical bodies still collapse |
| 12 | float→int conversion | **still closed** |

### What the array support actually enables here

Confirmed on an isolated stand first, per the owner's instruction:

- `var g:i64[>..]=[]` with `push`/`pop`/`resize(n)`/`resize(n,fill)`/`clear`/`g.len`/`g[i]` works and
  generates real code (`gg_append`, `gg_view`, `gg_header`), not stubs.
- `resize` without a fill only shrinks — "resize without fill may only shrink". That is behaviour,
  not a defect; the first probe of mine was wrong.
- Slices `g[1..3]` into `T[..]` are **not** implemented yet ("runtime-capacity limited array
  storage"), and non-fixed struct fields are **not** implemented ("nonfixed struct fields"), so a
  growable array cannot yet be a struct member.
- Non-fixed values are passed by reference; passing one as a plain parameter needs `&` or an
  explicit `copy`.

That is enough for the one thing that mattered: `merge_ranges` in the loader no longer keeps its
ordering in C++.

### Evidence of what changed in the emitted code

For every module, all `gg_function_*` bodies are byte-identical between the two compilers. The
only differences are the new `gg_globals_ready/claim/publish` trio, a rewritten `gg_program`, and
`GG_UNALIGNED` typedefs for packed-data access.

Every item below was reproduced on this machine with a minimal input or measured on a real
Apex run. Nothing here is speculative. Intended for the compiler backlog, after the priority
work on full stdlib support.

Severity is about the Apex port: **blocker** = a component cannot be ported at all,
**cost** = it ports but loses the performance a port exists to preserve,
**friction** = it ports but the workaround risks silent divergence from the native source.

---

## 1. No pointer types and no pointee provenance (severity: blocker for CUDA, cost everywhere)

**Missing capability.** There is no `*T`. The only way to name an address is `u64`, so every
device buffer is passed as an integer and dereferenced by casting it back inside the module.

**Concrete example.** `experiments/EXP-2026-11-08-184-goose-cuda-mmvf/source/cuda-runtime.cu`:

```c
static __device__ __forceinline__ float fg184m_ld_f32(uint64_t p, uint64_t o) {
    return *(const float *)((const char *)(uintptr_t)p + o);
}
```

All 14 ported CUDA modules use this shape.

**Technical cause.** Once the address is an integer, `nvcc` has no aliasing or constness
information. It cannot emit `LDG.E.CONSTANT` (the read-only path), cannot fold the offset into
the load's addressing mode, and cannot hoist loads across stores. Measured in EXP198 on the same
kernel with four variants: the typed version produced 56 SASS instructions and `LDG.E.CONSTANT`;
the integer version produced 64 and a plain `LD.E`.

**Two workarounds tried, both insufficient.** Pre-adding base and offset on the host still
yields an integer parameter. Splitting into two integers (`base`, `index*stride`) does not help
either, because the multiply and the add remain in the kernel.

**What to fix.** Typed address parameters in the language, e.g. `fn load(p:ptr<f32>) -> f32`,
lowered to real pointer types in the emitted C/CUDA. A per-parameter `const` qualifier for
read-only buffers would let the compiler use the non-coherent load path. This is the single
highest-value change for the CUDA half of the port.

---

## 2. `const` globals are not initialised when generated functions are called directly (severity: blocker)

**Missing capability.** The emitted module initialises top-level constants only inside
`gg_program`, which is reached only through `main`.

**Concrete example.**

```goose
const K:i64 = 7;
fn get() -> i64 { return K; }
fn main() { get(); }
```

The compiler emits:

```c
static int gg_program(int argc, char **argv) {
  ...
  gg_globals.v0 = ((int64_t)(((int64_t)(UINT64_C(7)))));   // line 503
  gg_function_0(frame.base + 0);
  ...
}
```

`gg_function_1`, the body of `get`, reads `gg_globals.v0`. Any host that renames `main` (which
every integration bridge does, as in `#define main fg201t_generated_main`) and calls the body
function directly never runs that assignment and silently reads `0`.

This is not theoretical: it broke EXP199 (`W`) and EXP201 (`MAXD`) during porting, both times
as a wrong constant rather than a crash.

**What to fix.** Emit constant initialisation as a static initialiser on `gg_globals`, or as a
separate `gg_init_globals()` that the emitted module exports and the host can call. Renaming
`main` is a supported pattern in this project and must not change program semantics.

---

## 3. No SIMD and no vector types (severity: blocker for the CPU expert path)

**Missing capability.** Goose has scalar `f32`, `f64` and integer types only. There is no way
to express an N-wide vector operation, and no intrinsics.

**Concrete example.** `ggml_vec_dot_q4_K_q8_K` in `libggml-cpu.so` contains 35+ vector
instructions in its hot loop (`vfmadd*`, `vpmaddubsw`, `vpdpbusd`, `vpshufb`, `vpermd`,
`vpblend*`, `vmovdqu`). Under Apex's `--cpu-moe` profile this is a dominant cost, because the
256 experts run on the CPU.

A scalar Goose transcription would be correct but an order of magnitude slower, which turns the
port into a regression and so cannot be promoted under the project's 2% rule.

**Technical cause.** The IR has no vector type and the emitter targets portable C, where
auto-vectorisation of a scalar kernel written in the Goose style (flat `u64` offsets, opaque
load/store primitives) does not happen.

**What to fix.** Either a `f32x8`-style vector type with elementwise arithmetic that lowers to
`__m256` / NEON, or a documented intrinsics escape hatch that the emitter passes through with
correct types. Without one of these the CPU half of the port is physically impossible.

---

## 4. No thread, warp, shared-memory or barrier primitives (severity: cost, blocker for some kernels)

**Missing capability.** No `thread_id`, no `barrier`, no shuffle, no shared memory declaration.

Confirmed: `fn f() -> i64 { return thread_id(); }` and `barrier()` both fail with
`error: unknown function`. EXP181's warp reduction works only because `__shfl_up_sync`,
`__any_sync` and the lane index are supplied as host-side extern primitives, and the probe
(`tests/probe.cu`) had to prove their semantics independently.

**Technical cause.** The language models a single-threaded data stack, not a GPU execution
hierarchy. Any kernel that needs cross-thread cooperation must have all cooperative
instructions supplied from outside, which pushes kernel structure into C++ and defeats the
purpose of the port.

**What to fix.** First-class `block_id`, `thread_id`, warp lane, `barrier()` and a shared-memory
declaration, with the emitter mapping them to `blockIdx`, `threadIdx`, `__syncthreads()` and
`__shared__`.

---

## 5. No array types (severity: friction)

**Missing capability.** `var a:[4]i64` fails with `error: expected identifier`. Buffers exist
only as flat `u64` bases plus extern `ld_*`/`st_*` primitives with computed byte offsets.

**Concrete example.** Every ported module, e.g. `fg201t_st_i64(base, byte, v)`, and the pair
arrays in EXP201's `merge_ranges`, which the caller allocates and Goose walks by index.

**Technical cause.** The type system has only scalars and structs.

**What to fix.** Fixed-size array types, and ideally a slice/reference form so a function can
accept a buffer with a length instead of two integers.

---

## 6. No string type (severity: blocker for the tokenizer, the last unported subsystem)

**Missing capability.** `let s:string = "ab";` fails with `error: unknown type 'string'`.

**Concrete example.** `llama-vocab.cpp` is 4847 lines built on `std::string`: symbol slices,
`text_to_token`, `rev_merge` lookup. EXP202 could port only the numeric decisions
(`utf8_len`, `bigram_valid`, `try_add_allowed`, `resegment_kind`, `clamp_symbol_len`); every
string operation still lives in C++.

**Technical cause.** No string or byte-slice type, and no lookup-keyed container.

**What to fix.** A string/byte-view type with length, plus comparison and hashing, so a token
lookup can be expressed in Goose. Without it the tokenizer cannot be ported beyond predicates.

---

## 7. No ordering primitive (severity: friction)

**Missing capability.** `sort(x)` fails with `error: unknown function 'sort'`.

**Concrete example.** `merge_ranges` in `llama-model-loader.cpp` begins with
`std::sort(ranges.begin(), ranges.end(), ...)`. EXP201 ported the merge pass to Goose and had to
leave that sort in C++, recording the split explicitly rather than pretending the port is
complete.

**What to fix.** A sort over an array of a comparable type, or enough of stdlib to write one
efficiently.

---

## 8. `pad` and `block` are reserved identifiers (severity: friction)

**Missing capability.** These cannot be used as variable or function names.

**Concrete example.** `let pad:i64 = 1;` → `error: expected identifier`; same for `block`,
which already forced a rename to `bs` in EXP177.

**Technical cause.** The parser reserves the padded-layout and block keywords for all
identifier positions, not just the contexts where they are meaningful.

**What to fix.** Allow them as ordinary identifiers, or at minimum in local scopes. The cost is
small but recurring: each rename is a place where the Goose source and the C source it was
transcribed from silently disagree, which is exactly the failure mode the port must avoid.

---

## 9. No inlining or constant-argument specialisation (severity: cost)

**Missing capability.** There is no way to mark a function `__forceinline__` or to have the
emitter specialise a body on a constant parameter.

**Concrete example.** EXP180 (`quantize_mmq_q8_1`) is byte-exact on all four cases and both
paths execute, but it is 11.4% slower than native. The measured cause is code shape: 824 SASS
instructions versus native's 160, including 206 `IMAD` against native's ~20. Six optimisation
rounds brought it from +13.5% to +11.4% and then stalled, so it was not promoted.

**Technical cause.** Each Goose function becomes one out-of-line `gg_function_N`. Parameter
values that are constant at the call site stay unknown inside the body, so all address
arithmetic is recomputed instead of folded.

**What to fix.** Call-site constant propagation into the emitted body, and an inline hint the
emitter honours. This is what turns the MMQ family — 40% of GPU time — from unportable to
portable.

---

## 10. Checked arithmetic on division and modulo (severity: cost)

**Missing capability / behaviour.** Every division emits two guard branches.

**Concrete example.** From `cells-generated.c`:

```c
static int64_t gg_div(int64_t a, int64_t b) {
    if (!b) gg_fail("division by zero");
    if (a == INT64_MIN && b == -1) gg_fail("division overflow");
    return a / b;
}
```

EXP186 measured the effect on a real kernel: 79 `ISETP`+`SEL` pairs versus 7 in the native
version.

**Technical cause.** Division is lowered through a checked helper unconditionally.

**What to fix.** Drop the guards when the divisor is a compile-time non-zero constant, and make
the checked helper opt-in rather than the default. Note that integer addition already wraps
correctly without guards (verified: `x + 1` emits a plain unsigned add), so only division and
modulo need this.

---

## 11. Identical function bodies are deduplicated (severity: friction)

**Missing capability.** Two wrappers with identical bodies and identical call arguments collapse
into one `gg_function_N`.

**Concrete example.** In EXP201, `resegment_kind` and `clamp_symbol_len` both take two `i64`
arguments and both landed as candidates for the same numbering; the bridge had to be checked
against the generated bodies to confirm which was which. Module authors work around this by
calling every wrapper from `main` with distinct arguments purely to force extraction.

**Technical cause.** Body deduplication keys on the generated body text.

**What to fix.** Emit one function per source function even when bodies coincide, or export a
stable name-to-symbol map so bridges do not have to pattern-match arity and body shape.

---

## 12. Resolved, recorded so it is not lost

`as` from float to integer was rejected by `stage3-f849233b999c` with
`error: not implemented yet: defined wrapping float-to-integer conversion`. The current build
`subset-84f31960f9f3031e` accepts it and emits `gg_float_integer_wrap`. This unblocked the MoE
quantisation paths, which previously needed an extern primitive. Whoever maintains the backlog
should keep this as a regression test.

---

## What this means for the port

Items 1 and 9 gate the CUDA half: together they explain why 40% of GPU time (the MMQ family) is
byte-exact but unportable at acceptable speed. Item 3 gates the CPU expert path, which under
`--cpu-moe` is the larger single cost. Item 6 gates the tokenizer. Item 2 is a correctness
hazard that has already produced two wrong-constant bugs and will keep doing so until it is
fixed, because every bridge renames `main`.

Items 5, 7, 8, 10, 11 and 12 are friction or moderate cost and do not stop the port.
