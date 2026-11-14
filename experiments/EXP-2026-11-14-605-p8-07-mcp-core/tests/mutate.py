#!/usr/bin/env python3
"""Mutation gate for the MCP session core (P8-07 S3). 12 edits in two halves."""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "mcp-core.goose")
GOOSE = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

M = [
    ("server order is not sorted", "        while j > 0 && mc_less(sb, noff[j], nlen[j], sb, noff[j - 1], nlen[j - 1]) {",
     "        while false && mc_less(sb, noff[j], nlen[j], sb, noff[j - 1], nlen[j - 1]) {"),
    ("a server without a command is kept", "        if cmd_l == 0 {\n            i = i + 1;\n        } else {",
     "        if false {\n            i = i + 1;\n        } else {"),
    ("the timeout default changes", '            let to = pget_int(p, id, "timeout_ms", 30000);',
     '            let to = pget_int(p, id, "timeout_ms", 1000);'),
    ("args are joined with a space", '                            pout = mput(out, pout, ",");',
     '                            pout = mput(out, pout, " ");'),
    ("env entries are joined with a comma", '                            pout = mput(out, pout, ";");',
     '                            pout = mput(out, pout, ",");'),
    ("an object error without message is dropped", "        if ml > 0 {\n            return mput(out, 0, sb[mo .. mo + ml]);\n        }",
     "        if false {\n            return mput(out, 0, sb[mo .. mo + ml]);\n        }"),
    ("text parts are concatenated without newlines", "                            if tn > 0 {\n                                text[tn] = 10;\n                                tn = tn + 1;\n                            }",
     "                            if false {\n                                text[tn] = 10;\n                                tn = tn + 1;\n                            }"),
    ("isError is ignored", "        if ie >= 0 && p.jk[ie] == PJ_K_BOOL && p.jv[ie] != 0 {",
     "        if false {"),
    ("the protocol version changes", '    p = mput(out, p, ",\\"jsonrpc\\":\\"2.0\\",\\"method\\":\\"initialize\\",\\"params\\":{\\"capabilities\\":{},\\"clientInfo\\":{\\"name\\":\\"llama.cpp\\",\\"version\\":\\"1.0\\"},\\"protocolVersion\\":\\"2024-11-05\\"}}");',
     '    p = mput(out, p, ",\\"jsonrpc\\":\\"2.0\\",\\"method\\":\\"initialize\\",\\"params\\":{\\"capabilities\\":{},\\"clientInfo\\":{\\"name\\":\\"llama.cpp\\",\\"version\\":\\"1.0\\"},\\"protocolVersion\\":\\"2024-11-04\\"}}");'),
    ("the call request renames its name member", '    p = mput(out, p, ",\\"name\\":\\"");',
     '    p = mput(out, p, ",\\"tool\\":\\"");'),
    ("the schema flag is inverted", '            if sc >= 0 {\n                pout = mput(out, pout, "y");\n            } else {\n                pout = mput(out, pout, "n");',
     '            if sc < 0 {\n                pout = mput(out, pout, "y");\n            } else {\n                pout = mput(out, pout, "n");'),
    ("cancelled reports a timeout", '        mput(out, 0, "{\\"error\\":{\\"code\\":-32603,\\"message\\":\\"cancelled\\"}}");',
     '        mput(out, 0, "{\\"error\\":{\\"code\\":-32603,\\"message\\":\\"request timed out\\"}}");'),
]


def run(cmd, timeout=300):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    orig = open(SRC, encoding="utf-8").read()
    caught = 0
    for idx in range(lo, hi):
        label, old, new = M[idx]
        if orig.count(old) != 1:
            print(f"[skip] {idx} {label}: anchor {orig.count(old)}")
            continue
        open(os.path.join(ROOT, "source", "m.goose"), "w", encoding="utf-8").write(orig.replace(old, new, 1))
        g = run(f"{GOOSE} --standalone -o {HERE}/m.c --header {HERE}/g.h {ROOT}/source/m.goose")
        if g.returncode != 0:
            print(f"[{idx}] BUILD-FAIL {label}")
            caught += 1
            continue
        c = run(f"gcc-13 -O0 -std=c11 -w -c {HERE}/m.c -o {HERE}/m.o")
        l = run(f"g++-13 -O1 -std=c++17 -I{HERE} -I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/vendor "
                f"{HERE}/fixture.cpp {HERE}/m.o -o {HERE}/mgate")
        if c.returncode or l.returncode:
            print(f"[{idx}] LINK-FAIL {label}")
            caught += 1
            continue
        r = run(f"{HERE}/mgate")
        ok = r.returncode != 0
        tail = (r.stdout.strip().splitlines() or ["no output"])[-1]
        print(f"[{'caught' if ok else 'MISSED'}] {idx} {label}: {tail}")
        caught += ok
    print(f"mutations {caught}/{hi - lo} caught (range {lo}:{hi})")
    return 0 if caught == hi - lo else 1


if __name__ == "__main__":
    sys.exit(main())
