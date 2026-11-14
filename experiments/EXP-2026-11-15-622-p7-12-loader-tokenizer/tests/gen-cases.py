#!/usr/bin/env python3
"""Build the S1 corpus from the native pre-tokenizer chain.

The pre-tokenizer names are read out of src/llama-vocab.cpp, so a name that the
native dispatch knows can never be missing from the corpus. Near misses are
added on top: an unknown name must be refused, and a name that differs only in
case, by a trailing blank or by a digit must not be accepted.

Columns: model pre has_merges merges_kv_array merges_arr_string n_merges
         charsmap_present charsmap_kv_array charsmap_i8u8 charsmap_len
         charsmap_blob
"""
import re
import sys
from pathlib import Path

NATIVE = Path(sys.argv[1])
OUT = Path(sys.argv[2])

text = NATIVE.read_text()
start = text.index('if (tokenizer_pre.empty()) {')
end = text.index('throw std::runtime_error(format("unknown pre-tokenizer type', start)
chain = text[start:end]
names = []
for m in re.finditer(r'tokenizer_pre\s*==\s*"([^"]*)"', chain):
    if m.group(1) not in names:
        names.append(m.group(1))

NEAR = ["", "llama4", "qwen2.5", "KIMI-K2", "kimi-k2 ", "default ", "gemma",
        "gemma4x", "flint2", "jina-v5", "glm4", "glm5x", "x", "0", "qwen35x"]

MODELS = ["no_vocab", "none", "llama", "bert", "gpt2", "hybriddna", "whitespace",
          "t5", "rwkv", "test", "plamo2", "plamo3", "gemma4",
          "unknown", "", "Llama", "gpt-2", "bert2"]

# has_merges, merges_kv_array, merges_arr_string, n_merges,
# charsmap_present, charsmap_kv_array, charsmap_i8u8, charsmap_len, charsmap_blob
CLEAN = (1, 1, 1, 0, 0, 1, 1, 8, 0)

rows = []
seen = set()


def add(model, pre, cfg):
    key = (model, pre) + tuple(cfg)
    if key in seen:
        return
    seen.add(key)
    rows.append(key)


for model in MODELS:
    for pre in ["none"] + names + NEAR:
        add(model, pre, CLEAN)

# merges shapes: missing, wrong key type, wrong array type, and 1..4 entries.
for model in MODELS:
    for pre in ["none", "kimi-k2", "llama3", "gemma4", "llama-3", "KIMI-K2"]:
        for cfg in [(0, 1, 1, 0, 0, 1, 1, 8, 0),
                    (1, 0, 1, 0, 0, 1, 1, 8, 0),
                    (1, 1, 0, 0, 0, 1, 1, 8, 0),
                    (1, 1, 1, 1, 0, 1, 1, 8, 0),
                    (1, 1, 1, 4, 0, 1, 1, 8, 0)]:
            add(model, pre, cfg)

# t5 charsmap shapes, including the size-header boundary and error precedence.
for model in ["t5", "gpt2", "llama"]:
    for pre in ["none", "default"]:
        for present, kvarray, i8u8, ln, blob in [
                (0, 1, 1, 8, 0),
                (1, 0, 1, 8, 0),
                (1, 1, 0, 8, 0),
                (1, 1, 1, 0, 0),
                (1, 1, 1, 1, 0),
                (1, 1, 1, 3, 0),
                (1, 1, 1, 4, 0),
                (1, 1, 1, 5, 0),
                (1, 1, 1, 8, 3),
                (1, 1, 1, 8, 4),
                (1, 1, 1, 8, 60),
                (1, 1, 1, 64, 100)]:
            add(model, pre, (1, 1, 1, 0, present, kvarray, i8u8, ln, blob))
        # both keys bad at once: the merge error comes first
        add(model, pre, (0, 0, 0, 0, 1, 0, 0, 2, 0))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open('w') as f:
    for r in rows:
        f.write('\t'.join(str(x) for x in r) + '\n')
print(f'{len(rows)} cases, {len(names)} native pre names, {len(MODELS)} models -> {OUT}')
