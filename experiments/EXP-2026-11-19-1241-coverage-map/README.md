# EXP-2026-11-19-1241: how much of the runtime actually calls Goose — measured across the main files

Method, reproducible in one line per file:

```
grep -cE 'fg_[a-z_]+_call|goose_init|fg_goose|dlsym' <native file>
```

| native file | lines | Goose touchpoints |
|---|---|---|
| `llama-sampler.cpp` | 4832 | **38** |
| `llama-grammar.cpp` | 1923 | **6** |
| `llama-vocab.cpp` | 4947 | **3** |
| `llama-model-loader.cpp` | 1883 | **0** |
| `llama-graph.cpp` | 4315 | **0** |
| `llama-context.cpp` | 4781 | **0** |
| `llama-model.cpp` | 3999 | **0** |
| `llama-quant.cpp` | 1546 | **0** |

Plus one component wired at source level rather than through a door: the Q4_K tiled kernel, a static function in
`ggml-cpu/tiled/tiled.cpp` with its identity string in the built library.

## What the map says

Only the sampler family is substantially wired (38 touchpoints in one file). Grammar is partial (6), the
tokenizer has two doors and a hook (3 by this pattern). The loader, graph, context, model and quant files -
17 000 lines together - call Goose **nowhere**.

Meanwhile the accumulated unit carries 5389 lines of GGUF and loader Goose code (EXP-2026-11-19-1240) and
several thousand lines across other families.

## The distinction the ledger has been blurring

"Ported" and "called" are different states, and the roadmap's counters mix them:

- **ported**: the decision exists in Goose, has a gate, is composed into the unit;
- **called**: a native file that runs in the shipped binary invokes it on a live path;
- **replaced**: the native body is deleted because the Goose path is proven.

By these measures, of the sampler family the first two hold and the third awaits approval; of GGUF/loader only
the first holds; of graph/context/model/quant, the first holds in part and nothing more.

## What this changes about the "37 remaining items"

Several of them are not "port this decision" but "build the call path for a decision that already exists". That
is a different task with a different estimate, and P7-12 is the clearest case: it cannot be finished by porting.

## What is not measured here

Touchpoint counts are a proxy, not coverage: a file with 38 touchpoints can still have most of its work in
native code, and a file with 0 can be dead. The number that matters for the end state is rule 27's: no C++ at
all, which is still far away for every family except the sampler.
