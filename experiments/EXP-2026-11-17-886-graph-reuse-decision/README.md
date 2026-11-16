# EXP-2026-11-17-886 — the graph-reuse decision in Goose, proven equal before it is wired

`can_reuse` is 21 of the 47 native decisions in the working runtime (EXP-885) and had no Goose implementation
to compose. This writes one and proves it equal to the code it will replace; it does not yet move the call
sites.

## The decision

Every instance of the family is the same question over a different field list:

```
res &= (!params.ubatch.token) || (tokens && tokens->ne[0] == params.ubatch.n_tokens);
res &= (!params.ubatch.embd)  || (embd   &&   embd->ne[1] == params.ubatch.n_tokens);
... five to eight such rows per architecture
```

In `goose/cpu/graph-reuse.goose` the rows are data: the caller supplies three equal-length arrays - whether the
input exists, the extent the incoming batch needs, and the extent the existing tensor has - and Goose answers.
Adding a field becomes one more row on the host side instead of another copy of the same conjunction.

It is FFI-light by construction: `ld`/`stw`/`fail` and nothing else, so there is no native dependency to
translate and no second decider is implied.

## Verified

```
GR_EQ cases=4000 mismatches=0 PASS
```

4000 randomised cases, 1-8 fields each: `present` is 0 or 1, `want` is 0..3, and `have` matches the want in two
trials of three so agreement cannot be reached by everything simply differing. The native side is the
conjunction transcribed directly from the C++ (`c &= (!p[i]) || (w[i] == h[i])`), the Goose side is the module.

`tests/run.sh` reproduces this from the repository.

## Deliberately not done

The 21 call sites are untouched. Moving them is a mechanical edit per graph input type, and it changes code on
the request path; doing it in the same step as the first proof of the decision would leave no clean signal about
which of the two introduced a fault. The decision is proven equal first, then it is wired.

## Next

Wire `llm_graph_input_*::can_reuse` to the module behind `GGML_GOOSE_GRAPH_REUSE=0`, one input type at a time,
keeping the C++ bodies as the rollback path, and confirm the reference hash and decode on the pinned profile.
