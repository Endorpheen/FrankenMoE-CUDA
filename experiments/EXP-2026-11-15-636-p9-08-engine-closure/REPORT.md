# P9-08 integration package — standalone engine closure, slices 1-3

Status: three slices ported and gated, production **unchanged**. This file is
the hand-off for whoever integrates them into the one Goose runtime.

- Base: `137bc40c`; `runtime/` unmodified in this working tree.
- Compiler: `goose-2026.3-string`,
  sha256 `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`
  (matches `runtime/compiler/current.json`), runtime `1e714e6146bc`.
- Host C/C++: `gcc-13` / `g++-13 (Ubuntu 13.4.0-10ubuntu1)`.
- No model, no GPU, no benchmark, no server was started.

## 1. Modules and their place in the canon

| Slice | Deliverable | Proposed canon path | Native target |
|---|---|---|---|
| S1 | `source/lga-load-admission.goose` | `goose/chat/engine-load-admission.goose` | `session.cpp` (open admission, model/context param projection, init order) |
| S2 | `source/ppl-perplexity-plan.goose` | `goose/chat/perplexity-plan.goose` | `session.cpp:459-622` (`Session::perplexity`) |
| S3 | `source/kvp-kv-turn-state.goose` | `goose/chat/kv-turn-state.goose` | `session.cpp` (`clear_kv`, prefix reuse, `rollback_turn`, appends) |

`goose/chat/` already holds the other `session.cpp` slices
(`session-loop.goose`, `thinking-control.goose`, `session-lifecycle.goose`),
so the three new modules belong beside them.

Ledger change required in the same commit: under `P9-08`,
`slices_total: 3`, `slices_done: 3`, `remaining` = the list in §7,
`coverage_doc` = this experiment's `README.md`, refreshed `status_totals` and
`updated_at`; then `tools/sync-goose-tree.py` adds the MANIFEST entries.
`P9-08` stays **PARTIAL** until §7 is closed or consciously re-scoped.

## 2. Interfaces the port relies on (all present at `137bc40c`)

From `runtime/core/include/bmoe/session.h` and `config.h`:

- `SessionConfig` — `vram_cache_mb`, `expert_hot_slots`, `n_gpu_layers`,
  `n_expert_used`, `chatml`, `compute_trace_layers`, `moe.enabled`,
  `spec.is_mtp()`, `spec.draft_max`.
- `Session::open(cfg, error, ...)` — the stage spine and every `fail()` site.
- `PplRequest` — `text`, `skip` (default 8), `as_decode` (default true),
  `step`, `choices`; `PplResult` — `n_tokens`, `n_scored`, `n_top1`, `nll`,
  `ppl`, `choice_logp`, `seconds`, the four routing counters.
- `GenerateRequest` — `clear_kv`, `n_predict`, `workload_*`;
  `Session` internals `kv_tokens`, `chat_history`, `history_pushed`,
  `ctx_dft`, `smpl`.

No new header field, no ABI change, no llama.cpp type crosses a slice
boundary. Goose export signatures (`i64`, `f64`, `const i64[:]`, `i64[:]`):

```
lga_hot_admit(hot,n_expert)                        -> i64
lga_override_admit(used,meta_ok,arch_len,n_expert) -> i64
lga_override_tag(code) / lga_override_value(code,used) -> i64
lga_mparams(gpu_layers,cpu_dev,moe,hot_slots,is_mtp) -> i64 mask
lga_cparams(hot_slots,moe,compute_trace,out[6])    -> i64
lga_init_plan(fail[20],out[20])                    -> i64 count
lga_refusal_code(stage) / lga_stage_rank(stage) / lga_stage_check(order) -> i64

ppl_token_gate(n,ctx) / ppl_prefix(skip,n) / ppl_step_last(ch,n)
ppl_scored(pos,skip,n) / ppl_score_count(n,skip,step,nb)
ppl_score_plan(n,skip,step,nb,out[])               -> i64 count
ppl_last_row(n,step,nb) / ppl_choice_gate(ch,bad,scored)
ppl_mean(nll,scored) / ppl_from_nll(nll,scored)    -> f64
ppl_delta(now,prev) / ppl_phase(step,as_decode)    -> i64

kvp_clear_actions(clear,draft,sampler)             -> i64 mask
kvp_max_common(n_tokens) / kvp_common_len(prev,cur,max) -> i64
kvp_rewind(prev_len,common,seq_rm_ok,out[2])       -> i64
kvp_draft_rewind(has_draft,seq_rm_ok)              -> i64
kvp_rollback_keep(chat,n_common,pushed,seq_rm_ok,out[5]) -> i64
kvp_prefill_span(chat,n_common,n_prompt) / kvp_append_token(chat,spec) -> i64
kvp_never_empty(n_tokens)                          -> i64
```

## 3. Exact integration step

### S1 — `Session::open()` (`session.cpp`)

1. `693-697`: replace the explicit-slot refusal with
   `lga_hot_admit(expert_hot_slots, info.n_expert) != 0` and keep the existing
   message. The budget-derived path stays `ss_hot_slots` (EXP632).
2. `737-751`: replace the four-branch ladder with
   `code = lga_override_admit(cfg.n_expert_used, info.ok, info.arch.size(), info.n_expert)`
   and switch on `code` to emit the four existing strings; `kv_overrides[0].tag`
   and `.val_i64` come from `lga_override_tag` / `lga_override_value`.
3. `699-722`: compute `lga_mparams(...)` from the flags and assert the expected
   bits before the assignments. The assignments themselves (function pointers,
   `devices` array) stay in C++; the mask is the drift pin that fails when a
   condition is edited without the module.
4. `819-847`: `lga_cparams(hot_slots, moe, compute_trace, out)` supplies
   `expert_hot_s`, the prepare-arming condition and the `cb_eval` condition.
   The floats are hundredths in the module (`98`, `110`): write them as
   `0.01f * out[1]` or keep the literals with an assert — do not print `98` as
   the decay.
5. `628-1271`: `lga_init_plan` / `lga_refusal_code` are the contract for the
   failing paths. Wiring is invasive (each `fail()` must carry its stage id);
   the low-risk first step is to log the stage and assert
   `lga_refusal_code(stage)` matches the message class. Do not re-order the
   existing stages to match the table — the table was built from them.

### S2 — `Session::perplexity()` (`session.cpp:459-622`)

1. `469-481`: `ppl_token_gate(n, im.cfg.n_ctx)`; `1` → the short-text error,
   `2` → the "exceeds n_ctx" error.
2. `540`: `prefix = ppl_prefix(req.skip, n)`.
3. `546`: `last = ppl_step_last(!req.choices.empty(), n)`.
4. `538-596`: allocate `count = ppl_score_count(...)` and fill
   `ppl_score_plan(...)` once, then walk the plan instead of the nested loops.
   The plan is flat (`pos*64 + row`); the decode still happens per chunk, so
   the loop becomes "decode chunk, score every plan entry whose `pos` falls in
   it". `ppl_scored` is the single predicate both regimes share.
5. `548`/`589`: `last_row = ppl_last_row(n, req.step, im.cfg.n_batch)`.
6. `598-610`: `ppl_choice_gate(!choices.empty(), bad_tokenized, scored)`; the
   choice tokenizer stays native and only reports how many failed.
7. `611-614`: `ppl_mean` / `ppl_from_nll`.

### S3 — `Session::generate()` (`session.cpp`)

1. `1324-1337`: `mask = kvp_clear_actions(req.clear_kv, im.ctx_dft != nullptr,
   im.smpl != nullptr)`; each bit replaces one native statement.
2. `1434-1455`: `max_common = kvp_max_common(tokens.size())`,
   `n_common = kvp_common_len(im.kv_tokens, tokens, max_common)`, then
   `kvp_rewind(im.kv_tokens.size(), n_common, seq_rm_ok, out)` for the target
   and `kvp_draft_rewind(im.ctx_dft != nullptr, seq_rm_ok)` for the draft.
3. `1459-1478`: `kvp_rollback_keep(chat_on, n_common, history_pushed,
   seq_rm_ok, out)`; `out[1]` is the token-list length, `out[4]` the target
   keep point, `out[3]` the draft keep point.
4. `1575-1576`: `kvp_prefill_span(chat_on, n_common, n_prompt)` is the append
   count.
5. `1854`: `kvp_append_token(chat_on, spec_on)`.

After each slice: re-run the byte-identity gates. These modules change no
graph, but S2 restructures the score loop and S3 changes where the token list
is written.

## 4. Compatibility checks

- **Leaves only**: no globals, no allocation, no `extern` beyond libm's `exp`,
  no llama/ggml type. Each module is a pure function set over integers, slices
  and doubles.
- **Prefixes free**: `lga_`, `ppl_`, `kvp_` have zero exports in `goose/`
  (`results/prefix-check.log`).
- **Link**: the three generated objects collide only on `goose_init`;
  `objcopy --redefine-sym goose_init=<prefix>_goose_init` resolves it and the
  binary links (`results/link-check.log`). The explicit initialiser of each
  renamed module must still be called once, as in EXP632.
- **Behaviour**: 467 cases / 3248 checks against the native transcription, 0
  differences, identical at `-O1` and `-O0`.
- **Runtime coupling**: none added. Every input is a value the carrier already
  computes at the call site.

## 5. Assumptions and conflicts

S1:

- The 20-id stage spine is an **interpretation** of `open()`, not a native
  table: one id per subsystem that can fail. `MPARAMS` covers both the model
  params and the override (they share one block); a failure injected there
  reports stage 5. Split it if finer attribution is wanted.
- `meta_ok`, `arch_len`, `cpu_dev_present` and the already-clamped budget are
  inputs. GGUF parsing, `accelerator_free_mib`, the tensor-name regex and every
  allocation stay native.
- The hot-store triple is carried in hundredths (`98`, `110`) because Goose's
  gate is an integer one; the native literals are `0.98f` and `1.10f`. This is
  a representation, not a rounding of behaviour.
- Refusal codes are internal ids; the error strings remain the carrier's and
  are matched by the integrator.

S2:

- **Native quirk preserved**: the step regime's prefix floor is one
  (`session.cpp:540`), so the prediction of token 0 is never scored even with
  `skip = 0`; a two-token text scores nothing and ends in "nothing scored". The
  port reproduces this and the gate pins it. Fixing it is a behaviour change
  and must be a separate, explicit decision.
- Choice strings are tokenized natively; the module only decides the order of
  the gates and how many choices failed.
- `ppl_mean`/`ppl_from_nll` reproduce the native double expression exactly and
  call the same libm `exp`; the gate compares bit patterns, not epsilons.
- The capacity check in `ppl_score_plan` is new (the native loop has no
  buffer). `-1` means "slice too small"; a caller must not read `out`.

S3:

- **Native asymmetry preserved, candidate fix**: `rollback_turn` resizes
  `kv_tokens` to `n_common` even when `seq_rm` refused the partial removal and
  the target memory was cleared (`1463` vs `1461`). The port keeps the native
  values so the gate pins the carrier; the wiring step should decide whether
  the token list must follow the clear (to 0) instead.
- The clear set is a mask; the order between independent stores is not
  observable and not modelled. `KVP_CL_TARGET` (memory) and `KVP_CL_TOKENS`
  (list) are separate bits although native clears both in one block.
- The draft rewind runs whenever a draft context exists, including when
  `n_common` did not move (`1448-1455`); that is what makes the second message
  of a conversation work.
- `kvp_common_len` takes both lists; the native `max_common` bound is passed in
  so the "one token always left" rule is visible in one place.

## 6. Artifacts

| artifact | sha256 |
|---|---|
| `source/lga-load-admission.goose` | `e89755c12fe2cc10596935c7f52cb4fb1ac4a46579c91a5abdcbf36f3b4c3276` |
| `source/ppl-perplexity-plan.goose` | `4a6508718a6328aabeddbbb2510495f18b5a3f45128657c7d43605a7310188ac` |
| `source/kvp-kv-turn-state.goose` | `aa2fb64bf8918758fa710be5722a497b49436660120752038f9117c9c1bc609e` |
| `tests/fixture_lga.cpp` | `c40a8b7a18034049d5fdab273e3c2cc3c70d37ed34857cef6df3efe15668045a` |
| `tests/fixture_ppl.cpp` | `f945f2e57bbf6bf59fd3cbc6996066e94ad0b70894f11def961a146d8511c696` |
| `tests/fixture_kvp.cpp` | `dece13cc796e7d3683ac76c2f7095de5610517024d730efac719e996d8d8e4ac` |
| `tests/build.sh` | `fb1d65c9b04e744282b0c790db1f90a1260e6a9c474d9d69f3decfc5a5d77fc2` |
| `tests/mutate.py` | `592b68f9e5d2385cf94f7d12fd833efa2bf59306c0a70fb442a89275373877a0` |
| `results/gate-O1.log` | `5f2ddb9280f7308f3177668a08f75a7fb98fb92ec494afcb17da747a691fe622` |
| `results/gate-O0.log` | `0605163129b686c61e244f394ecebdc4942cfa686d6c2540e7aa9f9aa156b093` |
| `results/mutations.log` | `74fcf9599013e3b6d3faebbb4e16998e287ea12fd5730f95902f8189f1c91c07` |
| `results/prefix-check.log` | `cc1222f89421c75929e828e8f93a8f6edb95a5a49dddf710414227de12fd9fff` |
| `results/link-check.log` | `a2794f7d8245022218b57547590439a38aac010d1bec52a93725c5615b7bf92d` |

`build/` and `build-O0/` hold the generated C, objects and gate binaries and
are not part of the deliverable (rule: evidence stays, build artefacts do not).

## 7. Honest remainder for P9-08 (not touched here)

Measured against `runtime/` at `137bc40c`, still native and still clean enough
to port:

1. `GenTally::record` and `PrefillTally` (`session.cpp:157-285`) — pure
   arithmetic over counters, deltas and the compute residual. Strongest next
   slice: no backend, 12+ mutations easy, and the metric columns are already
   `mt_`'s.
2. The prefill chunk plan (`1565-1591`) — `i = n_common; i < n_prompt; i += n_batch`
   with the mtp/non-mtp batch selection. Small, but it must not be confused
   with `kvp_prefill_span`, which only counts the append.
3. The trace framing order (`1480-1530`, `trace_begin`/`trace_flush`) — an
   ordered side-effect plan (compute closed first, route flush, then I/O
   stamped with the frame).
4. The MTP capture warm-up sequence (`988-1150`) — the order of
   `begin_capture`, the warm decode, the offset cross-check, `n_bound == 0`,
   the source init and the overlap/fork gate. `rh_` owns the rules, not this
   order.
5. `SamplingConfig` → sampler chain construction (`937-951`) — four
   `llama_sampler_chain_add` calls behind `temp > 0`.
6. `runtime/cli/main.cpp`'s driver loop — EXP572 ported the line protocol only;
   the argument parsing beyond `cc_`'s 11 fields, the bench driver and the
   `--ppl` command path are still native.
7. `runtime.cpp` — nothing left; `rts_` covers it.

Until items 1-6 are ported or consciously assigned, `P9-08` cannot be called
closed, and this package's three slices are its first accepted tranche.
