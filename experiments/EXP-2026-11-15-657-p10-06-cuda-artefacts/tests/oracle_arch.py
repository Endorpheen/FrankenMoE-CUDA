#!/usr/bin/env python3
"""Native oracle for P10-06 S1 (CUDA architecture list policy).

The native text is executed, not re-typed:

    default    ggml-cuda/CMakeLists.txt:8-57, run by `cmake -P` with
               CUDAToolkit_VERSION / GGML_NATIVE / CMAKE_VERSION supplied;
               the resulting CMAKE_CUDA_ARCHITECTURES is read back
    rewrite    CMakeLists.txt:75-92, run by `cmake -P` on a preset list
    final      CMakeLists.txt:8-57 + 75-92 + 94-99, run by `cmake -P`
    native_ok  the CMakeLists.txt:97 list regex, applied by `cmake -P`
    valid      the same regex or its element form, applied by `cmake -P`
    kind       CMakeLists.txt:21-23 suffix table, parsed from the comment text
    sm         CMakeLists.txt:10-19 arch table, parsed from the comment text

Usage: oracle_arch.py CASES
Results: "<row>\\t<value>"; integers decimal, text hex of the raw bytes.
"""

from __future__ import annotations

import re
import subprocess
import sys

sys.dont_write_bytecode = True
import os
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
NATIVE = HERE / "native"
TEXT = (NATIVE / "ggml-cuda-CMakeLists.txt").read_text()
LINES = TEXT.splitlines()


OPENERS = ("if (", "foreach(", "while(", "function(", "macro(")
CLOSERS = ("endif", "endforeach", "endwhile", "endfunction", "endmacro")


def _block(start: str) -> str:
    """Source text of the block that starts with `start`, tracked by a nesting
    stack over the native if/foreach/while/function keywords."""
    i = next(k for k, line in enumerate(LINES) if line.strip().startswith(start))
    depth = 0
    out: list[str] = []
    for line in LINES[i:]:
        s = line.strip()
        out.append(line)
        if s.startswith(CLOSERS):
            depth -= 1
            if depth == 0:
                return "\n".join(out) + "\n"
        elif s.startswith(OPENERS):
            depth += 1
    raise SystemExit("unterminated block: " + start)


BLOCK_A = _block("if (NOT DEFINED CMAKE_CUDA_ARCHITECTURES)")
BLOCK_B = _block("foreach(ARCHS IN ITEMS")
BLOCK_C = _block("if (CMAKE_CUDA_ARCHITECTURES STREQUAL")

LIST_RE = re.search(r"MATCHES \"(\^\[0-9\][^\"]*)\"", BLOCK_C).group(1)


def cmk(script: str, defines: dict[str, str]) -> str:
    with tempfile.NamedTemporaryFile("w", suffix=".cmake", delete=False) as f:
        f.write(script)
        path = f.name
    args = ["cmake"]
    for k, v in defines.items():
        args.append(f"-D{k}={v}")
    args += ["-P", path]
    try:
        p = subprocess.run(args, capture_output=True, text=True, check=False)
        return p.stdout + p.stderr
    finally:
        os.unlink(path)


def cmk_value(script: str, defines: dict[str, str], marker: str) -> str:
    out = cmk(script, defines)
    m = re.search(re.escape(marker) + r"\[(.*?)\]", out, re.S)
    if not m:
        raise SystemExit(f"cmake produced no {marker}: {out}")
    return m.group(1)


def ver(major: str, minor: str) -> str:
    return f"{major}.{minor}"


def do_default(f: list[str]) -> str:
    major, minor, native, cm, cn = f[1], f[2], f[3], f[4], f[5]
    script = f'set(CMAKE_VERSION "{ver(cm, cn)}")\n' + BLOCK_A
    script += 'message(STATUS "ARCH=[${CMAKE_CUDA_ARCHITECTURES}]")\n'
    return cmk_value(script, {"CUDAToolkit_VERSION": ver(major, minor),
                              "GGML_NATIVE": native}, "ARCH=")


def do_rewrite(f: list[str]) -> str:
    script = f'set(CMAKE_CUDA_ARCHITECTURES "{f[1]}")\n' + BLOCK_B
    script += 'message(STATUS "ARCH=[${CMAKE_CUDA_ARCHITECTURES}]")\n'
    return cmk_value(script, {}, "ARCH=")


def do_final(f: list[str]) -> str:
    major, minor, native, cm, cn, native_value = f[1], f[2], f[3], f[4], f[5], f[6]
    script = f'set(CMAKE_VERSION "{ver(cm, cn)}")\n' + BLOCK_A + BLOCK_B + BLOCK_C
    script += 'message(STATUS "ARCH=[${CMAKE_CUDA_ARCHITECTURES}]")\n'
    return cmk_value(script, {"CUDAToolkit_VERSION": ver(major, minor),
                              "GGML_NATIVE": native,
                              "CMAKE_CUDA_ARCHITECTURES_NATIVE": native_value}, "ARCH=")


def truth(script: str) -> str:
    out = cmk(script + 'if (_m)\nmessage(STATUS "OK=[1]")\nelse()\nmessage(STATUS "OK=[0]")\nendif()\n', {})
    return "1" if "OK=[1]" in out else "0"


def do_native_ok(f: list[str]) -> str:
    return truth(f'set(_s "{f[1]}")\nif (_s MATCHES "{LIST_RE}")\nset(_m 1)\nelse()\nset(_m 0)\nendif()\n')


def do_valid(f: list[str]) -> str:
    """The native list pattern. For a token without ';' the list pattern is
    exactly the element predicate, which is what list_mode 0 means."""
    text = f[1]
    return truth(f'set(_s "{text}")\nif (_s MATCHES "{LIST_RE}")\nset(_m 1)\nelse()\nset(_m 0)\nendif()\n')


def do_kind(f: list[str]) -> str:
    """Suffix table from the comment block applied to the case token. The
    suffix is detected by longest match against the table keys, independent of
    the port's own implementation."""
    table: dict[str, int] = {}
    for line in LINES:
        m = re.match(r"#\s+(XX-virtual|XX-real|no suffix)\s+==\s+(.*)$", line.strip())
        if not m:
            continue
        word, body = m.group(1), m.group(2)
        if "both PTX and device code" in body:
            table[""] = 0
        elif "as PTX" in body:
            table["-virtual"] = 2
        elif "as device code" in body:
            table["-real"] = 1
    if not table:
        raise SystemExit("suffix table not found in the native comment")
    token = f[1]
    for suf in sorted((k for k in table if k), key=len, reverse=True):
        if token.endswith(suf) and len(token) > len(suf):
            return str(table[suf])
    return str(table[""])


def do_sm(f: list[str]) -> str:
    """Arch table from the comment block: `# 89 == RTX 4000 ...`."""
    token = f[1]
    table = {}
    for line in LINES:
        m = re.match(r"#\s+(\d+)\s+==", line.strip())
        if m:
            table[m.group(1)] = int(m.group(1))
    num = re.match(r"(\d+)", token)
    if not num:
        return "-1"
    if num.group(1) not in table:
        return "?"
    return str(table[num.group(1)])


HANDLERS = {
    "default": do_default,
    "rewrite": do_rewrite,
    "final": do_final,
    "native_ok": do_native_ok,
    "valid": do_valid,
    "kind": do_kind,
    "sm": do_sm,
}


def enc(value: str) -> str:
    return value.encode().hex()


def main() -> int:
    cases = Path(sys.argv[1])
    row = 0
    for line in cases.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        row += 1
        f = line.split("\t")
        kind = f[0]
        value = HANDLERS[kind](f)
        if kind in ("default", "rewrite", "final"):
            print(f"{row}\t{enc(value)}")
        else:
            print(f"{row}\t{value}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
