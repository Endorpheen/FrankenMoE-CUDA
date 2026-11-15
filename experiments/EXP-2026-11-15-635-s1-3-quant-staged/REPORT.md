# Staged S1.3 — the Goose quant stack inside a CPU-only runtime copy

Date: 2026-11-15
Base commit of the repository: `b18249e0`
Work directory: `/tmp/opencode/sub-631/tree` (the repository was not modified)
Stage: `docs/goose-migration/RUNTIME-INTEGRATION.md`, S1 item 3
("the quantizer stack")

## 1. Goal and result

Goal: show the safe switching path for the whole P7-04 quant stack (seven Goose
modules) without touching production — one composed standalone unit, a build
flag that selects it for three native entry points, the native bodies still
compiled, and a differential gate that needs no model.

Result: done.

| check | result |
|---|---|
| base build (pristine llama.cpp, CPU only) | built |
| staged build, `-DFG_GOOSE_QUANT=OFF` | built |
| staged build, `-DFG_GOOSE_QUANT=ON` | built |
| gate, OFF | 46 cases, 0 failures, `row_calls=0`, `blck_calls=0` |
| gate, ON | 58 cases, 0 failures, `row_calls=24`, `blck_calls=7` |
| OFF vs ON per-case digests | 36/36 identical |
| base library vs OFF library | bit identical (`cmp` clean, same SHA-256) |
| Goose exports in `libggml-base.so` | 124 in ON, 0 in OFF |
| `qz_dequant` / `qz_quantize_row` / `qz_blck` definitions | 1 each in ON, 0 in OFF |
| deep arm gate (`fg-goose-quant-arms`) | 112 cases, 0 failures in ON; skipped in OFF |
| mutation run, 12 mutations, Goose object at `-O0` | caught **6/6** (half A) + **6/6** (half B) |
| gates after the mutation run restored the base | smoke 58/58 and 46/46, arms 112/112 |
| native ABI changed | no: same three signatures, no header consumer touched |
| model, server, GPU | not used at all |

## 2. Environment

```
gcc / g++   15.2.0 (Ubuntu 15.2.0-16ubuntu1)
cmake       4.2.3
goose       runtime 1e714e6146bc, toolchain goose-2026.3-string
kernel      7.0.0-43-generic
```

No GPU run, no `llama-server`, no model file, no commit.

## 3. What was added (all inside the staged copy)

| file | purpose |
|---|---|
| `scripts/compose-quant.py` | composes the seven canon modules into one Goose unit, deterministic collision policy |
| `scripts/stage-build-quant.sh` | the exact command sequence of section 4 |
| `goose/model/*.goose` | the seven canon files, byte identical to `b18249e0` |
| `goose/model-staged/` | the canon copy with the staged fix applied (built by the script, not tracked in canon) |
| `patches/quant-dequant-signed-zero.diff` | the staged fix (section 7) |
| `goose-gen/quant-stack.goose` | the composition (371,021 B) |
| `goose-gen/quant-stack.c/.h` | `goose --standalone` output (1,440,629 B / 4,871 B) |
| `common/fg-goose-quant-bridge.c/.h` | host primitives, `goose_init` latch, the three dispatch entries, the counters |
| `patches/ggml-quants.c.diff`, `patches/ggml.c.diff` | the complete native source diff (2 files, 93 lines added) |
| `tests/fg_goose_quant_smoke.c` | the seam gate (58 cases ON / 46 OFF) |
| `tests/fg_goose_quant_arms.c` | the arm gate behind the seam (112 cases, ON only) |
| `tests/quant_mutate.py` | the 12-mutation run, two halves of six |
| `CMakeLists.txt` | staged project: `FG_GOOSE_QUANT`, the Goose object, the bridge, the gate |
| `evidence/q-*.txt/.log` | raw logs, smoke output, `nm` dumps, disassembly, hashes |

The S1.1 stage (`FG_GOOSE_JSON_SCHEMA`, EXP626) is preserved unchanged in the
same file; the two flags are independent and were not enabled together.

What a commit must carry, and what it must not:

* carry: `goose/model/*.goose`, `scripts/compose-quant.py`,
  `scripts/stage-build-quant.sh`, `common/*.c/.h`, `tests/*.c`,
  `CMakeLists.txt`, `patches/*.diff`, `evidence/q-*`, this report;
* do not carry: `goose-gen/quant-stack.c` (1,440,629 B of generated C — the
  repository's `tools/check-generated-files.py` rejects a generated `.c` of
  1 MiB or more), `goose-gen/*.h`, `goose/model-staged/` (derived), and every
  `build-*/`, `*.o` and `*.so`. All are reproducible with the script.

## 4. Exact commands

```bash
cd /tmp/opencode/sub-631/tree
bash scripts/stage-build-quant.sh
```

which is:

```bash
# 0. canon -> one composed Goose unit -> one C unit
cp -a goose/model goose/model-staged
patch goose/model-staged/quant-dequant.goose < patches/quant-dequant-signed-zero.diff
python3 scripts/compose-quant.py goose/model-staged goose-gen/quant-stack.goose
goose check goose-gen/quant-stack.goose
goose --standalone -o goose-gen/quant-stack.c --header goose-gen/quant-stack.h \
      goose-gen/quant-stack.goose

# 1. base: pristine llama.cpp, CPU only, no flag
cmake -S llama.cpp -B build-base -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DGGML_CUDA=OFF -DGGML_NATIVE=ON -DLLAMA_CURL=OFF -DLLAMA_OPENSSL=OFF \
      -DLLAMA_SUBPROCESS=OFF -DLLAMA_BUILD_COMMON=ON -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF -DLLAMA_BUILD_TOOLS=OFF \
      -DLLAMA_BUILD_APP=OFF
cmake --build build-base -j 32 --target ggml

# 2. staged builds
for mode in OFF ON; do
    cmake -S . -B "build-${mode,,}" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DFG_GOOSE_QUANT="$mode"
    cmake --build "build-${mode,,}" -j 32 --target fg-goose-quant-smoke
done

# 3. gate and cross-build comparison
./build-off/fg-goose-quant-smoke
./build-on/fg-goose-quant-smoke
diff <(grep '^case=' off.txt) <(grep '^case=' on.txt)

# 4. symbols
nm build-on/bin/libggml-base.so  | grep -cE ' [TtWw] qz_dequant$'   # 1
nm build-off/bin/libggml-base.so | grep -cE ' qz_dequant$'          # 0

# 5. mutations: 12 one-site edits, Goose object at -O0, gates after every edit
python3 tests/quant_mutate.py        # -> evidence/q-mutations.txt
```

The mutation run configures its own tree (`build-mut`, `-DFG_GOOSE_QUANT=ON
-DFG_GOOSE_QUANT_O0=ON`), so the two gated builds above are not touched while a
mutant is being measured; the script rebuilds the base module and re-runs both
gates at the end.

The base build warns `Git repository not found` while generating
`build-info.cpp`; that is only the commit stamp of the copied tree.

## 5. How the flag is wired

Three native entry points, all inside `ggml-base` so that the library ABI and
every existing caller stay untouched. The Goose side asks first, the native
body answers every refusal in the same call.

`llama.cpp/ggml/src/ggml-quants.c`:

```c
void quantize_row_q8_0_ref(const float * x, block_q8_0 * y, int64_t k) {
    assert(k % QK8_0 == 0);
    const int nb = k / QK8_0;
#ifdef FG_GOOSE_QUANT
    if (fg_goose_quant_quantize_row_q8_0(x, y, k) == 0) {
        return;
    }
#endif
    ...the original body, unchanged...
}

void dequantize_row_q4_0(const block_q4_0 * x, float * y, int64_t k) {
    ...
#ifdef FG_GOOSE_QUANT
    if (fg_goose_quant_dequant_row_q4_0(x, y, k) == 0) {
        return;
    }
#endif
    ...the original body, unchanged...
}
```

`llama.cpp/ggml/src/ggml.c`:

```c
int64_t ggml_blck_size(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
#ifdef FG_GOOSE_QUANT
    const int gs_blck = fg_goose_quant_blck_size((int) type);
    if (gs_blck > 0) {
        return gs_blck;
    }
#endif
    return type_traits[type].blck_size;
}
```

`CMakeLists.txt`:

```cmake
if(FG_GOOSE_QUANT)
    add_library(fg-goose-quant OBJECT goose-gen/quant-stack.c)   # one object
    target_sources(ggml-base PRIVATE common/fg-goose-quant-bridge.c)
    target_compile_definitions(ggml-base PRIVATE FG_GOOSE_QUANT=1)
    target_include_directories(ggml-base PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
    target_link_libraries(ggml-base PRIVATE fg-goose-quant)
endif()
```

Dispatch is visible in the built library, not only in the counters
(`evidence/q-disasm-*.txt`):

```
<ggml_blck_size>:
    call   191d0 <fg_goose_quant_blck_size@plt>
    test   %eax,%eax
    jg     ...            ; module answered
    imul   $0x38,%rbx,%rbx  ; else the native type_traits path

<dequantize_row_q4_0>:
    call   19de0 <fg_goose_quant_dequant_row_q4_0@plt>
    test   %eax,%eax
    je     ...+0x200      ; module served the row, skip the native loop
```

The three module entries map to the canon exports with the same arguments:

| native entry | module call | agreement |
|---|---|---|
| `dequantize_row_q4_0(x,y,k)` | `qz_dequant(2, {x, k/32*18}, k, {y,k})` | returns `k`; else native |
| `quantize_row_q8_0_ref(x,y,k)` | `qz_quantize_row(8, x, y, k)` | returns `(k/32)*34`; else native |
| `ggml_blck_size(t)` | `qz_blck(t)` | `> 0` for the seven covered types; else `type_traits` |

## 6. Composition and `--standalone`

`goose/model` holds the seven canon files byte for byte (their SHA-256 values
are the ones `b18249e0` records; section 9). The composer concatenates them in
a fixed order and resolves the collisions the independent modules have:

* `main` dropped (a library unit has no entry point);
* 276 `const` declarations over 154 distinct names: the first definition is
  kept and the script fails if two declarations of one name differ (122 copies
  dropped, all identical values);
* 231 `fn` declarations over 199 distinct names: byte-identical duplicates keep
  the first body (32 copies dropped, comparison is textual, not assumed), and
  four groups need explicit handling:
  * module-internal helpers that exist in two variants (`scale_min_k4`,
    `e8m0_half`, `iq4_nl_block`, `qz_rows`) are renamed in the later module
    (definition and call sites together): `qd_*`, `qw_*`;
  * 26 host primitives carry the same module-visible name but bind to
    different C symbols (`qz_`/`qz2_`/`qd_` families), so the later modules get
    a suffix (`f32_bits_2`, `ld_f32_w`, `iq2_grid_d`, ...);
* any other duplicate is a hard error.

The result is deterministic: the same inputs produce the same 371,021 bytes and
the same SHA-256 on every run. `goose check` then reports 578 top-level
declarations, 402 specializations, and 70/269 index checks elided; the
generated C is one 1.44 MB translation unit and one 311,048 B object.

## 7. Finding: a real deviation in canon `quant-dequant.goose`

The gate found a byte-level difference that the accepted S5a gate did not
cover. Two defects, both about the sign of zero, both fixed by one staged
patch (`patches/quant-dequant-signed-zero.diff`):

1. `dq_half()` returned `0.0 - v` for a negative half. For `d = -0.0`
   (`0x8000`) that yields `+0.0`, while `GGML_FP16_TO_FP32` yields `-0.0`.
   Fix: `return -v;`, which negates the value and keeps the sign bit.
2. The q4_0 and q5_0 arms computed `x*d + m` with `m = 0.0f`, although those
   types have no min. When `x*d` is `-0.0`, adding `+0.0` turns it into `+0.0`;
   the native dequantizers compute `x*d` with no add. Fix: a branch that keeps
   the no-min types on `x*d`.

Evidence: with the 12 q4_0 dequant cases in the gate, `q4_0/zeros`,
`q4_0/rand_dneg2` failed on the missing-min defect and `q4_0/ramp_dnegzero` on
the `dq_half` defect — 3 of 34 cases red before the patch, all green after it.
A direct probe of case `q4_0/zeros` showed `00000000` from the module where the
native library returns `80000000`.

The canon files in `goose/model` are untouched; the patch is applied to
`goose/model-staged` only. Canon needs the same fix through its own experiment;
until then, a canon-based build that is gated on signed-zero inputs will fail.
Mutations A2 and A3 of section 8b re-introduce exactly these two defects and
are caught by the seam gate, so the fix is held in place by the mutation run and
not only by the happy-path corpus.

## 8. Gate

`tests/fg_goose_quant_smoke.c`, one binary per configuration, no model:

| group | ON | OFF | oracle |
|---|---|---|---|
| q4_0 dequant, 12 synthetic cases | 12/12 | 12/12 | local copy of the native body |
| q4_1/q5_0/q5_1/q8_0 dequant, 12 cases | 12/12 (module entry) | 12/12 | same |
| q8_0 quant, 12 patterns | 12/12 | 12/12 | local copy of the native body |
| `ggml_blck_size`, 10 types | 10/10 | 10/10 | expected values |
| **total** | **58/58** | **46/46** | |

```
evidence/q-smoke-on.txt
FGQ_SMOKE_RESULT mode=goose  cases=58 dequant=24 quant=12 failures=0 row_calls=24 blck_calls=7
evidence/q-smoke-off.txt
FGQ_SMOKE_RESULT mode=native cases=46 dequant=24 quant=12 failures=0 row_calls=0 blck_calls=0
```

Three separate statements, deliberately kept apart:

1. **The seam works.** ON dispatches 24 row entries and 7 block sizes into the
   module; OFF dispatches none, and the OFF library contains no Goose symbol at
   all, so it cannot.
2. **The results agree.** The 36 `case=` digests (24 dequant + 12 quant) are
   the same file in both builds (`evidence/q-dump-off.txt` vs
   `q-dump-on.txt`), which compares the Goose path against the native library
   across builds, not only against the in-binary oracle.
3. **The module arm agrees too.** Only `dequantize_row_q4_0` is rewired on the
   dequant side; the other four basic types go native in both builds. To keep
   them under the gate, the ON build calls `qz_dequant()` for those 12 cases
   directly (`module_case=` lines) and compares against the native body. That
   checks the module, not the seam — and it is what proved the q5_0 half of the
   signed-zero fix.

### 8a. The arm gate behind the seam

`tests/fg_goose_quant_arms.c` covers the entries the seam does not reach, so
that a mutation has somewhere to fail. It compares the module against the
native library, not against a copy of it:

| group | cases | oracle |
|---|---|---|
| chunk, unweighted, 18 types x 2 patterns | 36 | `ggml_quantize_chunk(..., NULL)` |
| chunk, two rows in (block boundary) | 2 | same, `start = n_per_row` |
| chunk, weighted, 16 types x 2 imatrix patterns | 32 | `ggml_quantize_chunk(..., imatrix)` |
| dequant, 21 types x 2 patterns | 42 | `dequantize_row_*` through the type traits |
| **total** | **112** | |

```
evidence/q-arms-on.txt
ARMS_RESULT mode=goose cases=112 failures=0
evidence/q-arms-off.txt
ARMS_RESULT mode=native cases=0 failures=0 skipped=1
```

Types are excluded from this gate for four measured reasons, and each exclusion
is a statement about the current canon, not about the seam:

* `q8_0` — the native entry `ggml_quantize_chunk` calls
  `quantize_row_q8_0_ref`, which this slice switches; the smoke gate covers
  q8_0 against a local copy of the native body instead.
* `iq2_xxs`, `iq2_xs`, `iq1_s` unweighted — the native entry asserts without an
  imatrix (`ggml_quantize_requires_imatrix`), so there is no oracle to compare
  to; their weighted arm does not match either (next item).
* `iq3_xxs`, `iq2_s`, `iq1_m` — canon diverges from native as soon as a 2-bit
  group falls off the IQ grid, which is the normal case for smooth input:
  `iq3_xxs` and `iq1_m` produce different bytes (first difference in the block
  scale) and `iq2_s` stops the process with `fail(1241)` in its final store.
  The native code takes the neighbour list at that point. This is the second
  canon finding of this stage; it was invisible to the accepted S3b/S4 gates
  because their corpora stayed on the grid.
* `q1_0`, `q2_0` weighted — canon has no weighted arm for these two types, and
  the native weighted entry does not accept them either.

### 8b. Mutation run

Twelve one-site edits of the staged Goose modules, two halves of six, the Goose
object compiled at `-O0` (`-DFG_GOOSE_QUANT_O0=ON`) while the rest of the tree
stays at Release. Every mutation is composed, checked with `goose check`,
regenerated with `goose --standalone`, rebuilt, and then both gates run. Only a
red gate counts as caught.

| id | mechanism | gate that caught it |
|---|---|---|
| A1 | quantization level: q4_0 scale divisor -8 -> -7 | arms (3) |
| A2 | sign of zero: q4_0 dequant adds the zero min again | smoke (3) |
| A3 | sign of zero: half decode returns `0.0 - v` | smoke (2) |
| A4 | min offset: q4_1 dequant halves the min | smoke (2) + arms (2) |
| A5 | tail rounding: q8_0 quantizer truncates instead of rounding | smoke (5) |
| A6 | block boundary: row stride uses `n_per_row`, not `row_size` | smoke (6) + arms (12) |
| B1 | super-scale: q3_K scale encoding -32 -> -31 | arms (2) |
| B2 | imatrix: q4_0 weighted arm ignores `quant_weights` | arms (1) |
| B3 | imatrix: q2_K `sigma2` factor 1 -> 2 | arms (2) |
| B4 | IQ neighbour search: iq4_nl trial count 7 -> 6 | arms (1) |
| B5 | quantization level: q2_K level count 3 -> 4 | arms (2) |
| B6 | dequant zero point: q6_K offset -32 -> -31 | arms (2) |

```
evidence/q-mutations.txt
half A: caught 6/6
half B: caught 6/6
after restore smoke: FGQ_SMOKE_RESULT mode=goose cases=58 ... failures=0 row_calls=24 blck_calls=7
after restore arms : ARMS_RESULT mode=goose cases=112 failures=0
```

Baseline before the run is the same 58/58 and 112/112, so the two halves are
measured against a green port, not against a broken one. The two gates are
complementary by construction: the seam gate catches everything that changes a
row the production entry points produce, the arm gate catches everything that
changes a type the seam does not reach yet.

The IQ neighbour search is covered through the IQ4 family (`iq4_nl`,
`iq4_xs`), the only IQ neighbour path whose canon arm matches native on this
corpus. The IQ2/IQ3-XXS/IQ1M neighbour path cannot be gated until the defect in
section 8a is fixed in canon.

No performance claim is made. `ggml_blck_size` is a hot helper; ON turns a
table lookup into a call plus an if-chain, which is exactly the kind of cost
that has to be measured on the production path and is not measured here.

## 9. Sizes and SHA-256

| artefact | bytes |
|---|---|
| canon `goose/model/*.goose` (seven files) | 361,616 |
| `goose-gen/quant-stack.goose` (composition) | 371,021 |
| `goose-gen/quant-stack.c` (generated) | 1,440,629 |
| `goose-gen/quant-stack.c.o` (Goose object) | 311,048 |
| `common/fg-goose-quant-bridge.c` | 10,057 |
| `libggml-base.so` OFF | 953,376 |
| `libggml-base.so` ON | 1,173,600 (+220,224, +23.1 %) |
| `fg-goose-quant-smoke` OFF / ON | 25,232 / 25,344 |
| `fg-goose-quant-arms` OFF / ON | 15,968 / 26,296 |

`libggml-base.so` OFF is byte-identical to the pristine base build
(`44f954d6…`, `cmp` clean): with the flag off the staged tree compiles to the
production library, so switching the feature off is a rebuild, not a revert.

Inputs, stage sources and artefacts (`evidence/q-sha256.txt`):

```
f9ca7bdbdc5a39f9d8f70d117e4ce8782f56f8294cfa0fd035ce781a00b0c767  goose/model/quant-types.goose
f1a86afc4099f029b73f4b85d0d94244ecc62ebddb22637e2049826cbba51eee  goose/model/quant-allow.goose
4362fc29d389c2be9b7337b2dad8cc8b1a4a6af6af1f964aa456ee35e19bffc6  goose/model/quant-rows.goose
0ddce390e5fa69875fb70655bd26ebf4521d054cc8244934d3feb796941fe0d9  goose/model/quant-rows2.goose
cea9b3457c1da0502ba041432be9d9d31dbbfca18bfa11f7b51d650f994b0011  goose/model/quant-dequant.goose
43a13e0bc34bca890875bed2da3b664c6866a335f23d03951e5cfff57f0eeb14  goose/model/quant-dequant2.goose
6d828de28ece9147998b4ea2f0044574896689fede1a2429d3f6fce4dadf5b0f  goose/model/quant-weighted.goose
```

The seven hashes are those of the files in `b18249e0`, and `quant-weighted.goose`
is `6d828de2…`, the value EXP616 recorded for its accepted source.

Stage files:

```
d3cbd7c28c19a0d9c8cc7871d143f607a6f130cef0aa1b91cea80fd1b3b1233c  goose-gen/quant-stack.goose
9ff97d4fb1cdf58ad489a04c85c3cb8ac8403132d9c57c7bbcc10f9eef3a2373  scripts/compose-quant.py
a0831d3ed15ea740dae0361fcb030535af9579a4f5198c72fa5c36b166417af0  common/fg-goose-quant-bridge.c
2328ed3931dd926095d9ff01b0812b4b358f2b4ba423f5e5bf7a4840b0074d29  common/fg-goose-quant-bridge.h
4026987356fbf81671281ad586f421ea6dd5af6eb0cfa5514ecb25ca5fedc33f  tests/fg_goose_quant_smoke.c
d5af38f0eaf8714f02b5d8eb27e96fc4ff753ec8416e032c860ec8ea00e09b72  tests/fg_goose_quant_arms.c
4a586c2471155165749801da5f2c2ce5d3db07de33b42a441d224008a3591af5  tests/quant_mutate.py
87db1d2beeb9475a0f384369ff51bf09a746a8c44ec20b637c59343a6954ece6  CMakeLists.txt
61db0218377ff6e26fd8825939203e44d806c5d871f0baa62b01cb45d7995e6e  patches/quant-dequant-signed-zero.diff
b244ed6241e7a6fe64a17a9f253d5d9b5b666609838944b441f0421ffe7f82ef  patches/ggml-quants.c.diff
9ebd529c7800d0b1abc156486cd61badae5b95c9aeebe3077e85fdbe03f6fe74  patches/ggml.c.diff
```

Artefacts:

```
44f954d6a281887c435f8622418066dbf395faa58a0d44db3248201fabb74568  build-off/bin/libggml-base.so.0.22.0
89f3675acd8d4f651298ce0b2eed24dcf88d7361cd627dc176ac4d5f9c208b84  build-on/bin/libggml-base.so.0.22.0
32958fe569a2f6292b00dd6032444cae8f29f386c5e8705ab5794140d1fb164c  build-off/fg-goose-quant-smoke
6d41e78825f304a36af9458c8159b54fa1f060c2fb05cea4b094545cb276a522  build-on/fg-goose-quant-smoke
9cb9599a903b952b21f4410b993720d709124edb501ab68db7a8ffc50e5e3158  build-off/fg-goose-quant-arms
fa3138f4a631f7b657ce0352937862014686cf099b91bed93ec801366082ac2d  build-on/fg-goose-quant-arms
63262e6a39b381610eaac77263d20a4be6921f0915e6d6b9208cc661d936f1fb  evidence/q-mutations.txt
bda8b548008a5ced7adac64499d693a283263735f3a35cb8263e9e5bce9f47fe  build-on/CMakeFiles/fg-goose-quant.dir/goose-gen/quant-stack.c.o
44f954d6a281887c435f8622418066dbf395faa58a0d44db3248201fabb74568  build-base/bin/libggml-base.so.0.22.0
```

`scripts/stage-build-quant.sh` rewrites `evidence/q-sha256.txt` on every run; a
clean rebuild from deleted build directories reproduced every value in it,
including the base and OFF libraries, byte for byte.

## 10. The main seam

The module now owns the arithmetic of P7-04 but not three things, and those are
the honest price of this slice.

1. **Host primitives, 55 of them.** The composed unit declares `extern`
   functions for what Goose has no primitive for: fp32/fp16 load and store,
   byte/half/u32 store, `fabsf/sqrtf/log2f/floorf/lroundf/ldexpf`, float ↔ bit
   reinterpretation, the native `nearest_int`, a 16 KiB scratch block, the IQ1
   sort, and the IQ grid/map/neighbour view. `common/fg-goose-quant-bridge.c`
   implements them exactly as the accepted P7-04 gates did
   (`experiments/EXP-2026-11-14-611/613/616/tests/fixture.cpp`), so the module
   sees the same arithmetic environment it was accepted on. This is the widest
   part of the seam: 55 C functions is a real surface to keep in sync, even
   though most are one-liners.
2. **The IQ tables.** `fg_goose_iq2_grid/iq3_grid` read the grid literals of
   `ggml-common.h`, and `fg_goose_iq2_map/iq2_nb/iq3_map/iq3_nb` read the
   neighbour tables that `ggml_quantize_init()` builds at run time. The
   accessors live in `ggml-quants.c` because those tables are file-static
   there. If the neighbour tables are missing the accessor aborts with a named
   message instead of returning a plausible wrong index — loud, because a silent
   wrong index would poison every IQ quantizer.
3. **The raw-pointer calling convention.** `qz_quantize_row`, `qz_quantize_chunk`
   and the weighted arm take buffer addresses as `u64`, not slices, so the
   bridge passes `(uint64_t)(uintptr_t)` and the module reads memory through the
   load/store primitives. `qz_dequant` does take slices, and the bridge builds
   them on the stack — no allocation on either path. The convention is the same
   one `goose/kv/iq4xs-encode.goose` and the P7-04 gates use; it works, but it
   means the compiler cannot check a buffer length across the seam.

Two smaller seams: `goose_init(0, NULL)` must run before the first export, done
once lazily with `pthread_once`; and the module's `f16` store goes through
`ggml_fp32_to_fp16`, which is the native primitive, not a Goose computation.

## 11. Boundaries — what this stage does not claim

1. **Only three entry points are rewired.** `dequantize_row_q4_0`,
   `quantize_row_q8_0_ref`, `ggml_blck_size`. The other 30+ row quantizers and
   dequantizers the module carries are linked but not reachable through the
   native API: `ggml_quantize_chunk`, the `_ref` row functions for the K/IQ
   types, `dequantize_row_q4_1/q5_0/q5_1/q8_0` and the rest still run native.
2. **The imatrix arm has no dispatch point.** `qw_quantize_chunk_im` exists in
   the ON library and is compared bit for bit by the arm gate on 16 types, but
   no native entry dispatches to it yet.
3. **IQ/IQ1 quantize arms need `ggml_quantize_init`.** They are linked; the map
   and neighbour accessors abort if the tables were never built. The arms the
   canon handles correctly are gated through the arm gate; the ones that do not
   match native are excluded and named in section 8a.
4. **fp16/fp32 conversion is native.** `dq_half` decodes half in Goose, but the
   store path uses `ggml_fp32_to_fp16`; a build that replaced the fp16
   primitive would need its own gate.
5. **No CUDA, no GPU kernel, no model.** Nothing here says anything about the
   GPU side, about model-level output parity, or about throughput. The S1
   acceptance of "identical model output" is untouched.
6. **Hot-path cost.** `ggml_blck_size` is called from graph building; ON adds a
   PLT call and an if-chain there. Unmeasured, and it must be measured before
   any production decision.
7. **Two-place maintenance.** The composition and the 55 primitives are new
   surface that canon does not have. Every canon change to these seven modules
   must be re-composed and re-gated; the script is the only supported path.
8. **Gate and mutation optimisation levels.** The repository expects `-O1` for
   gates and `-O0` for mutations. The gates here link a Release library, so the
   generated Goose object and the gates run at `-O2`; the mutation run
   recompiles the Goose object with `-O0` (`-DFG_GOOSE_QUANT_O0=ON`) while the
   tree stays at Release. Twelve mutations, two halves of six, 6/6 + 6/6 caught
   (section 8b); they cover this seam's mechanisms, not the whole canon — the
   module's own 12-mutation work in EXP611/616 remains its acceptance basis.
9. **The staged patch is not canon.** Section 7's fix lives in a patch file.
   Until canon takes it, a canon build is not byte-identical to native on
   signed-zero inputs.

## 12. What is still open

1. **Canon needs the signed-zero fix** (section 7) through its own experiment,
   with the signed-zero cases added to the S5a/S5b corpora — otherwise the
   defect survives in every future composition.
2. **Canon needs the IQ off-grid path** (section 8a): `iq2_s` stops with
   `fail(1241)` where native walks the neighbour list, and `iq3_xxs`/`iq1_m`
   produce different bytes once a group leaves the grid. Until that is fixed,
   these three types cannot be quantized from Goose on arbitrary input, and
   none of them can be gated here.
3. **Widen the seam.** `ggml_quantize_chunk` (the real quantizer entry) and the
   K/IQ dequantizers are the entries that matter for a model path; each needs
   its own dispatch, and the arm gate of section 8a is the gate it would start
   from.
4. **Give the weighted arm a dispatch point** (`qw_quantize_chunk_im`), now
   that the arm gate compares it on 16 types.
5. **Shrink the primitive surface.** The 55 externs could be replaced by a
   handful of slice-taking builtins; that is a canon decision, not a staged one.
6. **Measure the hot-path cost** of `ggml_blck_size` on a graph build before
   proposing ON as a default anywhere.
7. **Model-level parity** for the quantizer still needs the GPU runtime and
   Igor's explicit approval; nothing in this stage moves toward it.

## 13. Raw evidence

```
evidence/q-compose.txt            composer output, input hashes
evidence/q-patch.txt              the staged fix applied
evidence/q-goose-check.txt        goose check of the composition
evidence/q-goose-standalone.txt   goose --standalone output
evidence/q-base-configure.log     configure of the pristine llama.cpp
evidence/q-base-build.log         base build
evidence/q-off-configure.log      staged configure, flag OFF
evidence/q-off-build.log          staged build, flag OFF
evidence/q-on-configure.log       staged configure, flag ON
evidence/q-on-build.log           staged build, flag ON
evidence/q-smoke-off.txt          the 46 OFF cases and the result line
evidence/q-smoke-on.txt           the 58 ON cases and the result line
evidence/q-arms-off.txt           the arm gate on OFF (skipped, by design)
evidence/q-arms-on.txt            the 112 arm cases and the result line
evidence/q-mutations.txt          the 12 mutations, halves, and the restore run
evidence/q-dump-off.txt           per-case digests, OFF
evidence/q-dump-on.txt            per-case digests, ON (identical)
evidence/q-nm-on.txt              124 goose exports in the ON library
evidence/q-nm-off.txt             the same query on the OFF library (empty)
evidence/q-nm-counts.txt          per-symbol ON=1 / OFF=0
evidence/q-base-vs-off.txt        base and OFF libraries bit identical
evidence/q-disasm-blck.txt        ggml_blck_size calling the module first
evidence/q-disasm-dequant.txt     dequantize_row_q4_0 calling the module first
evidence/q-disasm-quant.txt       quantize_row_q8_0_ref calling the module first
evidence/q-sha256.txt             every input and artefact hash
```
