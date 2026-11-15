# Staged S1.5 — the Goose tokenizer decisions inside a CPU-only runtime copy

Date: 2026-11-15
Repository base commit: `620e3aa1` (the CPU-only copy comes from the S1.1 stage,
`98eaa70e`; `git diff 98eaa70e 620e3aa1 -- upstream/llama.cpp` is empty, so the
copy is the base commit's tree)
Work directory: `/tmp/opencode/sub-639/tree` (the repository was not modified)
Stage: `docs/goose-migration/RUNTIME-INTEGRATION.md`, S1 — the tokenizer slice
("vocab type and pre-tokenizer dispatch, special-token partition, common string
helpers"), following the S1.1 pattern (`FG_GOOSE_JSON_SCHEMA`)

## 1. Goal and result

Goal: put the accepted tokenizer decisions behind one build flag, keep the
native bodies compiled, and prove with a differential gate that the switched
path produces the native result — no model, no weights, no gguf file.

Result: three decisions switched, OFF and ON both built, 182 cases per build,
zero divergences, one recorded revision difference pinned in the bridge.

| check | result |
|---|---|
| composed Goose unit (`--standalone`) | built, 12 module exports |
| staged build, `-DFG_GOOSE_TOKENIZER=OFF` | built, native bodies selected |
| staged build, `-DFG_GOOSE_TOKENIZER=ON` | built, Goose selected |
| native bodies vs pristine sources | `PARITY PASS 3/3` |
| gate, OFF | `vt=146/146 part=20/20 strip=16/16 cases=182/182 failures=0 goose_calls=0 pins=0` |
| gate, ON | `vt=146/146 part=20/20 strip=16/16 cases=182/182 failures=0 goose_calls=182 pins=2` |
| definitions of `vt_dispatch` / `stp_partition` / `cs_strip` | 1 each in ON `libllama.so`, 0 in OFF |
| exported module names in the dynamic table | 0 in both builds (the object is hidden) |
| exported surface of `libllama-common.so` | identical to base in OFF and ON (0 removed, 0 added) |
| public C ABI (`llama.h`) | untouched: same names, same signatures |
| model, server, GPU | not used at all |

`goose_calls=182` on ON is the witness counter inside the bridge: one increment
per dispatch, 182 dispatches for 182 cases. ON is therefore the path that ran.
`pins=2` is the revision pin described in section 6 (two of the 182 cases have
`tokenizer_pre = "glm4"`).

## 2. Environment

```
gcc / g++   15.2.0 (Ubuntu 15.2.0-16ubuntu1)
cmake       4.2.3
goose       runtime 1e714e6146bc, toolchain goose-2026.3-string
kernel      7.0.0-43-generic, 32 cores
```

No GPU run, no `llama-server`, no model file, no commit, no write outside the
staged copy. A full clean run of `scripts/stage-build.sh` (compose, parity, both
builds, both gates, symbol check, hashes) takes 2 min 33 s at `-j 32`; the gate
itself runs in under 10 ms per build.

## 3. What was composed

The three accepted modules are concatenated into one goose unit and compiled
with `--standalone` into one C translation unit:

| source | accepted hash (EXP-622) | this copy |
|---|---|---|
| `goose/vocab-type.goose` | `f744f1a7…` | identical |
| `goose/special-partition.goose` | `154cfb0f…` | identical |
| `goose/common-strings.goose` | `f6647d36…` | identical |

The goose CLI takes a single input file, so composition is textual
(`scripts/compose-goose.sh`): the three bodies are concatenated with a banner
that names each part. The prefixes are disjoint (`vt_`, `stp_`, `cs_`), there is
no top-level state, and the generated header shows 12 module exports:

```
vt_dispatch, stp_partition,
cs_strip, cs_split, cs_join, cs_replace_all, cs_repeat,
cs_from_bool, cs_from_ints, cs_lcs, cs_process_escapes, cs_kv
```

Only three of them are switched in this stage (section 5); the rest travel along
because composition is one unit per flag, not one unit per function.

One dependency of the composed unit is not part of the runtime: `cs_kv` calls
`gs_json_strtod`, the json package's native strtod. A standalone unit declares
it and does not define it (`EXP-622` supplied it from its fixture too). The
bridge defines it for the library — `strtod` over the slice, current locale —
so `libllama.so` resolves it without a package response file.

## 4. Exact commands

```bash
cd /tmp/opencode/sub-639/tree

# 0. three goose sources -> one composed unit -> one C translation unit
./scripts/compose-goose.sh
# parsed ok: 35 top-level declarations
# typechecked ok: 49 specialization(s), -O2: 142 inlined, 60 folded
# wrote goose-gen/tokenizer-decisions.c (557852 bytes)
# wrote goose-gen/tokenizer-decisions.h (2265 bytes)

# 1. the native bodies are the pristine ones, moved not rewritten
python3 tests/check-native-parity.py reference .
# PARITY ok    src/llama-vocab.cpp: determine vocab type (481 native lines)
# PARITY ok    src/llama-vocab.cpp: tokenizer_st_partition (115 native lines)
# PARITY ok    common/common.cpp: string_strip (9 native lines)
# PARITY PASS 3/3

# 2. staged build, flag OFF
cmake -S . -B build-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_TOKENIZER=OFF
cmake --build build-off -j 32 --target llama llama-common

# 3. staged build, flag ON
cmake -S . -B build-on -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_TOKENIZER=ON
cmake --build build-on -j 32 --target llama llama-common

# 4. gate: both oracles are cut out of the sources on every run
./scripts/stage-gate.sh
# FG639_SMOKE mode=native vt=146/146 part=20/20 strip=16/16 cases=182/182 failures=0 goose_calls=0 pins=0
# FG639_SMOKE mode=goose  vt=146/146 part=20/20 strip=16/16 cases=182/182 failures=0 goose_calls=182 pins=2

# 5. symbols and ABI
./scripts/check-symbols.sh
```

`scripts/stage-build.sh` runs the whole sequence and writes `evidence/s1.5/`.
`scripts/apply-tokenizer-hooks.py` is the script that edited the two llama.cpp
files, and `patches/` holds the resulting diffs.

## 5. How the flag is wired

Two llama.cpp files are edited and one new file is added; the complete diff is
in `patches/llama-vocab.diff` and `patches/common.diff`. The flag touches no
header that `llama.h` exposes.

`src/llama-vocab.cpp` — the native block keeps its text:

```cpp
bool llama_vocab::impl::determine_vocab_type_native(llama_model_loader & ml, const LLM_KV & kv) {
    ... the 481-line "determine vocab type" block, byte for byte ...
}   // returns true for a model without vocab, i.e. the native early return

bool llama_vocab::impl::determine_vocab_type(llama_model_loader & ml, const LLM_KV & kv) {
#ifdef FG_GOOSE_TOKENIZER
    return determine_vocab_type_goose(ml, kv);
#else
    return determine_vocab_type_native(ml, kv);
#endif
}
```

`load()` calls `determine_vocab_type(ml, kv)` instead of holding the block, and
returns when it reports "no vocab". The partition follows the same shape:
`tokenizer_st_partition_native()` holds the native body and
`tokenizer_st_partition()` dispatches. `common/common.cpp` does the same with
`string_strip_native()` / `string_strip()`.

`llama.cpp/src/fg-goose-tokenizer-bridge.{h,cpp}` is compiled only under the
flag. It owns:

1. **The gguf answers.** The module never calls gguf. The host reads
   `tokenizer.ggml.model`, `.pre`, `.token_type_count`, the presence and the two
   gguf types of `merges`, the presence, array type, element type and length of
   `precompiled_charsmap`, and the 4-byte `xcda_blob_size` header (byte-swapped
   on big endian). It hands them over as plain integers.
2. **The name → enum table.** `vt_dispatch` returns names, never numbers
   (`NONE SPM BPE WPM UGM RWKV PLAMO2`; 57 pre-tokenizer names). The bridge maps
   them onto this revision's enums. This table is the single place a native
   renumbering would need attention.
3. **The refusal text.** Statuses 1–6 are thrown as `std::runtime_error` with
   the native text: `unknown tokenizer: '<model>'`, `unknown pre-tokenizer type:
   '<pre>'`, `cannot find tokenizer merges in model file\n` (the module writes
   the sentence without the newline; the bridge restores it),
   `invalid gguf type for <key>`, `precompiled charsmap too small for
   xcda_blob_size header!`, `Index out of array bounds in precompiled
   charsmap!`.
4. **The tree-version guards.** Names the canon module knows and this revision
   does not are refused with the native text instead of being accepted:
   * models `test`, `plamo3` → `unknown tokenizer: '<model>'` (this tree has
     neither `LLAMA_VOCAB_TYPE_TEST` nor `LLAMA_VOCAB_TYPE_PLAMO3`);
   * pre-tokenizers `flint`, `glm5`, `hy_v4`, `k2-horizon`, `mmbert`,
     `spark2_5`, `ufakzeka` → `unknown pre-tokenizer type: '<pre>'` (none of
     them has a `LLAMA_VOCAB_PRE_TYPE_*` in this tree).

`CMakeLists.txt` (staged project):

```cmake
if(FG_GOOSE_TOKENIZER)
    add_library(fg-goose-tokenizer OBJECT goose-gen/tokenizer-decisions.c)
    set_target_properties(fg-goose-tokenizer PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
    target_compile_options(fg-goose-tokenizer PRIVATE -w -fvisibility=hidden)

    target_sources(llama PRIVATE llama.cpp/src/fg-goose-tokenizer-bridge.cpp)
    target_include_directories(llama PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_compile_definitions(llama PRIVATE FG_GOOSE_TOKENIZER=1)
    target_link_libraries(llama PRIVATE fg-goose-tokenizer)

    target_compile_definitions(llama-common PRIVATE FG_GOOSE_TOKENIZER=1)
endif()
```

Everything lands in `libllama.so`: one object, one definition per export, and
`-fvisibility=hidden` keeps the module's names out of the dynamic symbol table.
`libllama-common.so` reaches the strip entry through the bridge, which it already
links against.

Properties that matter for the production rule ("a new feature's flag exists
only to disable it"):

- OFF is the default and selects the native bodies; no Goose object is compiled
  or linked, and `nm` finds no module name in the library;
- the native bodies are compiled in both configurations, so switching the flag
  off is a rebuild, not a revert;
- no call site disappears: each switched function sits beside its native body.

## 6. The gate

`tests/make-native.py` cuts the three native bodies out of a source tree and
wraps them with the smallest shims a harness needs: the gguf readers answer from
the case, the fragment buffer is the native class, the enum names are parsed
from the headers. Nothing is hand-transcribed, and the shims decide nothing.
`tests/gen-cases.py` extracts every model name and every pre-tokenizer name the
staged tree compares against, so the sweep cannot miss a name.

| mode | S1 vocab type (146) | S2 partition (20) | S3 strip (16) |
|---|---|---|---|
| OFF | extraction of the staged file vs extraction of the pristine file | same | library `string_strip` (native body) vs pristine extraction |
| ON | bridge (the function the loader calls) vs staged extraction | same | library `string_strip` (Goose body) vs pristine extraction |

- The 146 vocab cases are 48 hand-written combinations (every model family, the
  merges and charsmap error classes, the tree gaps) plus an 11-model sweep and
  an 87-pre-tokenizer sweep.
- The 20 partition cases cover adjacency, both kinds of strip, the
  `parse_special` skip rules, longest-first ordering, overlap and UTF-8 text.
- The 16 strip cases cover the empty string, whitespace-only, C-locale
  whitespace, interior runs and non-ASCII bytes.
- A refusal is compared on status and message only: the loader throws, so no
  caller ever reads the remaining fields of the record.
- OFF proves the moved bodies behave exactly like the pristine ones;
  `check-native-parity.py` proves the same thing at the byte level (3/3, with
  three recorded normalizations: one dedent, `return;` → `return true;`,
  `__func__` → `"load"`).
- ON proves the bridge produces the native record, 182 of 182, and that the
  path really ran (`goose_calls=182`).

**One real divergence was found and is pinned.** The canon module is one
upstream revision ahead of this tree: for `tokenizer_pre = "glm4"` (and `glm5`)
it sets `ignore_merges = true` (llama.cpp-flint-cuda at `1b44e307`,
`src/llama-vocab.cpp:2555-2560`), while this revision groups `glm4` with `chatglm-bpe` and leaves
`ignore_merges` false (`src/llama-vocab.cpp:2276`). Switching the module on must
not change this tree's tokenization, so the bridge keeps the tree's value and
counts the event:

```cpp
// Revision pins: decisions where the canon module is one upstream revision
// ahead of this llama.cpp. ...
if (pre == "glm4" || pre == "glm5") {
    out.ignore_merges = 0;
    g_pins.fetch_add(1, std::memory_order_relaxed);
}
```

The gate prints `pins=` so the difference cannot be forgotten: enabling the
module today keeps this revision's behaviour, and moving the runtime to the
canon revision is the step that removes the pin.

## 7. The seam this stage cannot remove

The module decides *what* the tokenizer configuration is; the host still owns
every byte array and every value that comes out of one. This is the honest price
of the slice, and it is where the next stage has to push.

1. **Token arrays.** `stp_partition` takes the token text table, the attribute
   table and the loader order as caller-owned arrays. The ON path copies the
   token text (one `std::vector<char>` of the whole vocab) and three `int64`
   arrays per call, then rebuilds the fragment list. That is a per-tokenize
   allocation of a few hundred kilobytes for a 100k-token vocab — correct, but
   it must be pooled (or owned by the module) before a request path.
2. **Merges.** The module counts the merge entries and classifies the key; the
   text-to-rank map is still built by the loader (`bpe_ranks.emplace` per
   entry), because the module has no map type. The ON path repeats the native
   loop for it, and that loop is *not* exercised by this gate (it needs a gguf
   file).
3. **Charsmap.** The module authenticates the key (array type, element type,
   length, size header); the blob itself is still copied into
   `precompiled_charsmap` by the host, and the big-endian swap of the array body
   stays native.
4. **Fragment budget.** The gate's bound is `4*(len+2) + 16*order + 16`
   fragments; the ON path allocates four `int64` vectors of that size per raw
   fragment (32 B per slot). Bounds checks inside the module are elided for the
   destination slices it writes (`bce: elided 57/156 index checks`), so the
   caller must size them right — a wrong `cap` is memory corruption, not a
   refusal.
5. **The loader path is not exercised.** Everything below `llama_vocab::impl::load`
   needs a gguf file: the merges ingest, the charsmap copy, the dummy-token
   branch, and the fragment rebuild inside `tokenize()`. This stage proves the
   decisions and the record; it does not prove a loader run end to end.
6. **The revision pin.** One decision is overridden by the tree's revision
   (section 6). It is visible in the bridge, in `pins=`, and here.
7. **Whitespace set.** `isspace` above byte 127 is native UB; the module uses
   the C-locale ASCII set, so strip stays on ASCII (as EXP-622 recorded).
8. **Empty special-token text.** The native loop cannot advance past it and
   spins; the ON path throws `tokenizer_st_partition: empty special token text`
   instead. The loader refuses such a vocab upstream, so this is a guard, not a
   behaviour change — but it is a difference, and it is deliberate.
9. **`gs_json_strtod`.** The composed unit needs the json package's native
   strtod because `cs_kv` is in the same unit. The bridge supplies it; a future
   composition that drops `cs_kv` would not need it.
10. **Model-level parity.** Not attempted: no server, no model, no GPU, so no
    claim about identical model output.

## 8. Sizes

| artefact | bytes |
|---|---|
| `goose/vocab-type.goose` (input) | 19,258 |
| `goose/special-partition.goose` (input) | 6,982 |
| `goose/common-strings.goose` (input) | 12,567 |
| `goose-gen/tokenizer-decisions.goose` (composed) | 39,084 |
| `goose-gen/tokenizer-decisions.c` (generated) | 557,852 |
| `goose-gen/tokenizer-decisions.h` (generated) | 2,265 |
| `tokenizer-decisions.c.o` (with `-fvisibility=hidden`) | 73,296 |
| `llama.cpp/src/fg-goose-tokenizer-bridge.cpp` (new) | 12,533 |
| `tests/fg_goose_tokenizer_smoke.cpp` (new) | 19,584 |
| `libllama.so` OFF → ON | 4,705,816 → 4,785,904 (+80,088, +1.7 %) |
| `libllama-common.so` OFF → ON | 5,962,736 → 5,962,824 (+88) |
| `fg-goose-tokenizer-smoke` OFF / ON | 140,736 / 145,408 |

The library growth is the Goose object plus the bridge. The native bodies are
still present in the ON libraries (`determine_vocab_type_native`,
`tokenizer_st_partition_native`, `string_strip_native`).

## 9. SHA-256

Inputs and stage files:

```
f744f1a7a31ffa52c62024388e28363858b9a2a47d7a5c09bef47b0637e8a5d2  goose/vocab-type.goose
154cfb0f910a66c147c74ba52f1277d60d43fa6c6cb21438240c9925b6a6e682  goose/special-partition.goose
f6647d3636090a1ded775976945f9f6b2b7b8ca4c5f3da57a69e2f2e89026c5b  goose/common-strings.goose
fb83b39cf7ca3148dc07f3642ac614baa2af801279777fdcd7dd23cf54301e1a  goose-gen/tokenizer-decisions.goose
e6d71ff8c56d942e59d4e35be85d7a33f87dc4cd5dbeda23b79e7d4ba5eb91a1  goose-gen/tokenizer-decisions.c
bb5fdfef52e40448cbb91ad2336f5ba792b23a298268138f0238f5d71dfa7770  goose-gen/tokenizer-decisions.h
bdd2675e63b3c7b65c219413293e95ac766015e570bad4d6736e69ab5cb948f0  llama.cpp/src/fg-goose-tokenizer-bridge.cpp
d3cbdf5af92f1a7343d4ad97b827888c90512329736bdd7d92623017c11e601b  llama.cpp/src/fg-goose-tokenizer-bridge.h
20f7152fb024067bae062b8dc5d8d72c4be34a6dec10306304a283653507f1a9  llama.cpp/src/llama-vocab.cpp
df914d2322d12b0bf1efc5474f5f46fcf5c77ac9ea17a802c214fb7f26d4b066  llama.cpp/common/common.cpp
c9f28ac69439b2297a2cb931c695289bfbd8dbeab142ba3d368b1ab059feb2d9  tests/fg_goose_tokenizer_smoke.cpp
3b1b9991d3372c2afa45cc74156c7a880bdb27f063877e0ac13cad9ac687af63  tests/make-native.py
dbd9c0079d1737cdd5b6b7b7aeb878073057d2c2b3eeeac2c80fcf32a72a707f  tests/gen-cases.py
5877bab8a853fca573ebf1259935a18482b217a4f15d7510dc5b75c550511522  tests/check-native-parity.py
c34978959ce2e8d15d1616c95a4b29299527a5631a901f9536bf802e58f63cec  CMakeLists.txt
8d43f6104de9225ac28accbf80c37fe624cff173f74cb8e5edf3d3751808208c  scripts/compose-goose.sh
98df5e89dba6631e1896e06969b2632dfa3284178ed55ad3971ea161bffd281a  scripts/apply-tokenizer-hooks.py
68a23290c9b8209fd2883685657a008e4432d3d4911d2f0a43dacb4466275453  scripts/stage-build.sh
7d7e360d2de2c5a90d46d3b6670db51bb43ef22da788bc25ce98d30bf8754983  scripts/stage-gate.sh
dce5cb2a8039a7acbf11fac5d9e8c7b2818c9b6f19e06e05a652a0c7e16757cc  scripts/check-symbols.sh
```

Pristine reference copies the parity check compares against:

```
32b8b7ac4a023bc6367eb7832e56e988ab7373a676ddcca4977665f78932ab88  reference/llama-vocab.cpp.orig
9d0819ee984f36cab9c35eb0fb374c9e73bfd0af0fe6a5b912cdf1ccce7d51a3  reference/common.cpp.orig
864acfeaa8b14c871bc31f9c94e50e26b3a1274219bbb9b99c94263be63af91c  reference/llama-vocab.h.orig
```

Artefacts:

```
0be7c3e4eb44d3607b9d2cf463a344082da02bd4c2f87bd79458a56ec690fdd1  build-off/bin/libllama.so.0.3.0
e6177d16ca289032bba64b104c11dc15bdeb338b37d5dc18fc09c7ddaecd5953  build-off/bin/libllama-common.so.0.3.0
4af19ebf4d4a2862fb555e7c5ec186835248b9fc2c9706328063279804083670  build-on/bin/libllama.so.0.3.0
aaef4e0c768391af4bc1fb363082c43726c4c475c3e3b46b5a891e61cdf3a827  build-on/bin/libllama-common.so.0.3.0
5e09b88e1da028f575e31ea2e73d1c7e5c24546d419e0f0b35f2cf69a1cd33cf  build-on/CMakeFiles/fg-goose-tokenizer.dir/goose-gen/tokenizer-decisions.c.o
432c4f73ecdf35648a47d1b686b3288f1116210b2f7c2864dad7b072286ff329  build-gate/fg-goose-tokenizer-smoke-off
9b94c6d2b967cf08f7de27650cfa2b3ee0c8a9cd2edf66f75ed19a2f170b7bf1  build-gate/fg-goose-tokenizer-smoke-on
```

The three goose sources hash to the values EXP-622 recorded for the accepted
sources, so this stage builds the accepted files and not a mutation of them.

## 10. Raw evidence

```
evidence/s1.5/compose.log          --standalone run, sizes, module export count
evidence/s1.5/native-parity.txt    PARITY PASS 3/3
evidence/s1.5/off-configure.log    staged configure, FG_GOOSE_TOKENIZER=OFF
evidence/s1.5/off-build.log        staged build, OFF
evidence/s1.5/on-configure.log     staged configure, FG_GOOSE_TOKENIZER=ON
evidence/s1.5/on-build.log         staged build, ON
evidence/s1.5/gate.log             both oracle extractions, both gates
evidence/s1.5/cases-off.txt        one line per case, OFF (182 records)
evidence/s1.5/cases-on.txt         one line per case, ON (182 records)
evidence/s1.5/symbols.txt          definitions, exports, ABI surface diffs
evidence/s1.5/sha-sources.txt      sources above
evidence/s1.5/sha-artifacts.txt    libraries, object, gate binaries
evidence/s1.1/                     the S1.1 stage (json-schema converter) logs
build-base/                        the pristine base build of the S1.1 stage; the
                                   ABI reference of step 5 (same upstream source,
                                   no stage edit). `check-symbols.sh` skips the
                                   surface diff if it is absent.
```

`patches/llama-vocab.diff` (4 hunks: include, member declarations, the moved
block plus the Goose path, the partition dispatcher) and `patches/common.diff`
(3 hunks) are the complete source change; the bridge and the gate are new files
listed above.

## 11. What is still open

1. **Pooled buffers.** The partition path allocates per call (section 7.1,
   7.4). Needs an arena owned by `llama_vocab::impl` before a request path.
2. **Merges in the module.** The rank map is the last big native structure in
   this family; a goose map type or a rank-table export would close it.
3. **A loader-level gate.** A synthetic gguf with zero tensors would let the
   real `llama_vocab::impl::load` and `tokenize()` run under both flags. That is
   the next step for this slice and the only way to cover section 7.2/7.3.
4. **The revision pin.** `glm4`/`glm5` `ignore_merges` must move from the bridge
   to the tree when the runtime follows the canon revision.
5. **The remaining `cs_*` family.** `cs_split`, `cs_join`, `cs_replace_all`,
   `cs_repeat`, `cs_from_*`, `cs_lcs`, `cs_process_escapes`, `cs_kv` are linked
   but not switched. `cs_kv` in particular owns the CLI override parse and needs
   its own differential gate (EXP-622 has the case corpus).
6. **`tokenizer_st_partition` on multi-fragment buffers.** The ON path applies
   the module per raw fragment; today `tokenize()` always starts from a single
   fragment, so the general case is implemented but unmeasured.
7. **Model-level parity.** Untouched: no server, no model, no GPU.
