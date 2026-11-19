#!/usr/bin/env python3
"""A text-prompt request of acceptance size, so the tokenizer is part of the measurement.

The acceptance request carries token ids, which means no tokenizer runs there and its hash can say nothing about one
(EXP-2026-11-19-1394). This builds the same shape of request with text instead: about 32 000 tokens for Apex.
"""
import json
import sys

out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/req_text.json"
para = ("The history of the printing press begins with movable type in fifteenth-century Europe. "
        "Its effects on literacy, science and religion were profound and long-lasting. ")
text = (para * 1400)[:128000]
with open(out, "w") as f:
    json.dump({"prompt": text, "n_predict": 512, "temperature": 0, "seed": 11}, f)
print(f"wrote {out}: {len(text)} characters, about 32 000 tokens for Apex")
