# P9-08 G4/G5/G7 — MTP warm-up, sampler chain, CLI driver loop

Three Goose modules that close the last audit gaps of P9-08 (P10-12 `REPORT.md`
§1: G4, G5, G7). Model-free, GPU-free, production untouched: `runtime/` is
byte-identical to base commit `050354e9`.

| slice | module | canon path | native region (session.cpp / cli/main.cpp at `050354e9`) |
|---|---|---|---|
| G5 sampler chain | `source/sampler-chain.goose` | `goose/sampling/sampler-chain.goose` | `session.cpp:937-945` |
| G4 MTP capture warm-up | `source/mtp-capture-warmup.goose` | `goose/misc/mtp-capture-warmup.goose` | `session.cpp:992-1008`, `1010-1037`, `1056-1082`, `1129-1130` |
| G7 CLI driver loop | `source/cli-driver-loop.goose` | `goose/cli/driver-loop.goose` | `cli/main.cpp:606-776`, `876-883`, `880-885`, `886-940` |

Prefixes `sch_`, `mw_`, `drv_` have no export anywhere in `goose/` outside
`_archive` (`results/prefix-check.log`), and no existing file name collides.

## 1. Interfaces

All exports are pure functions over integers, doubles and slices. No allocation,
no globals, no llama/ggml type crosses a boundary, no `extern` beyond libm.

### `sch_` — six exports

```
sch_on(temp)                                 -> 0/1     sampling on at all
sch_entries(temp)                            -> 0/4     stage count
sch_default_seed()                           -> i64     0xFFFFFFFF == LLAMA_DEFAULT_SEED
sch_seed_gate(seed)                          -> 0/1     config default still mirrors it
sch_min_keep()                               -> 1       top_p's floor
sch_plan(temp, top_k, top_p, seed, out[], outf[]) -> count
```

Plan stride 3 in `out` (kind, integer argument, min_keep) and 1 in `outf` (the
float argument). Kinds 1..4 are top_k, top_p, temp, dist — the carrier's add
order, because the chain is applied in insertion order.

### `mw_` — ten exports

```
mw_warm_token(bos)                            -> token   BOS, or 0 when absent
mw_warm_plan(moe, mtp, has_draft, out[])      -> count   (op, a0, a1) triples
mw_warm_fail(decode_rc, process_ok, mtp)      -> 0/1/2   which failure closed the open
mw_slot_step(bound, used, has_tensor, has_off)-> code
mw_count_step(cur, ne2)                       -> code    adopt / check / mismatch
mw_bound_gate(n_bound)                        -> 0/1
mw_offsets_gate(offs_ok)                      -> 0/1
mw_walk(nl, nslot, bound[], used[], has_t[], off[], ne2[], out[]) -> count
mw_dense_bucket(is_expert, has_tensor, has_buffer, dev_non_cpu, has_off, has_size, is_row) -> -1/0/1
mw_row_budget(mb)                             -> bytes
```

Warm-up ops: 1 begin_capture, 2 fill verify batch (a0 = pos0, a1 = all_logits),
3 decode verify (a0 = positions), 4 speculative process, 5 decode one-shot
(a0 = positions), 6 end_capture, 7 clear target KV, 8 clear draft KV.

Harvest plan, five i64 per event, in native order: 0 unbound layer, 1 visit
(l, p, offset, ne2), 2 missing tensor, 3 missing offset, 4 count adopted,
5 count checked, 6 count inconsistent (l, p, ne2, expected), 7 total bound,
8 total experts. A failure op is the last event.

### `drv_` — sixteen exports

```
drv_flag_id(name)                             -> 0..62 / -1
drv_kind(id)                                  -> 0..14 / -1
drv_span(id)                                  -> 0/1 whether it consumes the next token
drv_field(id)                                 -> 0..58 / -1
drv_action(id)                                -> 0 help, 1 version, 2 list-archs / -1
drv_dense_alias(id)                           -> enum value for --no-warm-dense / --dense-odirect
drv_exit(refusal_kind)                        -> exit code (1 or 2)
drv_draft_conflict(cur, want)                 -> 0/1
drv_workload_conflict(session, has_ppl, has_list, has_capture, has_replay) -> 0/1
drv_mode(session, has_ppl, has_list)          -> 0 one-shot, 1 session, 2 ppl
drv_list_keep(line)                           -> trimmed length / -1 to skip
drv_ppl_path_count(has_path, list_lines)      -> count
drv_csv_count(commas)                         -> fields
drv_csv_next(s, pos)                          -> comma index / -1
drv_ppl_pct(n_top1, n_scored)                 -> f64
drv_ppl_label(n_paths)                        -> 0/1
```

Kinds: 0 int, 1 float, 2 u32, 3 string, 4 sets true, 5 sets false, 6 `--cache-mb`
auto|number, 7 `--dense-weights` enum, 8 comma list, 9 action, 10/11 draft
source, 12 string plus a second bool, 13/14 dense aliases.

Deliberately NOT in `drv_`: argv decoding, `atoi`/`atof`/`strtoul`, the
`--dense-weights` vocabulary (`cc_dense_mode`, P9-01 S3), the flag-vs-env
precedence (`cc_env_int`/`cc_env_bool`/`cc_cache_auto`, P9-01 S3), the sink
constructions and every printf format.

## 2. Exact integration step

### G5 — `Session::open()`, `session.cpp:937-945`

1. Keep the `static_assert` and add `assert(sch_default_seed() == LLAMA_DEFAULT_SEED)`.
2. Replace `if (cfg.sampling.temp > 0.0f) { ... }` with `if (sch_on(temp) != 0)`,
   then `n = sch_plan(...)` into two stack arrays (`int64_t[12]`, `double[4]`).
3. `llama_sampler_chain_init` once, then for `k in 0..n` switch on `out[3k]` and
   call the matching `llama_sampler_init_*` with `out[3k+1]` or `outf[k]`
   (`top_p` also passes `out[3k+2]` as `min_keep`). No other change: the
   `smpl` pointer, the ownership and the argmax path are the carrier's.

### G4 — `Session::open()`, the streaming block

1. `992-1008`: replace the block with a walk of `mw_warm_plan(cfg.moe.enabled,
   cfg.spec.is_mtp(), im.ctx_dft != nullptr, plan)`. Op 2 calls `batch_fill(...,
   plan[a0+1], /*all_logits*/ plan[a0+2] != 0)`; ops 7/8 are the two
   `llama_memory_clear` calls moved after the capture; the token comes from
   `mw_warm_token(llama_vocab_bos(im.vocab))`. After the decode/process calls,
   `mw_warm_fail(rc, process_ok, mtp)` decides which `fail()` string to return.
2. `1010-1037`: build the five flat arrays from `im.hook->captured()` and
   `offs`, call `mw_walk(...)` once, and let the plan drive the two nested loops.
   `mw_offsets_gate(offs.ok)` replaces the early return; `mw_bound_gate(n_bound)`
   replaces the `n_bound == 0` test. The message text and the `file_off`/
   `file_idx` assignments stay in the carrier.
3. `1056-1080`: one `mw_dense_bucket(...)` call per captured weight replaces the
   four `continue` predicates.
4. `1082`: `mw_row_budget(cfg.moe.row_stream_mb)`.
5. `1129-1130`: unchanged text, now reached through op 7/8 of the plan.

### G7 — `cli/main.cpp`

1. `606-776`: replace the flag `if`-chain with `id = drv_flag_id(a)` plus a
   `switch (id)`; `drv_span(id)` drives the `next()` call, `drv_field(id)` names
   the projection slot, `drv_kind(id)` selects the parse. `drv_draft_conflict`,
   `drv_dense_alias` and `drv_action` cover the three special branches;
   `drv_exit(kind)` supplies every refusal code, so the codes sit in one place.
2. `876-883`: `drv_workload_conflict(...)` replaces the nested condition.
3. `880-885`: `mode = drv_mode(...)` selects session / ppl / one-shot; the
   branch bodies are unchanged.
4. `891-899`: `drv_list_keep(line)` replaces the trim-and-skip test.
5. `886-890`, `923-927`: `drv_ppl_path_count`, `drv_ppl_label`, `drv_csv_count`,
   `drv_csv_next`, `drv_ppl_pct`.

## 3. Compatibility checks

- `goose check` per module: 8 / 14 / 23 specializations, no errors, no warnings.
  The three sources concatenated typecheck as one unit (225 declarations, 45
  specializations): `results/compose-check.log`.
- Link: the three objects collide only on `goose_init`;
  `objcopy --redefine-sym goose_init=<prefix>_goose_init` resolves it and a host
  that calls the three initialisers links and runs (`results/link-check.log`).
- Prefixes free: `results/prefix-check.log`.
- Behaviour: 145 cases / 3644 compared values against the verbatim native text,
  0 differences, identical at `-O1` and `-O0` (`results/gate-o1.log`,
  `results/gate-o0.log`).
- Mutation: 12 per slice in two halves of six, plus six extra `drv_` mutations
  for the `--ppl` arithmetic — 42/42 caught (`results/mutate-*.log`).
- No new runtime coupling: every input is a value the carrier already computes
  at the call site.

## 4. Boundaries

- **G4 gates the order and the audit rules, not the graph.** Which tensors the
  hook captures, `dense_bytes_per_layer`, the route/compute/io trace static
  blocks and the streamer's buffers stay native and are not compared. The
  fixture's tensors are byte-less descriptors; the offsets are integers.
- **G5 keeps the chain object native.** The gate compares existence, order and
  arguments; `llama_sampler_chain_init`'s allocation and the sampler's own
  behaviour are llama.cpp's. `temp` and `top_p` reach the plan widened to `f64`
  from the carrier's `f32`, so the comparison is bit-exact on the widened value,
  not on the `f32` bits.
- **G7 keeps argv, the number parsers and the enum vocabulary native.** The
  module's opinion is the table, the span, the field, the kind, the refusal
  codes, the mode order and the `--ppl` arithmetic. The refusal code for a
  failed perplexity run (`drv_exit(8) == 1`) is pinned by the table alone: no
  extracted path reaches it without a `Session`, so it is the one code the gate
  does not compare end to end.
- The `--ppl` report's formatting stays native (`printf`); only the percentage
  and the label condition are decisions.
- `drv_flag_id` compares bytes, so a non-ASCII flag is simply unknown — as it is
  natively.

## 5. Artifacts

`results/hashes.txt` carries the SHA-256 of every source, test and extracted
region. `build/` holds generated C, objects and the gate binaries; it is not
part of the deliverable (evidence stays, build artefacts do not).

## 6. Running the gates

```
bash tests/build.sh                 # all three gates at -O1
bash tests/build.sh all mutate      # all three at -O0
python3 tests/mutate.py 1 --jobs 6  # first half: 6 mutations per slice
python3 tests/mutate.py 2 --jobs 6  # second half
python3 tests/mutate.py 3 --jobs 6  # extra: the --ppl arithmetic
```

Overrides: `FG_REPO` (default `/home/end0/AI/FrankenMoE-CUDA`), `FG_GOOSE`,
`CC`, `CXX`. The root is derived from the script's own location; there are no
absolute scratch paths.
