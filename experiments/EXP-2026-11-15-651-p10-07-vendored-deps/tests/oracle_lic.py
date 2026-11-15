#!/usr/bin/env python3
"""P10-07 S1 oracle: the native licence payload, executed by CMake itself.

Every expectation is produced by running the byte-identical copy of
tests/native/license.cmake in `cmake -P` script mode: license_add_file() and
license_generate() are the native functions, the global property store is
CMake's, and the resulting bytes are written verbatim to a file by
`file(WRITE ...)` so no message formatting is involved.

Nothing here re-types a native rule. Two script-mode accommodations are made
and both are asserted, not assumed:

  1. `define_property(GLOBAL PROPERTY LICENSE_TEXT ...)` is not scriptable, so
     the declaration block is removed; the property itself is created by the
     first set_property().
  2. `license_generate()` ends with `message(FATAL_ERROR "Target '...' does not
     exist")` when no such target exists, which cannot exist in script mode, so
     for the body case `message` is shadowed by a function that swallows the
     call; the file is written by the native code before that point.

The warning and fatal texts are read out of the native source with a regex, so
a change in license.cmake changes both arms on the next run.

    python3 oracle_lic.py <cases.tsv> <license.cmake> <fixtures-dir>

Output: one `<row>\\t<hex-or-int>` line per case, in file order.
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

CASES = Path(sys.argv[1]).resolve()
NATIVE_IN = Path(sys.argv[2]).resolve()
FIX = Path(sys.argv[3]).resolve()
NATIVE = NATIVE_IN / "license.cmake" if NATIVE_IN.is_dir() else NATIVE_IN

LICENSE_CMAKE = NATIVE.read_text()

m = re.search(r'message\(\s*WARNING\s+"([^"]*not found)"\s*\)', LICENSE_CMAKE)
if not m:
    sys.exit("oracle: warning format not found in the native license.cmake")
WARN_FMT = m.group(1)

mf = re.search(r'message\(\s*FATAL_ERROR\s+"([^"]*does not exist)"\s*\)', LICENSE_CMAKE)
if not mf:
    sys.exit("oracle: fatal target format not found in the native license.cmake")
FATAL_FMT = mf.group(1)

SCRIPTABLE = re.sub(
    r"define_property\(GLOBAL PROPERTY LICENSE_TEXT.*?\n\)\n", "", LICENSE_CMAKE, flags=re.S
)
if SCRIPTABLE == LICENSE_CMAKE:
    sys.exit("oracle: define_property block not found in the native license.cmake")

# Shadow message() so the FATAL_ERROR at the end of license_generate() cannot
# abort a script-mode run; the native code writes the file before that call.
MESSAGE_SHADOW = """function(message)
endfunction()
"""


def unescape(field: str) -> str:
    out = []
    i = 0
    while i < len(field):
        if field.startswith("~512~", i):
            out.append("x" * 512)
            i += 5
        elif field[i] == "\\" and i + 1 < len(field):
            c = field[i + 1]
            if c == "x" and i + 3 < len(field):
                out.append(chr(int(field[i + 2:i + 4], 16)))
                i += 4
                continue
            out.append({"n": "\n", "t": "\t", "r": "\r", "\\": "\\"}.get(c, c))
            i += 2
        else:
            out.append(field[i])
            i += 1
    return "".join(out)


HEXOUT = "hexout"


def emit(var: str) -> str:
    return f'string(HEX "${{{var}}}" HX)\nfile(WRITE "${{HEXOUT}}" "${{HX}}")\n'


def run_cmake(body: str, workdir: Path, shadow_message: bool = False) -> str:
    lic = workdir / "license-scriptable.cmake"
    lic.write_text(SCRIPTABLE)
    script = workdir / "case.cmake"
    script.write_text(
        f'set(HEXOUT "{workdir / HEXOUT}")\n'
        + (MESSAGE_SHADOW if shadow_message else "")
        + 'include("' + str(lic).replace("\\", "/") + '")\n'
        + body
        + "\n"
    )
    r = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True, cwd=str(workdir))
    if r.returncode != 0:
        sys.exit(f"oracle: cmake failed: {r.stdout}\n{r.stderr}")
    out = workdir / HEXOUT
    if not out.exists():
        sys.exit(f"oracle: case produced no value: {body!r}\n{r.stdout}\n{r.stderr}")
    return out.read_text()


def value(kind: str, fields: list[str]) -> str:
    with tempfile.TemporaryDirectory(prefix="p1007-") as td:
        wd = Path(td)
        if kind == "title":
            (wd / "name").write_bytes(unescape(fields[1]).encode("latin-1"))
            return run_cmake(
                'file(READ name NAME)\nset(TITLE "License for ${NAME}")\n' + emit("TITLE"), wd
            )
        if kind == "underline":
            (wd / "name").write_bytes(unescape(fields[1]).encode("latin-1"))
            return run_cmake(
                'file(READ name NAME)\nset(TITLE "License for ${NAME}")\n'
                'string(REGEX REPLACE "." "=" UNDERLINE "${TITLE}")\n' + emit("UNDERLINE"),
                wd,
            )
        if kind == "entry":
            (wd / "name").write_bytes(unescape(fields[1]).encode("latin-1"))
            (wd / "src").write_text(str((FIX / fields[2]).as_posix()))
            return run_cmake(
                "file(READ name NAME)\nfile(READ src SRC)\n"
                'license_add_file("${NAME}" "${SRC}")\n'
                "get_property(TMP GLOBAL PROPERTY LICENSE_TEXT)\n" + emit("TMP"),
                wd,
            )
        if kind == "body":
            body = ""
            for i, spec in enumerate(fields[1].split("|")):
                if not spec:
                    continue
                name, fn = spec.split(":", 1)
                (wd / f"name{i}").write_text(name)
                (wd / f"src{i}").write_text(str((FIX / fn).as_posix()))
                body += (
                    f"file(READ name{i} NAME)\nfile(READ src{i} SRC)\n"
                    'license_add_file("${NAME}" "${SRC}")\n'
                )
            body += (
                'license_generate("unused")\n'
                'file(READ "${CMAKE_BINARY_DIR}/license.cpp" OUT)\n' + emit("OUT")
            )
            return run_cmake(body, wd, shadow_message=True)
        if kind in ("resolve", "missing"):
            (wd / "base").write_bytes(unescape(fields[1]).encode("latin-1"))
            (wd / "rel").write_bytes(unescape(fields[2]).encode("latin-1"))
            body = (
                "file(READ base B)\nfile(READ rel F)\n"
                'set(CMAKE_CURRENT_SOURCE_DIR "${B}")\n'
                'set(FILE "${F}")\n'
                'if(NOT IS_ABSOLUTE "${FILE}")\n'
                '    set(FILE "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")\n'
                "endif()\n"
            )
            if kind == "resolve":
                return run_cmake(body + emit("FILE"), wd)
            pre, post = WARN_FMT.split("${FILE}")
            body += (
                'set(MSG "' + pre + '")\n'
                'string(APPEND MSG "${FILE}")\n'
                'string(APPEND MSG "' + post + '")\n' + emit("MSG")
            )
            return run_cmake(body, wd)
        if kind == "no_target":
            (wd / "name").write_bytes(unescape(fields[1]).encode("latin-1"))
            pre, post = FATAL_FMT.split("${TARGET_NAME}")
            body = (
                'file(READ name NAME)\n'
                'set(MSG "' + pre + '")\n'
                'string(APPEND MSG "${NAME}")\n'
                'string(APPEND MSG "' + post + '")\n' + emit("MSG")
            )
            return run_cmake(body, wd)
        if kind == "file_text":
            (wd / "raw").write_bytes(unescape(fields[1]).encode("latin-1"))
            return run_cmake(
                "file(READ raw T)\n" + emit("T"), wd
            )
        if kind == "absolute":
            (wd / "path").write_bytes(unescape(fields[1]).encode("latin-1"))
            return run_cmake(
                'file(READ path P)\nset(R 0)\nif(IS_ABSOLUTE "${P}")\n    set(R 1)\nendif()\n'
                'file(WRITE "${HEXOUT}" "${R}")\n',
                wd,
            )
    raise SystemExit(f"oracle: unknown case kind {kind!r}")


def main() -> int:
    row = 0
    out = []
    for line in CASES.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        row += 1
        f = line.split("\t")
        out.append(f"{row}\t{value(f[0], f)}")
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
