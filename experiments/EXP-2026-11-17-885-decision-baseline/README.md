# EXP-2026-11-17-885 — the decision baseline: 2/49 = 4.1%, and `can_reuse` is the frontier

EXP-884 established that counting `fgNNN_` call sites per file measures where the text is, not where a
decision is made. This replaces that number with one that measures decisions, and it is reproducible:
`tools/decision-coverage.py` derives it from the repository and needs no build.

## Baseline

```
file                         decisions  goose  native
llama-graph.cpp                     21      0      21
llama-hparams.cpp                   11      1      10
llama-vocab.cpp                     10      0      10
llama-model.cpp                      3      0       3
llama-kv-cache.cpp                   1      0       1
llama-memory-recurrent.cpp           1      0       1
llama-sampler.cpp                    1      0       1
llama-model-loader.cpp               1      1       0
TOTAL                               49      2      47

decision coverage: 2/49 = 4.1%
```

The unit is a predicate named `is_/can_/select_/should_/need_/use_/has_` whose body answers a question, counted
as Goose when the body contains an `fgNNN_*` call.

## The accessor correction, made before the number was reported

The first run said 55 decisions and 3.6%. Fourteen of those were not decisions: `is_normal`, `is_unknown`,
`is_control`, `is_byte`, `is_user_defined`, `is_unused`, `is_eog` are `return id & FLAG` in two token structs -
accessors, not choices. Counting them would have inflated the figure exactly the way call-site counting did, so
they are excluded by a rule now written into the tool: a body of three lines or fewer whose only statement is a
bitfield test is an accessor.

What remains is honest: 49 decisions, 2 of them Goose.

## The frontier, plainly

`can_reuse` — 19 instantiations plus `can_reuse_impl` — is **21 of the 47 native decisions, and none of them are
Goose**. It is the per-architecture answer to "can this graph be reused for the incoming batch", asked during
graph construction on the request path. `grep` over `goose/` finds no module for it; the only `reuse` matches are
in the Jinja family, which is unrelated chat-template code.

So the largest single block of native decisions in the working runtime is graph reuse, and unlike the plan
(already answered by `fg200g`) or the tokenizer decisions (already routed), there is no Goose implementation of
it to wire. That makes it genuinely new work rather than another composition — and it should be the next target
precisely because it is the highest count, not because it is convenient.

## Reproduce

```
python3 tools/decision-coverage.py          # table and total
python3 tools/decision-coverage.py --list   # every native decision body, file:line name
```

`baseline.txt` in this directory is the `--list` output at the time of writing, kept so a later run can be
diffed against it rather than compared from memory.
