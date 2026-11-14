#!/usr/bin/env python3
"""Mutation gate for the P8-07 S2 tool definitions (EXP586).

Each mutation is one semantic edit of source/tools-pure.goose. It must pass
`goose --standalone`, rebuild at -O0 and make the fixture fail. Usage:

    tests/mutate.py 0 6     # half 1: names, flags, validation
    tests/mutate.py 6 12    # half 2: result JSON and guards
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "tools-pure.goose")
WORK = os.path.join(ROOT, "build", "mut")
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

M = [
    # ---- half 1 -----------------------------------------------------------
    ("read_file is renamed",
     'return tp_put(out, 0, "read_file");',
     'return tp_put(out, 0, "read_files");'),
    ("exec loses the cwd flag",
     'if idx == TOOL_EXEC {\n        return 7;',
     'if idx == TOOL_EXEC {\n        return 5;'),
    ("read-only tools lose the cwd flag",
     'if idx == TOOL_READ_FILE || idx == TOOL_FILE_GLOB || idx == TOOL_GREP || idx == TOOL_GET_INFO {\n        return 2;',
     'if idx == TOOL_READ_FILE || idx == TOOL_FILE_GLOB || idx == TOOL_GREP || idx == TOOL_GET_INFO {\n        return 0;'),
    ("write tools lose the write flag",
     'if idx == TOOL_WRITE_FILE || idx == TOOL_EDIT_FILE {\n        return 3;',
     'if idx == TOOL_WRITE_FILE || idx == TOOL_EDIT_FILE {\n        return 2;'),
    ("a non-object request is accepted",
     'if root < 0 || jk[root] != PJ_K_OBJECT {',
     'if root < 0 || jk[root] == PJ_K_OBJECT {'),
    ("the exec timeout clamp is lifted",
     '    if t > LIMIT_EXEC_TIMEOUT {\n        st[10] = LIMIT_EXEC_TIMEOUT;',
     '    if t > LIMIT_EXEC_TIMEOUT + 1000 {\n        st[10] = t;'),
    # ---- half 2 -----------------------------------------------------------
    ("the error member is renamed",
     'var p = tp_put(out, 0, "{\\"error\\":\\"");',
     'var p = tp_put(out, 0, "{\\"err\\":\\"");'),
    ("the plain text member is renamed",
     'var p = tp_put(out, 0, "{\\"plain_text_response\\":\\"");',
     'var p = tp_put(out, 0, "{\\"plain_text\\":\\"");'),
    ("the write result loses a byte count",
     'p = tp_put(out, p, "\\",\\"bytes\\":");',
     'p = tp_put(out, p, "\\",\\"byte\\":");'),
    ("get_info renames cwd",
     'p = tp_put(out, p, "\\",\\"cwd\\":\\"");',
     'p = tp_put(out, p, "\\",\\"dir\\":\\"");'),
    ("the base64 result renames its member",
     'var p = tp_put(out, 0, "{\\"base64\\":\\"");',
     'var p = tp_put(out, 0, "{\\"b64\\":\\"");'),
    ("the stat error loses its wording",
     'p = tp_put(msg, 0, "cannot stat file: ");',
     'p = tp_put(msg, 0, "cannot stat: ");'),
]


def run(cmd, timeout=300):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    os.makedirs(WORK, exist_ok=True)
    with open(os.path.join(WORK, "jsonparse.goose"), "w", encoding="utf-8") as f:
        f.write(open(os.path.join(ROOT, "source", "jsonparse.goose"), encoding="utf-8").read())
    orig = open(SRC, encoding="utf-8").read()
    caught = missed = 0
    for idx in range(lo, hi):
        label, old, new = M[idx]
        if orig.count(old) != 1:
            print(f"[skip] {idx} {label}: anchor count {orig.count(old)}")
            continue
        (open(os.path.join(WORK, "tools-pure.goose"), "w", encoding="utf-8")
         .write(orig.replace(old, new, 1)))
        g = run(f'cd {WORK} && timeout 200 {GOOSE} --standalone -o g.c --header g.h tools-pure.goose')
        if g.returncode != 0:
            print(f"[{idx}] CAPTURE-BUILD {label}")
            caught += 1
            continue
        c = run(f'cd {WORK} && timeout 200 gcc-13 -O0 -std=c11 -w -c g.c -o g.o')
        l = run(f'cd {WORK} && timeout 200 g++-13 -O2 -std=c++17 -I {WORK} '
                f'{os.path.join(HERE, "tools_fixture.cpp")} g.o -o pfix')
        if c.returncode != 0 or l.returncode != 0:
            print(f"[{idx}] CAPTURE-LINK {label}")
            caught += 1
            continue
        r = run(f'cd {WORK} && timeout 120 ./pfix')
        ok = r.returncode != 0
        tail = (r.stdout.strip().splitlines() or ["no output"])[-1]
        print(f"[{'caught' if ok else 'MISSED'}] {idx} {label}: {tail}")
        caught += ok
        missed += (not ok)
    print(f"mutations {caught}/{hi - lo} caught (range {lo}:{hi}) missed={missed}")
    return 0 if caught == hi - lo else 1


if __name__ == "__main__":
    sys.exit(main())
