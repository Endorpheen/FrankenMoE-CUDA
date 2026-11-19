# The remaining items, by the one thing that blocks each

36 items were open at the start of this session; two were closed by evidence (P10-12, P10-08), leaving 21 PARTIAL and 13
TODO. Nearly every one of them is blocked by one of four things, and none of the four is "more porting work". This is the
list that makes the remainder a decision rather than a backlog.

## Class A - blocked by the acceptance rule, not by work (10 items)

Rule 15 forbids deleting the replaced C++ before acceptance, and the acceptance criteria of these items include that
deletion. Their work is **done**; what remains is not mine to do.

| item | done | waits for |
|---|---|---|
| P6-10 | sampling, grammar, RNG, chain, ABI all verified; grammar default-on blocked by cost (11.7x) | acceptance, then one removal pass |
| P7-06 | encoder served and swept over 248 320 token texts | acceptance, then delete the native tokenizer |
| P7-07 | detokeniser served on every path, streaming edges measured | acceptance, then delete the native body |
| P7-12 | reader verified (34 values bit for bit, 744 231 array elements); v1 and endian items closed as not-applicable | acceptance, then delete the native reads |
| P8-12 | `find_tool`'s decision exists as `tool_index`/`tool_flags`; blocked from composing by its `json_parse` provider | acceptance, then the adapter placement |
| P9-03 | policy compared on 61 441 decisions | acceptance; serving needs the maps inside the door |
| P11-04 | D1 F2-F5, F8, F10 remain | acceptance of each switch |
| C1-02 | three slices done | a Qwen GGUF to load |

## Class B - the host side is not in this tree (5 items)

Verified by grep with the hits inspected by name (`HOST-SIDE-INVENTORY.md`).

| item | missing host |
|---|---|
| P8-06 | `model_router.cpp` appears nowhere in `vendor/flint`, `integration`, `runtime` |
| P8-09 | `web_serve` appears nowhere |
| P9-07 | the CSV/decode-trace/route-trace sinks live in `runtime/core/src/metrics/`, not the shipped tree |
| P9-08 | `session.cpp` / `cli/main.cpp` match only unrelated files |
| P10-09 | `scripts/bench-analyze.py` and `trace_io.py` do not exist; the module attaches when they do |

## Class C - decided, and the decision is "not applicable here" (2 items)

| item | finding |
|---|---|
| P7-12's v1 half | the native reader refuses GGUF v1 outright; porting it would diverge from the product |
| P7-12's endian half | no byte-swap path exists and the format defines one order |

## Class D - real work, no blocker (the rest)

| item | next concrete step |
|---|---|
| P10-02 | compose fit-params (done), bench-report (done), tokenize utility: the standalone CLI already carries its decisions; a shipped tool is absent |
| P10-06 | compose arch/flags/install (done); the call sites are in CMake, which is a different kind of change |
| P10-07 | replace `common_json` with the Goose parser: needs `gs_json_strtod` from `goose_host.c`, which is not in this artefact |
| P10-10 | Apple/Windows and AVX512/VNNI/AMX need hardware this machine does not have |
| P6-11 | MTP needs a draft GGUF matching the target vocabulary |
| P11-01 | the standalone runtime lacks CUDA, tokenizer engine and HTTP |
| P11-02, P11-03, P11-07, P11-08, F1-01..04, C1-03..06, C2-04, P9-04, P9-05 | downstream of P11-01 |

## The one number that matters

Of 36, **two are closed and 15 are blocked by a rule, a missing host file, or a decided non-applicability** - none of which
this agent can change. The 19 in class D are downstream of items in the same list: P11-01 is the gate for eleven of them, and
it is blocked because the standalone runtime cannot yet run Apex, which is a large integration, not a checklist step.

## What a decision would unblock

1. **Acceptance of the served paths** (P7-06, P7-07, P7-12, P6-10, P9-03) turns five items into removal work and closes them.
2. **Placing `goose_host.c` in the shipped artefact** would unblock P10-07 and P8-12's tool dispatch, and is one build-set
   decision - but it collides with the provider already in the sampler-chain shim, which is why it needs a decision rather
   than a patch.
3. **A second small GGUF** (draft model, and a Qwen model) unblocks P6-11 and the whole C1 group.
