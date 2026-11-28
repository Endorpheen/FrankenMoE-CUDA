# Technical debt ledger

Working notes for the Goose migration. Each item is a reproducible observation
with a location and a reproduction, so it can be handed to whoever owns the
relevant component without a conversation.

## Remaining non-Goose runtime inputs

The four Bonsai MTP and Pipeline OpenCL C sources were added by
`1599aed9239b61354d73e67d8aba08003c4431d9` on 2026-11-27. Their original
physical line counts were verified against the sources before consolidation.

| Original runtime source | Lines | Purpose |
| --- | ---: | --- |
| `integration/goose-standalone/universal/bonsai-frozen-cuda/mtp-mvq.cl` | 6816 | Bonsai MTP |
| `integration/goose-standalone/universal/bonsai-frozen-cuda/pipeline-main.cl` | 5425 | Bonsai Pipeline main kernels |
| `integration/goose-standalone/universal/bonsai-frozen-cuda/pipeline-flash.cl` | 4566 | Bonsai Pipeline attention kernels |
| `integration/goose-standalone/universal/bonsai-frozen-cuda/pipeline-unpack.cl` | 5053 | Bonsai Pipeline unpack kernels |
| **Original Bonsai total** | **21860** | |

The first 4011 lines match byte for byte in all four files. Their three
redundant copies account for 12033 lines, leaving 9827 after accounting for
that prefix alone. The three Pipeline files share another 161 lines, and
`pipeline-main.cl` and `pipeline-unpack.cl` share a further 371 lines.
Consolidating those verified prefixes removes 12726 repeated source lines
in total. The seven resulting input files contain 9142 physical lines,
including six include directives and two additional separator lines. The
underlying source text totals 9134 lines after these extractions.

The shared prefixes now reside in these runtime inputs.

- `integration/goose-standalone/universal/bonsai-frozen-cuda/bonsai-common.clh`
- `integration/goose-standalone/universal/bonsai-frozen-cuda/pipeline-common.clh`
- `integration/goose-standalone/universal/bonsai-frozen-cuda/pipeline-mma-common.clh`

Expanding the includes reproduces all four original sources byte for byte.
The standard `tools/build-gooseinfer-bonsai-mtp.sh` and
`tools/build-gooseinfer-bonsai-pipeline.sh` recipes produce byte-identical
SM89 cubins before and after the extraction. Their respective SHA-256 hashes
are `8c56c4e79e9afee837188324c66419c49fbef050b83c3f599978afa32313a0ec`
and `2a18d1857ecb4067e30eb9ab6cc4f9c61cab7d6c46985b02c59cb57616b5c7cd`.

The separate Inkling Flash C ABI source is
`integration/goose-standalone/universal/cuda/images/device-types.h`.
It contains **10 physical lines** of type aliases and ABI macros and was
added in the same integration commit.

After the next qualified Linux and Windows release, audit remaining
duplication, reuse universal Goose kernels, and port the remaining unique
Bonsai OpenCL C compute and Inkling ABI source. Follow the
[required sequence](../../ROADMAP.md#required-work-sequence) through the
final runtime and build audit.

## TD-S1: a function result stored in an f64 local reads as zero

Observed while porting `dist` and `temp_ext`.

| case | result |
|---|---|
| `let x: f64 = 1.5; p[0] = x as f32` | 1.5 (correct) |
| `let y: f64 = lv[0] as f64 * 2.0` | 4 (correct) |
| f64 accumulator over a loop | 9 (correct) |
| `(mt_canonical(id) * 4294967296.0) as i64` in one expression | correct |
| `let u: f64 = mt_canonical(id); p[0] = u as f32` | **0** |
| the same with `* 1.0` or `+ 0.0` in the initialiser | **0** |

Reproduction: dispatch op 22 of `experiments/EXP-2026-11-18-1031-p6-10-typical`
with `sm_mt_seed(state, 1)` first; prints `bare=0 *1.0=0 +0.0=0` where the
native values are 0.997184808, 0.932557361 and 0.128124448.

Impact: `dist` (draws `mt_canonical`) and `temp_ext` (stores an extern `exp`
result for the double renormalisation) cannot be completed bit-exactly in f32,
so both are unported. Nothing else depends on it.

## TD-S2: generated function ids shift whenever the module changes

A caller that hardcodes `gg_function_N` silently calls a different function
after an unrelated edit (this produced a false filter defect during top-p).
Mitigated: `tools/build-sampler-chain.sh` derives the ids into a header, and the
module exposes one stable entry point per stage.

## TD-S3: module constants require an explicit init call

`gg_init_globals()` is called by the generated `main`, not by the module's
functions. A host that embeds the module must call it; otherwise every module
constant reads as 0 (`x & M32` was 0, so the MT stream returned zeros).
Mitigated: the shim runs it from a constructor.

## TD-S4: no export mechanism, and unused functions are dropped

`export fn`, `pub fn`, `@export` and attribute forms are all rejected and there
is no export flag. A module without `main` cannot be compiled, and functions not
reachable from `main` are removed. Mitigated: the build appends a root that
calls the entry points.

## TD-S5 (resolved): an entry removed as unreachable re-points its id

Observed while making the top-p entry's signature unique.

```
grep -c marker build/sampler-chain/sampler-chain-root.goose      -> 2
grep -c 171    build/sampler-chain/sampler_chain.c               -> 0
prototype stays: gg_function_2(gg_slice, gg_slice, gg_slice, int64_t, float, int64_t, int64_t base)
```

The source signature gained a trailing `marker: i64` that the body compares
against 171, the root file the compiler consumes contains it, a direct compile
of that same root file reports rc=0 and still produces no `171` and no
seven-argument prototype. A different edit to the same file (a constant value)
did change the output earlier, so this is not a blanket cache.

Cause found: the entry was not reachable from `main`, so the compiler removed
the function entirely and the id the build script matched belonged to a *neighbour*
with the same prototype shape. That is why the shim called different code and why
the signature edit never appeared.

Rule that follows: every entry point must be called from the build root. The
build script now calls each one, and the id header is generated only after that.

## TD-S6: the accepted Apex profile uses a flag that no source tree here defines

`tools/apex-test.sh` starts the accepted binary with `--moe-cache-mib "$CACHE"`,
but the string appears nowhere in `work/llama.cpp-integration`,
`vendor/flint` or the restored 150 runtime sources (the search covered
`common`, `src`, `tools` and the patch directory). Our own build of the working
source therefore rejects the flag:

```
build/expert-tier-franken-cuda/bin/llama-server ... --moe-cache-mib 4992
error: invalid argument: --moe-cache-mib
```

Consequence for the acceptance A/B: the profile cannot be reproduced by building
the source we have. Either the flag lives in a patch that is not in the
repository, or the accepted binary was built from a tree that is not preserved.
Until that is settled, an A/B that includes the flag can only use the accepted
binary, and an A/B of our Goose build must state that the flag was omitted on
both sides and that the parameter set therefore differs from the reference.

### TD-S6, sharpened

A second look changed the picture:

- the accepted binary accepts `--moe-cache-mib` and rejects a bogus flag, so the
  option is really implemented there (its literal is simply not visible to
  `strings`);
- the restored runtime tree carries `src/llama-moe-cache.h` and uses it from
  `llama-context.cpp`, `llama-graph.cpp` and others;
- **`work/llama.cpp-integration` has no `llama-moe-cache` at all** (no files, no
  mentions in `llama-context.cpp`).

So the builds made so far came from a tree that is not the cumulative baseline:
they cannot reproduce the accepted profile because the whole MoE-cache feature is
absent, not because of one missing flag.

Consequence: the Goose runtime must be built from the restored runtime tree
(`.../franken-goose-exp150-restored-20261108/runtime/reconstructed`), which holds
the accepted components, with the sampler shim added there. Measuring a build of
`work/` against the reference would compare two different runtimes and prove
nothing about the migration.

## TD-S7: a range-based edit deleted three entry points

While rewriting `sm_apply_penalties` I replaced the text from its definition to
the definition of `fg_sampler_penalties`. Three entries defined in between
(`fg_sampler_typical`, `fg_sampler_top_p`, `fg_sampler_xtc`) were deleted with
it; the compiler then failed on an unknown callee, and `git checkout` restored
the file.

Rule that follows: when rewriting a function in a file that holds several
top-level declarations, bound the edit by the function's own text, never by
"from here to the next name I happen to know". Verify with `grep -c "fn "`
before and after the edit, and keep the pre-edit file recoverable.

No data was lost: the deleted entries were restored from HEAD and the penalties
gate still reports 200/200.

## TD-S8: `export fn` exists in the canonical compiler, and it emits a stable name

Recorded 2026-11-18. **This corrects TD-S4**, which said Goose has no export mechanism. That
was true of the caret `simd7` compiler and false of the canonical build compiler, and the
difference changes how entry points should be named.

Reproduction, one file, two compilers:

```
printf 'export fn foo(a: i64) -> i64 {\n    return a + 1;\n}\n\nfn main() {\n    let r = foo(41);\n    if r < 0 {\n        return;\n    }\n}\n' > exptest.goose

# canonical compiler (the one tools/build-main-runtime.sh uses)
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose -o exptest.c exptest.goose
grep -n foo exptest.c
  1332:static int64_t foo_g(int64_t p0);
  1365:int64_t foo(int64_t gs_export_arg0) { ... }

# caret compiler
/home/end0/AI/Fried-Goose/.cache/goose/simd7 --check exptest.goose
  error: expected top-level declaration  (at `export fn`)
```

The emitted C contains a real global function:

```
gcc -O1 -c exptest.c -o exptest.o && nm -g --defined-only exptest.o
0000000000000000 T foo
0000000000000022 T main
```

Consequences:

1. Every id-based workaround was a workaround for one compiler, not for the language. The
   `gg_function_N` derivation, the "entry point must be called from the build root or it is
   deleted with its id landing on a neighbour" rule, and `gen_ids.py` exist because of it.
2. The shipping sampler chain should switch its entry points to `export fn` with stable names,
   which removes the whole class of failures that cost this run several iterations: a shifted
   index silently ran typical-p where top-p was measured, and 747 of 900 comparisons were
   wasted on a misidentified function.
3. An exported function still needs a call to exist in the output? **No** - the emitted
   `foo` is present because it is exported, and `main` in that file also calls it. The case
   of an exported function with no caller has not been tested yet, and it must be: if export
   alone keeps the body, TD-S5's rule can be relaxed to "export it" instead of "call it".

### TD-S8, the open question answered

An exported function needs **no caller** to survive. Reproduced the same day:

```
printf 'export fn bar(a: i64) -> i64 {\n    return a * 3;\n}\n\nfn main() {\n    let r = 0 - 1;\n    if r < 0 {\n        return;\n    }\n}\n' > exptest2.goose
goose -o exptest2.c exptest2.goose
gcc -O1 -c exptest2.c -o exptest2.o && nm -g --defined-only exptest2.o
0000000000000000 T bar
0000000000000022 T main
```

`bar` is present even though `main` never calls it. So on the canonical compiler:

- **exported** functions are emitted and keep a stable name whether or not anything calls them;
- the "call it from the build root" rule was a workaround for the caret compiler, where an
  unreachable function was dropped and its id silently moved to a neighbour.

Action, not yet taken: the shipping sampler chain and the grammar shim still address entry
points by `gg_function_N`. Converting them to `export fn` removes that fragility and lets the
host declare plain prototypes. It is a contained change in
`goose/sampling/sampler-chain-full.goose` plus the two generators, and it should be done before
the next chain change rather than during it.

## TD-S9: what switching the chain to `export fn` actually needs

Attempted 2026-11-18 and deliberately reverted before it could leave the tree broken. The
switch is right; it is not a one-line change, and here is everything measured, so the next
attempt starts from facts.

### The good part, verified

With the canonical compiler the whole chain typechecks and generates:

```
GOOSE_HOME=$PWD GOOSE_COMPILER=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose \
    bash tools/build-sampler-chain.sh
typechecked ok: 78 specialization(s) ... wrote sampler_chain.c (255281 bytes)
```

Twelve entry points then resolve **by name**, and the generated C carries them at top level:

```
int64_t fg_sampler_chain(sl_f32, sl_f32, sl_i64, int64_t, float, int64_t, float, float)
int64_t fg_sampler_dry(sl_i64 x6, sl_f32, sl_i64, int64_t x4, float x2)
...
```

There is no `gg_function_N` anywhere in that output, so the entire id-fragility class is gone
for this build.

### The exported ABI differs from the caret one

- slices are structs `{ T * data; int64_t len; }`, named `sl_f32`, `sl_i64`, `sl_u8`, not
  `gg_slice`;
- there is no `gg_init_globals`; a host calls `goose_init(int argc, char **argv)`, which is
  public **only** when the C is generated with `--header out.h`;
- with `--header`, the generated C has no `main`, so the shim can be its own translation unit
  instead of `#include`-ing the generated C to avoid duplicate symbols.

### One function refuses to export

`fg_sampler_typical` fails with `could not resolve one concrete function signature`, while its
thirteen siblings export fine. Reduced attempts that do **not** reproduce it: three slices of
the same element type, an exported function called from two places, an exported function with
four slices. A thin exported wrapper around it resolves, so the door is available under another
name - but the cause is unknown and belongs to the compiler agent, not to guessing.

### What remains to do

1. rewrite both shim generators for the `sl_*` structs and `goose_init`;
2. keep `fg_typical_entry` as the exported door for typical-p;
3. rebuild the host library and re-run the sampler A/B to confirm unchanged results.

Until then the chain keeps the caret compiler and the id header, which is the state that is
accepted and measured.

### TD-S9 resolved: the switch is in

The chain now builds on the canonical compiler with `export fn` entry points:

```
compiler   /home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
generate   --standalone --header sampler_chain.h -o sampler_chain.c
entries    resolved by exported NAME, not by prototype shape
```

Three things had to be right, and each was found by failing:

1. **`--standalone` together with `--header`.** `--header` alone leaves `gs_rt_start` undefined,
   because the runtime is expected from a separate link; `--standalone` writes the runtime into
   the same C, and the result's only undefined symbols are libc and libm.
2. **The generated C is its own translation unit.** The shim used to `#include` it to avoid
   duplicate symbols, which is no longer needed and would now duplicate `goose_init`'s runtime.
   CMake compiles `sampler_chain.c` with `-Dmain=fg_chain_main` and the shim separately.
3. **The grammar wrapper is generated from the header.** The in-module marker is the last
   parameter; the host never passes it, so the wrapper drops it and supplies the constant. The
   parameter count is read from the compiler's prototype, which is why the host's 24-parameter
   declaration now matches instead of being hand-maintained.

Evidence:

```
[ON ] '\n\nHere\'s a sentence about the sea:\n\n"The vast ocean stretche'
[OFF] '\n\nHere\'s a sentence about the sea:\n\n"The vast ocean stretche'
FG171_DIST goose calls=1
FG171_TOP_P goose calls=1
[ON ] decode=49.46   [OFF] decode=50.44
```

Identical text, real Goose calls through the exported names, and the decode difference is the
known chain cost, not a new one.

`fg_sampler_typical` remains non-exported with `fg_typical_entry` as its door: the export
failure is still unexplained and still belongs to the compiler agent.

## TD-S10: the ported grammar automaton is correct and ~12x too slow

Measured 2026-11-18 with the port deciding the full candidate set (it used to decline, which is why
the earlier figure was 3-5x):

```
decode  4.32 tok/s vs 50.3   -> 11.7x slower
prefill 5.85 tok/s vs 20.9   ->  3.6x slower
```

Correctness is not in question: zero disagreements against `llama_grammar_reject_candidates` over
1 736 513 candidates, and constrained output identical to the reference library.

The cost is in the generated per-candidate loop, not in the host. Eight host-side alternatives were
tried and closed earlier (allocation, per-rule caching, array writes, first-codepoint pre-filter),
so this needs the compiler: the loop walks a per-candidate codepoint array with bounds checks and
function calls, where the native code walks a pointer chain.

Until the loop is comparable, the grammar stage stays opt-in and P6-10's grammar item stays open.

### 2026-11-19: the correctness claim above does not hold on the shipped path

`tools/apex-grammar-pair.sh` runs one constrained request on the shipped build with `GGML_GOOSE_GRAMMAR` unset and with it
set, temperature zero, same seed, twice each:

```
off: {"city": "Tokyo", "country": "Japan"}        (2 of 2 runs)
on:  { "city" : "Dhokar" , "country" : "Nepal" }  (2 of 2 runs)
```

The baseline repeats exactly, so this is not run-to-run variance: the automaton changes the answer on the path the server
uses. Both strings obey the grammar, which means the **accepted set** differs at some position - the hook allowed a token the
native stack did not, or refused one it would have allowed - and greedy decoding then chose differently.

That is the defect the flag's own comment described, not the equality the standalone gate measured. The two are different
questions: the gate compares decisions over 1 736 513 candidates of a real vocabulary, and the server passes a state the gate
does not reproduce. Until this is understood, the grammar stage stays off and the deletion of the native grammar body stays
blocked by rule 15 - a 12x speed problem would be a trade, a different answer is not.

Relevant detail for whoever takes it: neither run prints any marker from the grammar door, so the evidence is behavioural
rather than traces, and a marker on the door's answer (accepted/declined, candidates in, candidates out) is the first thing to
add.

## TD-S11 — RESOLVED, and it was my error, not the compiler's

Reproduced 2026-11-18, ten lines, deterministic:

```
dlopen libllama.so.0; goose_init(0,0); fg_sp_prefix(draft,4,target,4,4,out,2)  -> SIGSEGV
fg_sp_compatible(13 scalars)                                                  -> returns 1
```

The generated wrapper is `int64_t fg_sp_prefix(sl_i64, sl_i64, int64_t, sl_i64)` and calls
`fg_sp_prefix_g(data,len,data,len,k,data,len)`, so the shape of the call matches. The same algorithm
gated on its own compiles and passes (EXP-2026-11-18-1166), so the difference is the composed unit plus
the exported slice ABI.

Impact: every host-side call that passes slices through an exported door is affected - which is most of
them - and the symptom is silent in the server (the call does not return, the request still completes).

### Resolution

The exported wrapper takes **structs by value**:

```c
typedef struct { int64_t * data; int64_t len; } sl_i64;
int64_t fg_sp_prefix(sl_i64, sl_i64, int64_t, sl_i64);
```

My host code declared it as flattened `(data, len, data, len, k, out, len)` pairs. That compiles, and
crashes at entry, because the callee reads a struct layout the caller never produced. With the real
struct types the same call returns the right answer (`r=2` for a draft that agrees on two tokens), and in
the server the counter finally moves:

```
FG_SP_PREFIX goose calls=3 disagreements=0
```

So: **not a compiler defect**. The sampler shim always used real structs - which is why its 2965
differential cases passed - and my two hand-written hooks did not, which cost several iterations and one
false crash report.

The rule that follows, and it is the same shape as the build-source mistake: when calling a generated
export by hand, **copy the prototype from the generated header**, never re-type it as a list of pieces.
