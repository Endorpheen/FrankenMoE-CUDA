# P10-09 S2 — the tiny-MoE fixture plan

File: `source/fx-fixture-plan.goose` (619 lines, 9 exports, `fx_` prefix).
Base commit: `70de805c`; every native file is byte-identical at HEAD `9ac3eab0`.

## Hypothesis

The byte-identity gates depend on a generated fixture, and the generator is a
Python script that needs `gguf` and `numpy`. A native-free build has to be able
to describe the same fixture without those packages and without the script. The
part that matters is not the random weights but the *plan*: which tensors exist,
in which order, with which shapes and RNG seeds, which metadata the loader sees,
which writer mode is legal, and what the generator announces. That plan is pure
arithmetic and can be ported and gated offline.

## Native sources

| native | what is owned here |
|---|---|
| `upstream/BigMoeOnEdge/scripts/make-tiny-moe.py` | the architecture constants (31-44), the vocabulary rule (46-56), the tokenizer calls (63-70), both tensor plans (75-205), the writer construction including `small_first_shard` (83-90), the split refusal (236-238), and the banner lines |
| `runtime/tests/CMakeLists.txt` | the fixture names the gates request (`qwen3moe`, `gemma4`), the split case (51 tensors at 20 per shard), the python3-present skip |

## Interfaces

| export | decision |
|---|---|
| `fx_arch(name) -> i64` | `qwen3moe` → 0, `gemma4` → 1, anything else → -1 |
| `fx_split_guard(arch, split, out) -> i64` | a split gemma4 fixture is refused with the native message; 0 = legal, 1 = refused |
| `fx_small_first(split) -> i64` | a sharded writer always asks for a metadata-only first shard |
| `fx_tensor_count(arch) -> i64` | 51 tensors for qwen3moe, 76 for gemma4 (3 + 13 + 3×(13+7)) |
| `fx_shards(n, per, small_first) -> i64` | 51 tensors at 20 per shard with a small first shard = 4 files |
| `fx_tensor(arch, i, out) -> i64` | row `blk.<L>.<suffix> <d0>x<d1>[x<d2>] <seed>`, numpy shape order |
| `fx_meta_count(arch) -> i64` | 28 metadata calls for qwen3moe, 33 for gemma4 |
| `fx_meta(arch, i, out) -> i64` | the i-th metadata call as `name args`, including the three list-valued tokenizer calls expanded from the vocabulary rule |
| `fx_banner(arch, path, out) -> i64` | the `wrote …` line |

## Gate

`tests/capture_fx.py` runs the **real** generator with recording stand-ins for
`gguf` and `numpy`: no wheel is imported, no weight is generated, no file is
written. It records the writer construction, every metadata call in order,
every tensor with its numpy shape and RNG seed, and the banner. `tests/gen_fx.py`
builds one Goose driver from the port and compares the two transcripts line by
line at `-O1` and `-O0`.

```
cd tests && python3 gen_fx.py             # uses build/native_fx.tsv
cd tests && python3 gen_fx.py --rebuild   # re-runs the native generator
```

Result: **195/195 rows identical** (`M=61` metadata, `T=127` tensors,
`W=3` writer constructions, `B=3` banners, `G=1` split guard), `-O1` == `-O0`.

The 51-tensor count and the 4-shard arithmetic pin the CMake comment
"51 tensors at 20 per shard is deterministic: -00001-of-00004" as an assertion
rather than a comment.

## Mutations

`tests/mutate_fx.py <lo> <hi>`.

| # | mutation | result |
|---|---|---|
| 0 | the layer count changes | caught (row count 195 → 227) |
| 1 | the expert count changes | caught (metadata + shapes) |
| 2 | the fused gate_up width is halved | caught (row 149) |
| 3 | the grouped key/value weights are transposed | caught (row 34) |
| 4 | the per-layer seed stride shrinks | caught (row 44) |
| 5 | the attention width doubles | caught (row 8) |
| 6 | the vocabulary hex becomes lower case | caught (row 18) |
| 7 | the vocabulary loses a byte token | caught (row 18) |
| 8 | the sliding-window pattern inverts | caught (row 97) |
| 9 | the split writer stops asking for a small first shard | caught (row 192) |
| 10 | the split guard accepts a gemma4 split | caught (row 194) |
| 11 | the banner miscounts the MoE layers | caught (row 191) |

```
cd tests && python3 mutate_fx.py 0 6    # 6/6
cd tests && python3 mutate_fx.py 6 12   # 6/6
```

## Recorded limits

* Weights are not produced: they are random by construction and belong to the
  gguf writer. The port owns names, order, shapes and seeds, which is what makes
  the file reproducible.
* `fx_shards` reproduces the shard arithmetic the CMake test relies on, not the
  gguf library's splitter; the splitter itself stays native.
* `add_token_list` is checked through the vocabulary rule (uppercase `<0xNN>`,
  three specials first), not as stored bytes.
* The two tensors whose dimensions are symmetric (`ffn_gate_exps`,
  `ffn_down_exps`: 8×128×128) cannot expose a transposition; mutation 3 uses
  `attn_k` (64×128) for that reason.

## Assumptions and conflicts

* Constants are the script's, not the model's: 4 layers, 128 embedding,
  8 experts, top-2, 259 tokens. A different tiny fixture is a different slice.
* No conflicting module. `misc/shard-naming.goose` (P10-12 S3) parses shard
  *paths* at load time; this module decides the fixture *plan* at generation
  time. They meet only at the `-00001-of-DDDDD.gguf` convention.
* `import std` only, no externs.

## Integration step

1. Copy `source/fx-fixture-plan.goose` to `goose/misc/fixture-plan.goose`.
2. Add to `tools/sync-goose-tree.py` `CANONICAL_SOURCES`:
   `("misc/fixture-plan.goose", "EXP-<date>-<nnn>-p10-09-fixtures", "source/fx-fixture-plan.goose", "scripts/make-tiny-moe.py", "195 rows / 0 differences, 12/12 mutations")`.
3. Add the `goose/MANIFEST.json` entry with the file's SHA-256.
4. Update `tasks.json` `P10-09` (`slices_done` 2 of 3).
5. Keep `tests/native/make_tiny_moe.py` in the experiment directory as the
   pinned oracle copy (SHA-256 below) so the gate can be re-run without the
   upstream tree.

## SHA-256

```
3a6ed27760f9842768431c1aa984a01acff0cee700aa0e5d433040fa4786893d  source/fx-fixture-plan.goose
67f420a93191b7e93d358d8f0b7bcab34102a74e837de14c38682b40635290b2  tests/native/make_tiny_moe.py
754099ce7ae03a400dd0b4bf6efee6dbb741eeb866173513926eb45919ac53d3  tests/capture_fx.py
6a9aaae3b4878f371e7ac0abdd6cb6f28a452a32c968544e972b8eb08d26ac32  tests/gen_fx.py
b304b2115d17c2d765d9cbfda3a619ff6e57fab7c56654b88c90243ee6f0a118  tests/mutate_fx.py
cb4deaca2df98351694b0c7d0376616777c4330c6080ea1c63007828b825d4f1  build/native_fx.tsv
8ba587c4099e816332d625365e103a11590de3978db6ab6c02023a41f0cba2d7  build/goose_fx.tsv
```
