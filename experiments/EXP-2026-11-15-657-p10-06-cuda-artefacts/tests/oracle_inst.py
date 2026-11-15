#!/usr/bin/env python3
"""Native oracle for P10-06 S3 (install and dist layout of the CUDA backend).

The native text is executed by a real CMake configure, build and install:

    kind       ggml/src/CMakeLists.txt:379-424, target TYPE read back
    dest       the same function, the installed file's directory read back
    files      the same function, `cmake --install` writes a real prefix and
               the relative paths are listed
    versioned  the same function, the VERSION property read back
    privdefs   target COMPILE_DEFINITIONS
    pubdefs    target INTERFACE_COMPILE_DEFINITIONS
    avail      GGML_AVAILABLE_BACKENDS after one call, pre-seeded from -D
    version    ggml/CMakeLists.txt:6-9,31 evaluated by `cmake -P`
    guard      ggml/src/CMakeLists.txt:366-371, configure must fail

Case fields are positional:
    kind      dl shared
    dest      dl dir_set
    files     dl shared dir
    versioned dl apple
    privdefs  dl shared
    pubdefs   shared
    avail     existing
    version   which (version|soversion)
    guard     dl dir_set

Usage: oracle_inst.py CASES
Results: "<row>\\t<value>"; text is hex of the raw bytes.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
NATIVE = HERE / "native"
SRC = (NATIVE / "ggml-src-CMakeLists.txt").read_text()
GGML = (NATIVE / "ggml-CMakeLists.txt").read_text()
LINES = SRC.splitlines()


def _func(name: str) -> str:
    i = next(k for k, l in enumerate(LINES) if l.startswith(f"function({name}"))
    j = next(k for k in range(i, len(LINES)) if LINES[k].strip() == "endfunction()")
    return "\n".join(LINES[i:j + 1]) + "\n"


def _guard() -> str:
    i = next(k for k, l in enumerate(LINES) if l.strip().startswith("if (GGML_BACKEND_DIR)"))
    depth = 0
    for j in range(i, len(LINES)):
        s = LINES[j].strip()
        if s.startswith("if ("):
            depth += 1
        elif s.startswith("endif"):
            depth -= 1
            if depth == 0:
                return "\n".join(LINES[i:j + 1]) + "\n"
    raise SystemExit("unterminated GGML_BACKEND_DIR guard")


BACKEND_FN = _func("ggml_add_backend_library")
GUARD = _guard()

GL = GGML.splitlines()
_I0 = next(k for k, l in enumerate(GL) if l.startswith("set(GGML_VERSION_MAJOR"))
_I1 = next(k for k, l in enumerate(GL) if l.startswith("set(GGML_VERSION_BASE"))
_I2 = next(k for k, l in enumerate(GL) if l.startswith('set(GGML_VERSION "'))
VERSION_BLOCK = "\n".join(GL[_I0:_I1 + 1]) + "\n" + GL[_I2] + "\n"

PROJECT = """cmake_minimum_required(VERSION 3.20)
project(gate C)
include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/bin")
set(APPLE {apple})
set(GGML_VERSION "@VERSION@")
set(GGML_VERSION_MAJOR @MAJOR@)
add_library(ggml STATIC dummy.c)
add_library(ggml-base INTERFACE)
unset(GGML_AVAILABLE_BACKENDS CACHE)
if (NOT GATE_AVAIL STREQUAL "")
  set(GGML_AVAILABLE_BACKENDS "${{GATE_AVAIL}}" CACHE INTERNAL "seeded" FORCE)
endif()
{guard}
{fn}
ggml_add_backend_library(ggml-cuda cuda.c)
get_target_property(_t ggml-cuda TYPE)
get_target_property(_v ggml-cuda VERSION)
get_target_property(_d ggml-cuda COMPILE_DEFINITIONS)
get_target_property(_i ggml-cuda INTERFACE_COMPILE_DEFINITIONS)
file(WRITE "${{GATE_OUT}}/type.txt" "${{_t}}")
file(WRITE "${{GATE_OUT}}/version.txt" "${{_v}}")
file(WRITE "${{GATE_OUT}}/defs.txt" "${{_d}}")
file(WRITE "${{GATE_OUT}}/idefs.txt" "${{_i}}")
file(WRITE "${{GATE_OUT}}/avail.txt" "${{GGML_AVAILABLE_BACKENDS}}")
"""

TYPES = {"STATIC_LIBRARY": "0", "SHARED_LIBRARY": "1", "MODULE_LIBRARY": "2"}

BACKEND_DIR = "libexec/ggml"


class Work:
    """One synthetic project, one build directory, one install prefix."""

    def __init__(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="fg660-inst-"))
        self.proj = self.root / "proj"
        self.proj.mkdir()
        (self.proj / "dummy.c").write_text("int f(void){return 0;}\n")
        (self.proj / "cuda.c").write_text("int g(void){return 1;}\n")
        self.out = self.root / "out"
        self.out.mkdir()
        self.build = self.root / "build"
        self.prefix = self.root / "prefix"

    def configure(self, apple: str, opts: dict[str, str]) -> subprocess.CompletedProcess:
        text = (PROJECT.format(apple=apple, guard=GUARD, fn=BACKEND_FN)
                .replace("@VERSION@", "0.22.0").replace("@MAJOR@", "0"))
        (self.proj / "CMakeLists.txt").write_text(text)
        args = ["cmake", "-S", str(self.proj), "-B", str(self.build),
                f"-DGATE_OUT={self.out}", f"-DCMAKE_INSTALL_PREFIX={self.prefix}",
                "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_INSTALL_BINDIR=bin"]
        args += [f"-D{k}={v}" for k, v in opts.items()]
        return subprocess.run(args, capture_output=True, text=True, check=False)

    def installed(self) -> list[str]:
        shutil.rmtree(self.prefix, ignore_errors=True)
        p = subprocess.run(["cmake", "--build", str(self.build), "-j2"],
                           capture_output=True, text=True, check=False)
        if p.returncode != 0:
            raise SystemExit("native build failed:\n" + p.stdout + p.stderr)
        p = subprocess.run(["cmake", "--install", str(self.build)],
                           capture_output=True, text=True, check=False)
        if p.returncode != 0:
            raise SystemExit("native install failed:\n" + p.stdout + p.stderr)
        return sorted(str(f.relative_to(self.prefix)) for f in self.prefix.rglob("*")
                      if f.is_file() or f.is_symlink())

    def read(self, name: str) -> str:
        """A target property that was never set reads back as <name>-NOTFOUND."""
        value = (self.out / name).read_text().strip()
        return "" if value.endswith("-NOTFOUND") else value

    def cleanup(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)


def version_oracle() -> tuple[str, str]:
    with tempfile.NamedTemporaryFile("w", suffix=".cmake", delete=False) as f:
        f.write(VERSION_BLOCK + 'message(STATUS "V=[${GGML_VERSION}] M=[${GGML_VERSION_MAJOR}]")\n')
        path = f.name
    try:
        p = subprocess.run(["cmake", "-P", path], capture_output=True, text=True, check=False)
    finally:
        os.unlink(path)
    m = re.search(r"V=\[([^\]]*)\] M=\[([^\]]*)\]", p.stdout)
    if not m:
        raise SystemExit("version block produced no value:\n" + p.stdout + p.stderr)
    return m.group(1), m.group(2)


def options(kind: str, f: list[str]) -> tuple[str, dict[str, str]]:
    """Translate one case row into the configure inputs of the native function."""
    apple = "0"
    opts = {"GGML_BACKEND_DL": "0", "BUILD_SHARED_LIBS": "1", "GGML_BACKEND_DIR": "",
            "GATE_AVAIL": ""}
    if kind in ("kind", "dest", "files", "privdefs", "guard"):
        opts["GGML_BACKEND_DL"] = f[1]
    if kind == "pubdefs":
        opts["BUILD_SHARED_LIBS"] = f[1]
    if kind in ("kind", "privdefs", "dest", "files"):
        opts["BUILD_SHARED_LIBS"] = f[2]
    if kind == "versioned":
        apple = f[2]
        opts["GGML_BACKEND_DL"] = f[1]
    if kind == "dest" and f[2] == "1":
        opts["GGML_BACKEND_DIR"] = BACKEND_DIR
    if kind == "files" and f[3] == BACKEND_DIR:
        opts["GGML_BACKEND_DIR"] = BACKEND_DIR
    if kind == "avail":
        opts["GATE_AVAIL"] = f[1]
    if kind == "guard":
        if f[2] == "1":
            opts["GGML_BACKEND_DIR"] = BACKEND_DIR
    return apple, opts


def main() -> int:
    cases = Path(sys.argv[1])
    rows = [l.split("\t") for l in cases.read_text().splitlines() if l and not l.startswith("#")]
    work = Work()
    try:
        for n, f in enumerate(rows, 1):
            kind = f[0]
            if kind == "version":
                full, major = version_oracle()
                value = full if f[1] == "version" else major
                print(f"{n}\t{value.encode().hex()}")
                continue
            apple, opts = options(kind, f)
            p = work.configure(apple, opts)
            if kind == "guard":
                value = "1" if p.returncode != 0 else "0"
                print(f"{n}\t{value.encode().hex()}")
                continue
            if p.returncode != 0:
                raise SystemExit(f"row {n}: unexpected configure failure\n" + p.stdout + p.stderr)
            if kind == "kind":
                value = TYPES[work.read("type.txt").strip()]
            elif kind == "versioned":
                value = "0" if work.read("version.txt").strip() in ("", "_v-NOTFOUND") else "1"
            elif kind == "privdefs":
                value = ";".join(x for x in work.read("defs.txt").split(";") if x)
            elif kind == "pubdefs":
                value = ";".join(x for x in work.read("idefs.txt").split(";") if x)
            elif kind == "avail":
                value = work.read("avail.txt")
            elif kind == "dest":
                parent = Path(work.installed()[0]).parent.as_posix()
                value = "0" if parent == "lib" else ("1" if parent == "bin" else "2")
            elif kind == "files":
                value = ";".join(work.installed())
            else:
                raise SystemExit("unknown kind: " + kind)
            print(f"{n}\t{value.encode().hex()}")
    finally:
        work.cleanup()
    return 0


if __name__ == "__main__":
    sys.exit(main())
