#!/usr/bin/env python3
"""replay.py - hash-bound default-on hooks for common memory/IO.

Patches copies of inputs/llama-memory.cpp and inputs/llama-io.cpp under
common-memory-io/patched/; inputs are never mutated. Original bodies stay for
rollback (FG153_GOOSE_COMMON=0).
"""
import difflib, hashlib, json, os, sys

IROOT = '/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/helpers/omo5/inputs'
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "patched")
HASH = os.path.join(HERE, "source-hashes.json")
INPUTS = ["llama-memory.cpp", "llama-io.cpp"]

INC_ANCHOR = '#include "llama-io.h"'
INC_PAYLOAD = '\n#include "fg153-common-hooks.h"'

HOOKS = {
    "llama-memory.cpp": [
        ("llama_memory_status llama_memory_status_combine(",
         "\n    if (fg153_common_hooks_on()) { return fg153_common_hook_status_combine(s0, s1); }"),
        ("bool llama_memory_status_is_fail(",
         "\n    if (fg153_common_hooks_on()) { return fg153_common_hook_status_is_fail(status); }"),
        ("std::vector<ggml_backend_buffer_t> llama_memory_get_buffers(",
         "\n    if (fg153_common_hooks_on()) { return fg153_common_hook_get_buffers(mem); }"),
    ],
    "llama-io.cpp": [
        ("void llama_io_write_i::write_string(",
         "\n    if (fg153_common_hooks_on()) { fg153_common_hook_write_string(this, str); return; }"),
        ("void llama_io_read_i::read_string(",
         "\n    if (fg153_common_hooks_on()) { fg153_common_hook_read_string(this, str); return; }"),
    ],
}


def read(name):
    with open(os.path.join(IROOT, name), "r", encoding="utf-8") as f:
        return f.read()


def sha(name):
    h = hashlib.sha256()
    with open(os.path.join(IROOT, name), "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def record():
    data = {n: sha(n) for n in INPUTS}
    with open(HASH, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2); f.write("\n")
    print("recorded", HASH)


def verify(verbose=True):
    if not os.path.exists(HASH):
        print("NO RECORD: run record first", file=sys.stderr); return False
    want = json.load(open(HASH, encoding="utf-8"))
    ok = True
    for n in INPUTS:
        got = sha(n)
        if want.get(n) != got:
            print(f"HASH MISMATCH {n}: {want.get(n)} != {got}", file=sys.stderr); ok = False
        elif verbose:
            print("ok", n, got)
    return ok


def insert_after(text, anchor, payload, what):
    if payload in text:
        return text
    idx = text.find(anchor)
    if idx < 0:
        raise SystemExit("anchor missing: " + what)
    end = idx + len(anchor)
    return text[:end] + payload + text[end:]


def hook(text, prefix, payload, what):
    if payload in text:
        return text
    idx = text.find(prefix)
    if idx < 0:
        raise SystemExit("method missing: " + what)
    nl = text.find("\n", idx)
    brace = text.rfind("{", idx, nl if nl >= 0 else len(text))
    if brace < 0:
        raise SystemExit("body missing: " + what)
    pos = brace + 1
    return text[:pos] + payload + text[pos:]


def patched(name):
    t = read(name)
    t = insert_after(t, INC_ANCHOR, INC_PAYLOAD, name)
    for prefix, payload in HOOKS[name]:
        t = hook(t, prefix, payload, prefix)
    return t


def diff():
    for n in INPUTS:
        sys.stdout.write("".join(difflib.unified_diff(
            read(n).splitlines(True), patched(n).splitlines(True),
            fromfile="a/" + n, tofile="b/" + n)))


def emit():
    if not verify(False):
        sys.exit("refusing to emit: inputs not hash-bound")
    os.makedirs(OUT, exist_ok=True)
    for n in INPUTS:
        with open(os.path.join(OUT, n), "w", encoding="utf-8") as f:
            f.write(patched(n))
    for n in ("bridge.h", "services.cpp", "fg153-common-hooks.h", "fg153-common-hooks.cpp"):
        src = os.path.join(HERE, n)
        dst = os.path.join(OUT, n)
        with open(src, "r", encoding="utf-8") as f:
            body = f.read()
        with open(dst, "w", encoding="utf-8") as f:
            f.write(body)
    print("emitted", OUT)


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "verify"
    if cmd == "record":
        record()
    elif cmd == "verify":
        sys.exit(0 if verify() else 1)
    elif cmd == "diff":
        diff()
    elif cmd == "emit":
        emit()
    else:
        sys.exit("unknown " + cmd)
