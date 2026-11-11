#!/usr/bin/env python3
"""Byte-fallback decisions (EXP402), applied to the staged llama-vocab.cpp."""
from pathlib import Path

V = Path(__file__).resolve().parent / 'src/llama-vocab.cpp'
s = V.read_text()

ANCHOR = 'extern "C" int fg202t_enabled(void);'
DECL = ('// Goose byte-fallback decisions (EXP402). GGML_GOOSE_BYTE_FALLBACK=0 restores the native path.\n'
        'extern "C" int fg402s_enabled(void);\n'
        'extern "C" int64_t fg402s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);\n\n')
assert ANCHOR in s
s = s.replace(ANCHOR, DECL + ANCHOR, 1)

old = '                if (token == LLAMA_TOKEN_NULL) {'
new = ('                if (fg402s_enabled() ? fg402s_decide(3, token == LLAMA_TOKEN_NULL ? 0 : 1, 0, 0, 0, 0, nullptr) != 0\n'
       '                                     : token == LLAMA_TOKEN_NULL) {')
assert old in s
s = s.replace(old, new, 1)

old = '                        if (tokenizer.byte_encode) {'
new = ('                        const int64_t g_path = fg402s_enabled()\n'
       '                            ? fg402s_decide(0, tokenizer.byte_encode ? 1 : 0, 0, 0, 0, 0, nullptr) : 1;\n'
       '                        if (g_path == 1) {')
assert old in s
s = s.replace(old, new, 1)

old = "                            const char buf[7] = { '<', '0', 'x', hex[ch >> 4], hex[ch & 15], '>', 0 };"
new = ("                            char buf[7] = { '<', '0', 'x',\n"
       "                                            fg402s_enabled() ? (char) fg402s_decide(1, (int64_t) ch, 0, 0, 0, 0, nullptr) : hex[ch >> 4],\n"
       "                                            fg402s_enabled() ? (char) fg402s_decide(1, (int64_t) ch, 1, 0, 0, 0, nullptr) : hex[ch & 15],\n"
       "                                            '>', 0 };")
assert old in s
s = s.replace(old, new, 1)

old = '                        if (token_multibyte != LLAMA_TOKEN_NULL) {'
new = ('                        if (fg402s_enabled() ? fg402s_decide(2, token_multibyte == LLAMA_TOKEN_NULL ? 0 : 1, 0, 0, 0, 0, nullptr) != 0\n'
       '                                             : token_multibyte != LLAMA_TOKEN_NULL) {')
assert old in s
s = s.replace(old, new, 1)

V.write_text(s)
print('byte fallback applied')
