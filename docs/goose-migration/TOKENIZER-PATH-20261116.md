# The tokenizer decisions are live in the hot path, not merely linked

Checked 2026-11-16 after committing the missing sources. The status line already says the eight `tok_*`
exports answer; this records something different and stronger: the decisions are **called by the tokenizer
loop that runs during a real request**, and they are switched at run time, not at compile time.

## How the switch actually works

`llama-vocab.cpp` does not use `#ifdef FG_GOOSE_TOKENIZER` (grep count: 0). It calls a runtime predicate:

```cpp
sym.n = fg202t_enabled()
    ? (size_t) fg202t_clamp_symbol_len((int64_t) len, (int64_t) (text.size() - offs))
    : std::min(len, text.size() - offs);
```

That matters because the S1.5 gate's `-DFG_GOOSE_TOKENIZER` path and the shipped runtime use **two
different mechanisms**. The gate proves the bridge functions equal the native text; the runtime does not
use that flag at all - it uses `fg202t_enabled()`, which is default-on through the same
`goose_on()` helper as every other bridge.

So the earlier conclusion "the flag is not wired into llama-vocab.cpp" was right about the flag and wrong
about the wiring: the wiring exists under a different mechanism. Corrected here.

## Witnessed on a real run

Apex, pinned argv, 32K prompt:

```
FG202T_TOKENIZER        first call calls=1
FG402S_BYTE_FALLBACK    first call calls=1
FG401S_DETOK_DECISIONS  calls=1000 ... 3000
```

`FG202T_TOKENIZER` firing with `calls=1` is the counter's shape, not evidence of one call: the object prints
a "first call" line and then only at intervals, and the decisions sit inside per-symbol and per-bigram
loops (`llama-vocab.cpp:150, 174, 214`) that run over every character of the prompt and every merge
candidate. The detok counter reaching 3000 in the same run shows the same mechanism under load.

## The three decisions and where they sit

| decision | line | loop |
|---|---:|---|
| `fg202t_clamp_symbol_len` | 150 | per symbol, over the prompt text |
| `fg202t_bigram_valid` | 174 | per bigram, over the merge candidates |
| `fg202t_bpe_merge_valid` / `add_allowed` / `ignore_merges` | 214 | per fragment during resegmentation |

## What is still missing for the standalone

The decisions answer, and in the integrated runtime they are on the tokenisation path. What the standalone
still lacks is the **engine around them**: reading a GGUF vocabulary, building the symbol list, driving the
merge loop, producing ids. That is the S1.5 stage's remaining scope, and it is a different piece of work
from the decisions - exactly the distinction the status line draws.

## Provenance

`fg202t.o` is built by `build.py`'s module discovery from
`experiments/EXP-2026-11-08-202-goose-tokenizer/source/bridge.c` and `vocab-generated.c`, both committed.
No hand-made object, unlike the MoE-cache bridges before EXP-756's tool.
