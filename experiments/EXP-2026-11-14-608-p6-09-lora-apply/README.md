# EXP-2026-11-14-605 — LoRA application, scale and cleanup (P6-09 S2)

Native counterparts (flint tree, base commit `05048b02`):

- `src/llama-graph.cpp:1629-1671` — `llm_graph_context::build_lora_mm()`, the
  per-weight LoRA loop;
- `src/llama-adapter.cpp:140-150` — `llama_adapter_lora::get_weight()`, the
  name-to-pair lookup that decides whether a weight participates;
- `src/llama-adapter.h:52-57` — `llama_adapter_lora_weight::get_scale()`;
- `src/llama-adapter.h:85-87` — `llama_adapter_lora::get_n_nodes()`;
- `src/llama-adapter.cpp:503-515` — `llama_adapter_lora_free()`.

## What this slice covers

`source/lora-apply.goose` owns the pure decisions of the application path. It
builds no graph and touches no tensor: the caller passes the adapter pair table
that S1 produced, the tensor geometry, the weight name and the activation
shape, and gets back a description of what the runtime must build.

| export | native | scope |
|---|---|---|
| `lora_budget` | `get_n_nodes` | 6 nodes per pair, the graph budget |
| `lora_app_scale` | `get_scale` | `alpha == 0` keeps the caller scale, else `caller * alpha / rank`, `rank = b.ne[0]` |
| `lora_apply_dense` | the LoRA loop of `build_lora_mm` + `get_weight` | selection, per-adapter scale, delta geometry, node count |
| `lora_cleanup` | `llama_adapter_lora_free` | unregister from `model.loras`, then release; null is a no-op |

Semantics the port pins down:

- **selection** is an exact byte comparison of the weight name with the base
  name of a pair. An adapter without a pair for that weight contributes no
  node at all: the base matmul is the whole result.
- **scale** is recomputed for every (adapter, weight) pair, exactly as
  `build_lora_mm` calls `lw->get_scale(...)` on every hit. `alpha == 0` is the
  absent-alpha case and keeps the caller's scale.
- **node sequence** per applied adapter, in native order: `a x cur`,
  `b x (a x cur)`, `scale`, `add` onto the base result — four nodes, plus the
  base `mul_mat`. That is four, not six: the six-node figure belongs to
  `get_n_nodes` and is a per-pair budget, so the port reports both numbers
  separately and the gate checks both.
- **batch semantics**: `cur` is `[n_embd, n_tokens, ne2, ne3]`; the first
  matmul yields `[rank, n_tokens, ne2, ne3]`, the second and the delta keep
  `[n_out, n_tokens, ne2, ne3]`, and the add is elementwise on that shape. The
  three trailing dimensions are carried through unchanged, which is what the
  `c07`, `c08` and `c19` cases check.
- **shape guards** (port-only, native asserts): `a.ne0 == cur.ne0`,
  `b.ne0 == a.ne1`, `b.ne1 == w.ne1`, and a positive rank.
- **cleanup**: `free` erases the adapter from `model.loras` while the adapter
  still points at the model, then releases it. A null pointer is a no-op; an
  adapter that was never registered is only released (erase of a missing key);
  an id that is not in the registry reports `found = 0` and leaves the
  registry alone. The registry is the caller's ordered copy of the set, because
  the native container is a hash set with no order.

Flat layouts, no structs, no allocation: adapters are `alpha`/`caller` float
arrays, pairs are `PAIR_SLOTS` integers per entry (adapter, name offset, name
length, a, b), and the output is `AP_SLOTS` integers per adapter plus one float
scale per adapter. Return codes are documented at the top of the source file.

## Gate

The oracle is the real `build_lora_mm`, not a transcription. The fixture
constructs a `llm_graph_result` and a `llm_graph_context` with only `loras` and
`res` set (no `hdmd`, no `prec_policy`, no `w_s`, which is the plain dense
path), calls the method once for one weight tensor, expands the graph, and
evaluates it on a CPU backend. The adapter objects are built field by field
(`alpha`, `ab_map`) instead of being loaded: S1 already gates the loader, and
this slice starts from the pair table it produces.

`llama_model` is abstract in this tree (seven pure virtuals), so `TestModel`
implements them as unused stubs; `build_lora_mm` never calls them.

For every case the gate compares the port description with the native graph:

- the number of `MUL_MAT`, `SCALE` and `ADD` nodes and the op of the returned
  tensor;
- every `SCALE` constant against the port scale, bit for bit (a single adapter
  case uses a direct bit comparison, the multi-adapter cases compare the
  multiset because the native container is unordered);
- every delta shape `[b.ne1, n_tokens, ne2, ne3]`;
- the `get_n_nodes` budget against the port budget;
- the cleanup result against the native set size before and after the free;
- the evaluated tensor against a reference computed from the **port's** data
  (selected pair, scale, shapes), so a wrong pair or a wrong scale changes the
  numbers and cannot pass.

All payloads are multiples of 0.25 in [-2, 2] with `n_embd <= 8` and
`rank <= 3`, so every dot product is an exact float sum; a different
accumulation order, or FMA, cannot change a bit. Scale application is one float
multiply on both sides, so the gate compares values with exact equality
instead of a tolerance.

```
FG605_LORA cases=21 values=435 failures=0
```

| case | what it pins down |
|---|---|
| `c01_single` | one adapter, alpha 2 over rank 2, caller scale 1 |
| `c02_alpha_zero` | alpha 0 keeps the caller scale 0.5 |
| `c03_scale_bits` | non-representable scale (0.1, 1.7, rank 3), bits |
| `c04_no_weight` | adapter without a pair for this weight: base matmul only |
| `c05_pair_select` | two pairs, only the matching one applies |
| `c06_two_adapters` | two deltas, two scales, exact sum |
| `c07_batch` | four tokens: delta ne1 = 4 |
| `c08_batch_4d` | `[4, 2, 3, 2]` activation |
| `c09_rank1` | rank 1 |
| `c10_negative` | negative alpha and negative caller scale |
| `c11_mixed` | rank 3, two tokens, one skipped adapter |
| `c12_scalar` | `n_embd = 1`, one output row |
| `c13_rank0` | rank 0: port refuses, native divides by zero (contract) |
| `c14_bad_out` | `b.ne1 != w.ne1`: port refuses (contract) |
| `c15_cleanup` | free a registered adapter, graph uses the survivor |
| `c16_cleanup_unregistered` | free an adapter that was never registered |
| `c17_cleanup_null` | `free(nullptr)` is a no-op |
| `c18_cleanup_unknown` | free the only adapter; unknown id keeps the registry |
| `c19_batch_4d_two` | two adapters over `[4, 3, 2, 2]` |
| `c20_no_adapter` | empty adapter set |

## Mutations

`tests/mutations.json` holds 12 mutations in two halves: A is selection and the
application scale, B is geometry, node budget and cleanup. Each must still pass
`goose check` and must then fail the gate.

```
python3 tests/mutate.py --half A   # 6/6 caught
python3 tests/mutate.py --half B   # 6/6 caught
python3 tests/mutate.py            # 12/12 caught
```

The recorded gate and mutation output is kept in `tests/logs/`.

## Recorded limits

- The MoE variant `build_lora_mm_id()` is **not** covered. It repeats the same
  scale formula over `mul_mat_id` with per-expert 3D tensors and the
  expert-tier `slots`/`moe_cache` branch; the expert geometry belongs to the
  MoE runtime slices. The scale rule it computes is the one this module owns.
- The token-embedding LoRA block inside `build_tok` (a lambda in the graph
  builder, `src/llama-graph.cpp:2696-2708`) is not covered: it is not callable
  on its own and it reads `get_rows(a, ids)` instead of `a x cur`. Its shape
  rule is S1's flipped `token_embd` rule; its scale is the same `get_scale`.
- `w_s` post-multiplication, the NVFP4 precision policy and the Hadamard input
  folding happen outside the LoRA loop and are not ported. The gate runs the
  plain dense path (`w_s == nullptr`, `hdmd == nullptr`, `prec_policy ==
  nullptr`).
- Adapter tensors are treated as 2D. `ggml_mul_mat` would accept a 3D `a` that
  broadcasts over `cur.ne2`; the loader only produces 2D pairs, so the port
  keeps `t_ne0`/`t_ne1` and does not model that broadcast.
- The port requires `b.ne1 == w.ne1`. Native `ggml_add` would broadcast a
  narrower `b` (and assert on a wider one), but S1 rejects such a pair at load
  time, so this is a port-only guard and the gate checks it as a contract case
  rather than against native.
- Rank 0 is `E_RANK` in the port. Native divides by zero and builds an infinite
  scale; again unreachable after S1, so it is a contract case.
- The adapter order is the caller's. Native iterates
  `std::unordered_map<llama_adapter_lora *, float>`, so the port takes an
  ordered list and the gate compares scale multisets and shapes, not node
  order. With the dyadic payloads used here the sum is order-independent, which
  is why exact equality is legitimate.
- Cleanup models `model.loras` only. The context keeps its own
  `llama_adapter_loras` scale map; native `llama_adapter_lora_free` does not
  touch it, and a caller that keeps a freed adapter in a context is out of
  scope.
- The scale formula is duplicated from S1's `lora_scale` because the native
  application calls `get_scale` for every tensor. The two copies must stay in
  sync; this gate and the S1 gate both compare against the same native header.
- Only the CPU backend is exercised, with F32 tensors and no model weights.
  Loading, buffers and adapter registration belong to S1 and to the loader.
