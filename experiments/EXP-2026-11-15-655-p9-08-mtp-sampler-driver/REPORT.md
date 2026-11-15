# P9-08 G4/G5/G7 — closing the engine-closure remainder

Date: 2026-11-15. Base commit: `050354e9`. Work done in
`/tmp/opencode/sub-657/EXP-2026-11-15-654-p9-08-mtp-sampler-driver/`; the shared
tree, the production runtime and other agents' files are untouched. No model, no
GPU, no server, no benchmark was started. Verdict: **ACCEPTED candidate**, three
slices, model-free.

- Compiler: `goose-2026.3-string`,
  sha256 `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`
  (matches `runtime/compiler/current.json`), runtime `1e714e6146bc`.
- Host C/C++: `gcc-13` / `g++-13 (Ubuntu 13.4.0-10ubuntu1)`.
- `runtime/core/src/engine/session.cpp` and `runtime/cli/main.cpp` are
  byte-identical to the base commit and to `1b3f4a1a`, so the extracted oracle
  is the base-commit text.

## 1. What this closes

`EXP-2026-11-15-636` §7 listed six open items for P9-08; `EXP-2026-11-15-650`
closed the first three (tallies, prefill plan, trace order) and one audit gap
(G6). This package closes the last three that a module can own:

| gap | state before | state now |
|---|---|---|
| G4 MTP capture warm-up (`session.cpp:992-1130`) | open — "larger, touches the vocabulary" | **closed** — `mw_`, 10 exports |
| G5 sampler chain (`session.cpp:937-945`) | open — "llama API call; substrate" | **closed** — `sch_`, 6 exports |
| G7 CLI driver loop (`cli/main.cpp` beyond the line protocol) | open — "queue/threads are substrate, the `--ppl` argument projection is not" | **closed** — `drv_`, 16 exports |

Neither gap was "just a llama call". G5 is two decisions around one allocation
(whether a chain exists, and which four stages in which order); G4 is an order
plus seven failure sites plus a container-walk audit; G7 is an argument table
with a span, a field and a parse kind per flag, the refusal codes and the
perplexity arithmetic. The llama calls themselves stay in the carrier in all
three.

P9-08 after this package: the two characterised behaviours of S2/S3 (the
perplexity prefix floor at `session.cpp:540`, the `rollback_turn` resize
asymmetry at `1463`) are still undecided, and wiring is still open. Nothing
else in the engine is unattributed.

## 2. Method

The carrier files cannot be linked (`session.cpp` pulls llama.cpp, the router
hook, the expert source and three sinks; `main.cpp` pulls the engine), so the
oracle is **extracted, not retyped** by `tests/native_extract.py`, which copies
exact byte ranges out of the base-commit files using unique line anchors and
fails if an anchor moves:

| region | lines | used by |
|---|---|---|
| sampler chain | `session.cpp:937-945` | `sch` gate |
| capture warm-up | `session.cpp:992-1008` | `mw` gate |
| captured-layer harvest | `session.cpp:1010-1037` | `mw` gate |
| dense/row split | `session.cpp:1056-1080` | `mw` gate |
| row window | `session.cpp:1082` | `mw` gate |
| warm-up KV wipe | `session.cpp:1129-1130` | `mw` gate |
| argument parse loop | `cli/main.cpp:606-776` | `drv` gate |
| workload/mode rejection | `cli/main.cpp:876-883` | `drv` gate |
| mode conditions | `cli/main.cpp:880,885` | `drv` gate |
| `--ppl-list` open refusal | `cli/main.cpp:887-891` | `drv` gate |
| `--ppl` file open refusal | `cli/main.cpp:910-914` | `drv` gate |
| `--ppl-list` line filter | `cli/main.cpp:893-895` | `drv` gate |

Each fixture `#include`s its regions inside functions that supply only the
services the port does not own:

- **`sch`**: a recording stub of `llama_sampler_chain_init`/`_add`/`init_*`, so
  "which stages, in which order, with which arguments" becomes comparable data.
- **`mw`**: a stub llama surface, hook and gguf offset maps. The offset map's
  `find` is the hook that makes the harvest order observable without touching
  the carrier, and the tensors are byte-less descriptors.
- **`drv`**: the parse loop runs in a **forked child** with the fixture's `cfg`,
  locals and `seen`, because two of its paths end the process
  (`std::exit(1)` for a missing value). The child writes a snapshot of the
  resulting configuration into shared memory, including the exit it took; the
  parent compares it with the module's projection. Both arms format their state
  through **one** snapshot function, so a difference is a difference in the
  parse, never in the printing.

## 3. Results

```
FG654_SCH cases= 14 values= 214 failures=0   sampler chain
FG654_MW  cases= 38 values= 324 failures=0   warm-up, harvest, dense split, row window
FG654_DRV cases= 93 values=3106 failures=0   argument projection, modes, --ppl path
```

145 cases / 3644 compared values, identical at `-O1` and `-O0`
(`results/gate-o1.log`, `results/gate-o0.log`). Doubles are compared bit-exactly
through the widened value; integers exactly; strings byte for byte.

Mutations, 12 per slice in two halves of six, plus a six-mutation extra half for
the `--ppl` arithmetic (the module regenerated at `-O0` for each):

```
FG654_MUT half=1 caught=18/18  sch=6/6 mw=6/6 drv=6/6
FG654_MUT half=2 caught=18/18  sch=6/6 mw=6/6 drv=6/6
FG654_MUT half=3 caught= 6/6   sch=0/6 mw=0/6 drv=6/6   (extra, drv only)
```

42 mutations, 42 caught, no survivors.

## 4. What the gates found

Four real holes, all closed by strengthening the gate or the corpus rather than
by relabelling:

1. **A first version could not see the difference it was built to see.** The
   warm-up plan originally carried only op codes, so a verify batch filled with
   `all_logits = false` compared equal to the carrier's `true`. The plan now
   carries the fill's `pos0`/`all_logits` and the decode width, which is exactly
   what "any valid token builds the same graph" means operationally.
2. **`mw_warm_token` had an equivalent mutant.** `bos <= 0` and `bos < 0` agree
   at `bos == 0` (both yield 0), so the mutation was unobservable rather than
   missed. It was replaced by `bos < -1`, which the `bos = -1` case catches.
3. **`llama_decode`'s failure code can be negative.** The corpus only used
   `rc = 0` and `rc = 1`, so `decode_rc > 0` survived `!= 0`. A `rc = -1` case
   was added: a port that tests `> 0` would ignore a real decode failure.
4. **Two refusal codes had no end-to-end path.** `drv_exit`'s list-open and
   file-open codes are only reachable by actually opening a missing file, so the
   two native refusal blocks are now extracted and run (against a missing path
   and a present one), and the workload rejection now has a `--ppl-list`-only
   case — the gap that let "the workload rejection ignores the ppl list" survive.

Behaviour carried, not normalised: the warm-up decodes BOS or 0; the MTP path
decodes the **same** verify batch twice, once through the target and once
through `common_speculative_process`; the KV wipe runs after the capture and
touches the draft context whenever it exists, independent of `is_mtp()`;
`--mtp --mtp` is not a conflict (`cur != 0 && cur != want` allows the same source
twice) while `--mtp --ngram` is exit 2; an action flag (`-h`, `--version`,
`--list-archs`) ends the parse with exit 0 and everything after it is ignored;
`--ppl-list` trims only the END of a line, so `" #not"` is a path and `"#not"`
is a comment; `--ppl-choices` keeps empty fields, so `"a,"` is two choices and
`","` is two empty ones.

## 5. Compatibility and assumptions

- `goose check` per module: 8 / 14 / 23 specializations, no errors, no warnings.
  The three sources concatenated typecheck as one unit (225 declarations,
  45 specializations), so the composition step sees no duplicate symbol.
- Prefixes `sch_`, `mw_`, `drv_` are free in the active tree; no file-name
  collision (`results/prefix-check.log`).
- Link: the three objects collide only on `goose_init`;
  `objcopy --redefine-sym goose_init=<prefix>_goose_init` resolves it and a host
  calling the three initialisers links and runs (`results/link-check.log`).
- Assumptions stated as inputs, not discovered at run time: `nslot` is the
  architecture's used projection slots (a null `exps_suffix`), `has_draft` is
  `im.ctx_dft != nullptr`, `offs_ok` is the gguf offset map's own verdict,
  `temp`/`top_p` reach the plan widened from the carrier's `f32`, and argv
  arrives already decoded as bytes.
- No new header field, no ABI change, no llama/ggml type across a boundary, no
  allocation, no globals.

## 6. Exact integration step

`README.md` §2 carries the call-site list: `session.cpp:937-945` and
`992-1130` (`open()`), and `cli/main.cpp:606-776`, `876-885`, `886-940`. The
llama calls, the sink constructions, the `printf` formats, argv decoding and
the number parsers do not move.

## 7. Limitations

- The gates compare the port against the carrier's **text**, not the carrier
  under a model. A decision that only misbehaves with a real capture (for
  example a slot that is skipped for the wrong architecture) is covered only as
  far as the synthetic state goes: one-, two- and three-slot architectures,
  unbound middle layers, an empty capture, missing tensors, missing offsets and
  inconsistent expert counts are all present in the corpus.
- The `mw` gate does not compare the tensors the hook actually captured, the
  dense byte accounting or the three trace static blocks; those are native and
  out of this slice.
- The `drv` gate does not run `validate()`, the sink constructors or the
  generation path; `drv_exit(8)` (a failed perplexity run) is pinned by the
  table alone, because no extracted path reaches it without a `Session`.
- Coverage here means "these three regions have a module with a gate", not "the
  carrier is dead": until the integration step lands, both files still compile
  into the production build.

## 8. Artifacts

| artifact | sha256 |
|---|---|
| `source/sampler-chain.goose` | `4cd6c37efb41e2e1e9567647bf743cd89abf7b10779b3208b8a58db2e1d19eac` |
| `source/mtp-capture-warmup.goose` | `0e07ec6f071e1980b3d6ff3971b5e4082b93f532bef4f23dc3f08e4dee5e9f0e` |
| `source/cli-driver-loop.goose` | `1d1e0e174ded264384e569cf66fe51c847a244ff3ab0674fac3c8bdc24710ad1` |
| `tests/sch_fixture.cpp` | `bc237f49eaaf9d3585da19a6dd033a2f5655698158fa3c2adc63d89e3767c772` |
| `tests/mw_fixture.cpp` | `266e93cd803b7c395558fa0b35eb7610c7914dd62a444794696fcc3787d321e9` |
| `tests/drv_fixture.cpp` | `8534133cdc07d22dcd4668b837139a7849e4fb81a9ea305fedb96a53d9c816e1` |
| `tests/native_extract.py` | `68f621ceaa1b41e1802fed01462114404ee985cfc30bb9c2b4ec079af3227ecf` |
| `tests/build.sh` | `30c522fdfd2e4e5d553bbd1223d2771d8ccba1c143a7ad4113daf2bff13ce2e7` |
| `tests/mutate.py` | `4f43a81c0a70c0a19a02916e3995114f60ceb64d6075414715c331bd07dd27cf` |
| `results/gate-o1.log` | see `results/hashes.txt` |
| `results/gate-o0.log` | see `results/hashes.txt` |
| `results/mutate-1.log`, `-2`, `-3` | see `results/hashes.txt` |

`results/hashes.txt` holds the full list, including the extracted regions and
the base-commit inputs (`session.cpp`
`24ea1ce4f796a49efa3a65d9f2cae7cc85f83abd781190451823cc090fb33593`, `main.cpp`
`baa36d47bcee47b7a212407a71faf1168d4877071d49369a1bc7936ef55b4e72`).

`build/` holds the generated C, the objects and the gate binaries; it is not
part of the deliverable.

## 9. Ledger change for the integrator

Under P10-12: G4, G5 and G7 move to closed, `slices_total` 3 → 6 with the
existing three, `remaining` drops the three gaps, `coverage_doc` points at this
experiment's `README.md`, and `status_totals`/`updated_at`/`work_front` are
refreshed in the same change. Under P9-08 the remainder shrinks to the two
characterised behaviours plus wiring; `P9-08` stays **PARTIAL** until those are
decided or consciously re-scoped.
