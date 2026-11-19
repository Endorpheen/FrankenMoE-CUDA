#!/usr/bin/env python3
"""Vocabulary round-trip: HF tokenizer.json -> GGUF -> runtime llama_tokenize."""
import json
import os
import subprocess
import sys
import tempfile

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "experiments", "EXP-2026-11-17-927-maxwell-quant-convert", "tests"))
import test_convert as T

CONVERT = os.path.join(ROOT, "tools", "maxwell-convert.py")
LLAMA = "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda"
BUILD = "/home/end0/AI/LlamaServer/maxwell-build-20261117"
VB = 50
WORDS = ["h", "e", "l", "o", "w", "r", "d", "he", "ll", "hell", "hello",
         "wo", "wor", "worl", "world", "\u0120"]
IDS = {w: 4 + i for i, w in enumerate(WORDS)}
MERGES = ["h e", "l l", "he ll", "hell o", "w o", "wo r", "wor l", "worl d"]


def build_tokenizer_json(n_vocab):
    vocab = {"<unk>": 0, "<s>": 1, "</s>": 2, "<pad>": 3}
    vocab.update(IDS)
    for i in range(4 + len(WORDS), n_vocab):
        vocab[f"t{i}"] = i
    return {
        "model": {"type": "BPE", "vocab": vocab, "merges": MERGES},
        "pre_tokenizer": {"type": "ByteLevel", "add_prefix_space": False, "use_regex": True},
        "added_tokens": [
            {"id": 0, "content": "<unk>", "special": True},
            {"id": 1, "content": "<s>", "special": True},
            {"id": 2, "content": "</s>", "special": True},
            {"id": 3, "content": "<pad>", "special": True},
        ],
    }


def main():
    fails = []
    checks = 0

    def check(c, m):
        nonlocal checks
        checks += 1
        if not c:
            fails.append(m)
            print("FAIL", m)

    cfg = dict(T.CFG)
    cfg["vocab_size"] = VB
    rng = np.random.default_rng(3)
    tensors = T.build_tensors(rng, vocab=VB)

    with tempfile.TemporaryDirectory() as td:
        json.dump(cfg, open(os.path.join(td, "config.json"), "w"))
        json.dump(build_tokenizer_json(VB), open(os.path.join(td, "tokenizer.json"), "w"))
        open(os.path.join(td, "chat_template.jinja"), "w").write("{{ messages[0].role }}")
        shards = {f"model-0000{i}.safetensors": [] for i in (1, 2, 3)}
        idx = {"weight_map": {}}
        for k, (n, a) in enumerate(tensors):
            shards[list(shards)[k % 3]].append((n, a))
            idx["weight_map"][n] = list(shards)[k % 3]
        for fn, items in shards.items():
            T.write_safetensors(os.path.join(td, fn), items)
        json.dump(idx, open(os.path.join(td, "model.safetensors.index.json"), "w"))

        out = os.path.join(td, "v.gguf")
        r = subprocess.run([sys.executable, CONVERT, td, out], capture_output=True, text=True)
        check(r.returncode == 0, f"convert rc={r.returncode} {r.stderr[-300:]}")
        if r.returncode != 0:
            return report(checks, fails)
        check("pre=gpt-2" in r.stdout, f"pre type not detected: {r.stdout.strip()}")
        check("merges=8" in r.stdout, f"merges count: {r.stdout.strip()}")
        rd = T.Reader(out)
        check(rd.kv.get("tokenizer.chat_template") == "{{ messages[0].role }}", "chat template key")

        probe = os.path.join(BUILD, "tokenize_probe")
        cc = subprocess.run(["g++-13", "-O2", "-std=c++17", f"-I{LLAMA}/include",
                             f"-I{LLAMA}/ggml/include",
                             os.path.join(ROOT, "experiments", "EXP-2026-11-17-930-maxwell-vocab",
                                          "tests", "tokenize_probe.cpp"),
                             f"-L{LLAMA}/build-cuda/bin", "-lllama", "-lggml", "-lggml-base",
                             "-lggml-cpu", f"-Wl,-rpath,{LLAMA}/build-cuda/bin", "-o", probe],
                            capture_output=True, text=True)
        check(cc.returncode == 0, f"probe build: {cc.stderr[-400:]}")
        if cc.returncode != 0:
            return report(checks, fails)

        r = subprocess.run([probe, out, "hello", "world"], capture_output=True, text=True)
        print(r.stdout)
        check(r.returncode == 0, f"probe rc={r.returncode} {r.stderr[-300:]}")
        check(f"vocab={VB}" in r.stdout, "vocab size")
        check(f"ids={IDS['hello']} " in r.stdout, f"hello -> {IDS['hello']}")
        check(f"ids={IDS['world']} " in r.stdout, f"world -> {IDS['world']}")

    return report(checks, fails)


def report(checks, fails):
    print(f"checks={checks} failures={len(fails)}")
    print("MAXWELL-VOCAB OK" if not fails else "MAXWELL-VOCAB MISMATCH")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
