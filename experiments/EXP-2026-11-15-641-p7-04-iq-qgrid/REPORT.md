# EXP-2026-11-15-636 — the IQ quantizers read the wrong grid (P7-04)

Date: 2026-11-15
Base commit: `4930d479` (canon already carries the signed-zero fix of EXP633)
Experiment root: `/tmp/opencode/sub-640/experiments/EXP-2026-11-15-636-p7-04-iq-offgrid`
Native oracle: `../../tree/llama.cpp` (CPU-only, same commit; `upstream/llama.cpp`
did not change between `b18249e0` and `4930d479`)

## 1. Verdict

ACCEPTED as a fix for one mechanism: the port's IQ quantizers must read the
grid that `ggml_quantize_init()` builds, not the static grid literals the
dequantizers read. Fourteen functions in two canon files, 28 changed lines, no
change to the arithmetic itself.

| state | gate result |
|---|---|
| canon `4930d479` | `iq2_s`: segmentation fault; `iq3_xxs`: 2 byte mismatches then `fail(1221)`; whole gate: crash |
| canon + `patches/canon-iq-qgrid.diff` | **40/40 cases, 0 mismatches**, 122,245 measured off-grid lookups |
| mutations of the fixed logic | **6/6 + 6/6** |

`iq1_m` carries a second, independent defect and is **not** fixed here; see
section 6.

## 2. The defect

### 2.1 Two tables, one list of numbers

`ggml-common.h` carries six hand-written grid tables. The dequantizers index
them directly:

```
ggml-quants.c:2503  const uint8_t * grid = (const uint8_t *)(iq2xxs_grid + aux8[l]);
ggml-quants.c:2595+ dequantize_row_iq3_xxs: (const uint8_t *)(iq3xxs_grid + ...)
ggml-quants.c:2684  dequantize_row_iq1_m:  (const int8_t  *)(iq1s_grid + ...)
```

The quantizers never touch those tables. They read the tables built at start-up:

```
ggml-quants.c:3298  const uint64_t * kgrid_q2xs = iq2_data[gindex].grid;   // iq2_xxs
ggml-quants.c:3476  const uint64_t * kgrid_q2xs = iq2_data[gindex].grid;   // iq2_xs
ggml-quants.c:4520  const uint64_t * kgrid_q2xs = iq2_data[gindex].grid;   // iq2_s
ggml-quants.c:4702  const uint64_t * kgrid_q2xs = iq2_data[gindex].grid;   // iq1_m
ggml-quants.c:4183  const uint32_t * kgrid_q3xs = iq3_data[gindex].grid;   // iq3_xxs
```

and those tables hold a different encoding. `iq2xs_init_impl` writes each grid
point as one level code per element (`ggml-quants.c:3124`, `pos[i] = 2*l + 1`),
so the quantizers decode a byte as `(pg[i] - 1)/2` — the level itself. The
literals in `ggml-common.h` are stored pre-scaled: `iq3xxs_grid[0]` is
`0x04040404` and `iq2xxs_grid[0]` is `0x0808080808080808`, while the generated
tables start at `0x01010101` and `0x0101010101010101`. The dequantizers are
written for the scaled form; the quantizers are written for the level form.

A direct comparison (`results/tables.txt`, probe in `probe/tables.c`):

```
iq2_xxs  n= 256 differing= 256   static[0]=0808...  generated[0]=0101...
iq2_xs   n= 512 differing= 512   static[0]=0808...  generated[0]=0101...
iq1_s    n=2048 differing=2048   static[0]=ffff...  generated[0]=0101...
iq2_s    n=1024 differing=1024   static[0]=0808...  generated[0]=0101...
iq3_xxs  n= 256 differing= 256   static[0]=04040404 generated[0]=01010101
iq3_s    n= 512 differing=   0   (the only pair that happens to agree)
```

Every table except `iq3_s` differs in every entry. `iq3_s` agreeing is exactly
why the old gates could not see the problem: the one type that was gated with
the generated tables (EXP611/616 used tables extracted from the init code)
passed, and the type whose table does not agree (`iq3_xxs`) was gated with a
corpus that stayed on the grid.

### 2.2 What the port did

`goose/model/quant-rows2.goose` and `goose/model/quant-weighted.goose` use one
primitive for both purposes:

```
extern "qz2_iq2_grid" fn iq2_grid(g: i64, i: i64) -> i64;
extern "qz2_iq3_grid" fn iq3_grid(g: i64, i: i64) -> i64;
```

and fourteen functions decode grid entries with it:

| file | functions |
|---|---|
| `quant-rows2.goose` | `iq2_best_nb`, `iq3_best_nb`, `iq1_best_nb2`, `qz2_row_iq3_xxs`, `qz2_row_iq3_s`, `qz2_row_iq1_m` |
| `quant-weighted.goose` | `iq2_best_nb`, `iq3_best_nb`, `iq1_best_nb2`, `qz_row_iq2_xxs`, `qz_row_iq3_xxs`, `qz_row_iq3_s`, `qz_row_iq1_m`, `qz_row_iq1_s` |

With the static table the decoded level is wrong (`(4-1)/2 = 1` where the
generated table says `(1-1)/2 = 0`), the level feeds back into `u`, and:

* `iq3_xxs` ends with a wrong block scale — the first differing byte in the
  gate is byte 0, the fp16 scale;
* `iq2_s` builds a `u` whose `kmap` entry is negative, and the aborted lookup
  cascades into an out-of-range index into the static table: the canon binary
  does not even reach `fail(1241)`, it dies with SIGSEGV;
* in the host bridge of S1.3 the same defect surfaced as `fail(1241)` and as
  scale mismatches.

## 3. The fix

Two changes, applied to both files, no arithmetic touched:

1. a second primitive for the quantizer view, next to the existing one:

```
extern "qz2_iq2_grid"  fn iq2_grid (g: i64, i: i64) -> i64;   // dequant view
extern "qz2_iq2_qgrid" fn iq2_qgrid(g: i64, i: i64) -> i64;   // quantizer view
extern "qz2_iq3_grid"  fn iq3_grid (g: i64, i: i64) -> i64;
extern "qz2_iq3_qgrid" fn iq3_qgrid(g: i64, i: i64) -> i64;
```

2. every use of the grid inside the fourteen quantizer functions now calls the
   `qgrid` primitive. The dequant modules (`quant-dequant2.goose`, with its own
   `qd_iq2_grid`/`qd_iq3_grid` bindings) are untouched and keep the static
   view, which is what the native dequantizers read.

This is a contract change for the host: a host that implements
`qz*_iq2_grid` must also implement `qz*_iq2_qgrid`, fed from the tables built
by `ggml_quantize_init` (`iq2_data[g].grid`, `iq3_data[g].grid`). The S1.3
bridge in the repository already has both tables in reach — `iq2_data` and
`iq3_data` live in `ggml-quants.c`, where its accessors are defined — so the
change is two more accessors there.

The patch to canon is `patches/canon-iq-qgrid.diff`; it touches exactly

```
goose/model/quant-rows2.goose     (6 functions, 13 changed lines)
goose/model/quant-weighted.goose  (8 functions, 15 changed lines)
```

and nothing else.

## 4. Gate

`tests/fixture.cpp`, one binary per module state. The fixture gives the module
both views:

* `qz2_iq2_grid` / `qz2_iq3_grid` — the static literals of `ggml-common.h`,
  which is what the canon port reads and what the dequantizers need;
* `qz2_iq2_qgrid` / `qz2_iq3_qgrid` — the tables built by the native init code,
  extracted into `results/native-tables.c` by `tests/make-tables.py`.

The oracle is the native library itself (`ggml_quantize_chunk` through
`libggml-base`), compared byte for byte.

Corpus: ten patterns (ramp, uniform, sine, sparse spikes, step, bump, small
ramp, alternating, constant, ternary) × one and two blocks × two types. The
counters prove the corpus leaves the grid: `offgrid_lookups=122245` on the
fixed build, i.e. the module asked for a grid point that is not in the map
122,245 times, and `nb_calls=1137684` neighbour entries were read.

```
results/gate-before.txt
== canon, full offgrid corpus
FAIL iq2_s pat=0 rows=1 byte=31 native=50 port=ff
FAIL iq2_s pat=0 rows=2 byte=31 native=50 port=ff
[segmentation fault]
exit=139

results/gate-before-iq3_xxs.txt
FAIL iq3_xxs pat=0 rows=1 byte=0 native=b2 port=b3
FAIL iq3_xxs pat=0 rows=2 byte=0 native=b2 port=b3
EXP636 module failure 1221

results/gate-after.txt
== fixed, full offgrid corpus
EXP636 type=offgrid cases=40 bytes=10800 mismatches=0 offgrid_lookups=122245 nb_calls=1137684
exit=0
```

## 5. Mutations

Twelve one-site edits of the fixed module, two halves of six, the Goose object
compiled at `-O0` (`tests/build.sh`), the same gate. A mutation counts as
caught only if the gate goes red.

| id | mechanism | result |
|---|---|---|
| A1 | `iq2` quantizers rebind to the static table | caught (mismatch) |
| A2 | `iq3` quantizers rebind to the static table | caught (`fail(1221)`) |
| A3 | `iq2_best_nb` scores neighbours on the static grid | caught (mismatch) |
| A4 | `iq3_best_nb` scores neighbours on the static grid | caught (18 mismatches) |
| A5 | `iq3_xxs` decodes the chosen neighbour on the static grid | caught (`fail(1221)`) |
| A6 | `iq2_s` looks its final group up in the IQ1 table | caught (mismatch) |
| B1 | `iq3_xxs` neighbour index one entry too far | caught |
| B2 | `iq3_xxs` scale search step 0.2 → 0.1 | caught (12) |
| B3 | `iq3_xxs` sign-correction parity inverted | caught (20) |
| B4 | `iq3_xxs` level clamp 7 → 6 | caught (1) |
| B5 | `iq3_xxs` level-to-value map drops +1 | caught (18) |
| B6 | `iq3_xxs` super-scale denominator 31 → 32 | caught (20) |

```
results/mutations.txt
half A: caught 6/6
half B: caught 6/6
after restore: EXP636 type=offgrid cases=40 bytes=10800 mismatches=0 ...
```

## 6. Second defect: `iq1_m` (not fixed here)

After the grid fix `iq1_m` still differs from native on 10 of 20 corpus cases
(`results/gate-iq1_m.txt`). The difference is not the grid view: the same cases
fail before and after, and they fail on smooth inputs (ramp, sine, spikes,
small ramp) while constant and ternary inputs match. Instrumented comparison of
the split decisions (`besti1`, `besti2`, `bestk`, `scale`) shows the port and
the native code agree on the first two groups of a block and diverge from the
third, which points at the prefix-sum reformulation of the exhaustive split
search in `qz2_row_iq1_m` (or at the neighbour search it feeds), not at the
tables. Replacing that search with a literal transcription of the native
triple loop made the result worse (14 mismatches), so the reformulation is not
simply non-bit-exact: something in it is wrong for this corpus.

`iq1_m` therefore needs its own experiment. It is excluded from the gate and
named here so it cannot be mistaken for fixed.

## 7. Boundaries

1. **Weighted (imatrix) arms.** The same fourteen-function fix is applied to
   `quant-weighted.goose` for consistency, but this experiment has no imatrix
   corpus, so those eight functions are patched and not gated.
2. **`iq2_xxs`, `iq2_xs`, `iq1_s`.** The same table defect applies (all three
   differ in every entry), and the fix covers their weighted arms. They are not
   gated: the native entries require an imatrix, and the unweighted reference
   has no such call.
3. **`iq1_m`.** See section 6.
4. **fp16 and host primitives.** `st_f16`, `bits/of_bits`, `nearest`, the sort
   and the load/store set are unchanged; the gate runs them exactly as the
   accepted P7-04 gates did.
5. **No model, no server, no GPU.** Nothing here says anything about runtime
   behaviour beyond the quantizer entry points.
6. **Host contract.** The patch adds two required primitives. A host that does
   not provide them fails to link, which is the intended, visible failure.

## 8. SHA-256

Canon inputs (byte identical to `4930d479`):

```
0ddce390e5fa69875fb70655bd26ebf4521d054cc8244934d3feb796941fe0d9  source/quant-rows2.goose
6d828de28ece9147998b4ea2f0044574896689fede1a2429d3f6fce4dadf5b0f  source/quant-weighted.goose
```

The patch to canon:

```
7a9159c6be7cd8a610039ea0a699f64d7fe9e903dc8d6d213f79993e957e00b5  patches/canon-iq-qgrid.diff
2bbab10bb43d3ce066381513e644a0d76bcdf8dbea184dba64234803c1a29a2b  patches/quant-rows2-iq-qgrid.diff
c9e6211b466c646ed6a3e57c965e7dddd3fdde80deec55a489f044269143af8f  patches/quant-weighted-iq-qgrid.diff
```

Experiment files:

```
cc0b6f2d21cbe5cc336bdc9dfb87c8b69411557fbfe6d05e095f7d6add290a3a  tests/fixture.cpp
8c31d9caa08c7d284f13579b39942b7f9ffc1ee5ee7f327579464521eeab8bd0  tests/mutate.py
e612c569d49a12ce1968d812a65ff71215f058cb94b40cc8c93ce079a3953a9f  tests/build.sh
251a8c145097fe37554bb0eab1a4950488c97b0795a33b4a34b083eea1700ba3  tests/run-mutations.sh
a23f3433d49acfa487ea00cef1a00c32789279aa26866fede2dd97436031369d  results/native-tables.c
2cb138abae0419a188275c85d912ebc2d8f2583bda7c49a0a77aaa302612ceaa  results/gen/fixed.c
ebcab290a29eb81cf45d1ea84bd97cfcf5fab9070f61505e6441e9e9954468d4  results/gate-after.txt
fd9c324e56c97c3e60945817b6fb1cb77d58d3c24cbf7aded7d844ad296c803e  results/mutations.txt
```

`results/hashes.txt` (written by `tests/hashes.sh`) is the authoritative list
and covers the gate binaries and generated C as well.

## 9. Raw evidence

```
results/tables.txt            static vs generated grid tables, per type
results/gate-before.txt       canon: crash / mismatches
results/gate-before-iq2_s.txt     canon iq2_s: segfault
results/gate-before-iq3_xxs.txt   canon iq3_xxs: 2 mismatches, fail(1221)
results/gate-after.txt        fixed: 40/40, 0 mismatches
results/gate-iq1_m.txt        fixed: iq1_m, second defect
results/mutations.txt         twelve mutations, halves, restore
results/native-tables.c       tables extracted from the native init code
results/gen/                  generated C for canon, fixed and every mutant
results/work/                 materialised module states
```
