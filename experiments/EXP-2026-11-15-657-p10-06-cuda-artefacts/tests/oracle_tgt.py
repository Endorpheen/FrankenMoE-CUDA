#!/usr/bin/env python3
"""Native oracle for P10-06 S2 (CUDA compile flags, defines and link set).

The native text is executed by a real CMake configure, not re-typed:

    flags   ggml-cuda/CMakeLists.txt:195-266 included into a synthetic project;
            COMPILE_OPTIONS is read back and the
            $<$<COMPILE_LANGUAGE:CUDA>:...> wrapper is unwrapped
    final   the same block with the host-compiler branch enabled; the host flag
            text comes from a ggml_get_flags() shim and a fake CUDA compiler
            that prints a fixed version, so the branch is deterministic
    defs    CMakeLists.txt:131-156 plus the NCCL and debug defines; the
            COMPILE_DEFINITIONS directory property is read back
    link    CMakeLists.txt:157-193 with imported cuda/NCCL targets; the
            LINK_LIBRARIES property is read back

Usage: oracle_tgt.py CASES
Results: "<row>\\t<value>"; text is hex of the raw bytes.
"""

from __future__ import annotations

import re
import shutil
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


def _at(prefix: str) -> int:
    return next(k for k, line in enumerate(LINES) if line.strip().startswith(prefix))


# The three native sections in source order: defines, link selection (which
# also carries the NCCL define), then CUDA_FLAGS with target_compile_options.
_A = _at("add_compile_definitions(GGML_CUDA_PEER_MAX_BATCH_SIZE")
_B = _at("if (GGML_STATIC)")
_C = _at("set(CUDA_CXX_FLAGS")
_D = _at("target_compile_options(ggml-cuda")
BLOCK_DEFS = "\n".join(LINES[_A:_B]) + "\n"
BLOCK_LINK = "\n".join(LINES[_B:_C]) + "\n"
BLOCK_FLAGS = "\n".join(LINES[_C:_D + 1]) + "\n"

PROJECT = """cmake_minimum_required(VERSION 3.20)
project(gate C)
include(GNUInstallDirs)
set(CMAKE_CUDA_COMPILER "${{GATE_CC}}")
set(WIN32 {win})
function(ggml_get_flags id ver)
    set(CXX_FLAGS "-Wno-unused-parameter ${{id}}-${{ver}}" PARENT_SCOPE)
    set(GF_CXX_FLAGS "-Wno-format-truncation" PARENT_SCOPE)
endfunction()
add_library(ggml-base INTERFACE)
add_library(ggml-cuda STATIC dummy.c)
foreach(t CCCL::CCCL CUDA::cudart CUDA::cudart_static CUDA::cublas CUDA::cublas_static
          CUDA::cublasLt_static CUDA::cuda_driver)
    add_library(${{t}} INTERFACE IMPORTED)
endforeach()
@DEFS@
@LINK@
@FLAGS@
get_target_property(_o ggml-cuda COMPILE_OPTIONS)
get_directory_property(_d COMPILE_DEFINITIONS)
get_target_property(_l ggml-cuda LINK_LIBRARIES)
file(WRITE "${{GATE_OUT}}/flags.txt" "${{_o}}")
string(REPLACE ";" "\\n" _dj "${{_d}}")
string(REPLACE ";" "\\n" _lj "${{_l}}")
file(WRITE "${{GATE_OUT}}/defs.txt" "${{_dj}}")
file(WRITE "${{GATE_OUT}}/link.txt" "${{_lj}}")
"""

FIND_NCCL = """set(NCCL_FOUND FALSE)
if (GATE_NCCL)
    set(NCCL_FOUND TRUE)
    if (NOT TARGET NCCL::NCCL)
        add_library(NCCL::NCCL INTERFACE IMPORTED)
    endif()
endif()
"""

FAKE_CC = "#!/bin/sh\necho \"12.3.0\"\n"


class Work:
    """One synthetic project tree plus one reusable build directory."""

    def __init__(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="fg660-tgt-"))
        self.proj = self.root / "proj"
        self.proj.mkdir()
        (self.proj / "dummy.c").write_text("int f(void){return 0;}\n")
        (self.proj / "FindNCCL.cmake").write_text(FIND_NCCL)
        self.out = self.root / "out"
        self.out.mkdir()
        self.cc = self.root / "fakecc"
        self.cc.write_text(FAKE_CC)
        self.cc.chmod(0o755)
        self.build = self.root / "build"
        self.base = [f"-DGATE_OUT={self.out}", f"-DGATE_CC={self.cc}",
                     f"-DCMAKE_MODULE_PATH={self.proj}"]

    def configure(self, win: str, options: dict[str, str]) -> None:
        text = (PROJECT.format(win=win).replace("@DEFS@", BLOCK_DEFS)
                .replace("@LINK@", BLOCK_LINK).replace("@FLAGS@", BLOCK_FLAGS))
        (self.proj / "CMakeLists.txt").write_text(text)
        args = ["cmake", "-S", str(self.proj), "-B", str(self.build)] + self.base
        args += [f"-D{k}={v}" for k, v in options.items()]
        p = subprocess.run(args, capture_output=True, text=True, check=False)
        if p.returncode != 0:
            raise SystemExit("native configure failed:\n" + p.stdout + p.stderr)

    def read(self, name: str) -> list[str]:
        text = (self.out / name).read_text()
        return text.split("\n")

    def cleanup(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)


def base_opts(row: dict[str, str]) -> dict[str, str]:
    """Every option the included native blocks test, always explicit so that a
    cache value from a previous configure cannot leak into the next case."""
    return {
        "GGML_CUDA_DEBUG": row.get("debug", "0"),
        "GGML_FATAL_WARNINGS": row.get("fatal", "0"),
        "GGML_ALL_WARNINGS": row.get("all_warn", "0"),
        "GGML_CUDA_COMPRESSION_MODE": row.get("mode", "speed"),
        "GGML_CUDA_PEER_MAX_BATCH_SIZE": row.get("peer", "64"),
        "GGML_CUDA_GRAPHS": "0",
        "GGML_CUDA_FORCE_MMQ": "0",
        "GGML_CUDA_FORCE_CUBLAS": "0",
        "GGML_CUDA_NO_VMM": row.get("novmm", "0"),
        "GGML_CUDA_FA": row.get("fa", "1"),
        "GGML_CUDA_NO_PEER_COPY": "0",
        # tgt_link takes the single "NCCL requested and found" bit, which in the
        # native text is GGML_CUDA_NCCL && NCCL_FOUND
        "GGML_CUDA_NCCL": row.get("nccl", row.get("nccl_found", "0")),
        "GATE_NCCL": row.get("nccl_found", "0"),
        "GGML_STATIC": row.get("static", "0"),
        "GGML_CUDA_CUB_3DOT2": row.get("cub", "0"),
        "CUDAToolkit_VERSION": row.get("cuda", "12.8"),
        "MSVC": row.get("msvc", "0"),
    }


XCOMPILER = re.search(r"list\(APPEND CUDA_FLAGS (-Xcompiler) ", BLOCK_FLAGS).group(1)


def strip_xcompiler(value: str) -> str:
    """The pre--Xcompiler prefix of the native flag list. The cut element is the
    literal the native text itself appends, read from that text."""
    parts = value.split(";")
    if XCOMPILER in parts:
        return ";".join(parts[: parts.index(XCOMPILER)])
    return value


def unwrap_flags(raw: str) -> str:
    """Strip the $<$<COMPILE_LANGUAGE:CUDA>:...> wrapper the native text adds."""
    m = re.match(r"^\$<\$<COMPILE_LANGUAGE:CUDA>:(.*)>$", raw, re.S)
    if not m:
        raise SystemExit("unexpected COMPILE_OPTIONS shape: " + raw[:120])
    return m.group(1)


def main() -> int:
    cases = Path(sys.argv[1])
    rows: list[dict[str, str]] = []
    for line in cases.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        f = line.split("\t")
        row = {"kind": f[0]}
        if f[0] == "flags":
            row.update(major=f[1], minor=f[2], debug=f[3], fatal=f[4], mode=f[5])
        elif f[0] == "final":
            row.update(major=f[1], minor=f[2], debug=f[3], fatal=f[4], mode=f[5],
                       msvc=f[6], all_warn=f[7], host=f[8] if len(f) > 8 else "")
        elif f[0] == "defs":
            row.update(peer=f[1], graphs=f[2], mmq=f[3], cublas=f[4], novmm=f[5],
                       fa=f[6], nopeer=f[7], nccl=f[8], nccl_found=f[9], debug=f[10])
        elif f[0] == "link":
            row.update(static=f[1], win=f[2], major=f[3], minor=f[4], novmm=f[5],
                       nccl_found=f[6], cub=f[7])
        else:
            raise SystemExit("unknown kind: " + f[0])
        row["cuda"] = f"{row.get('major', '12')}.{row.get('minor', '8')}"
        rows.append(row)

    work = Work()
    try:
        for n, row in enumerate(rows, 1):
            opts = base_opts(row)
            if row["kind"] in ("defs",):
                opts.update(GGML_CUDA_GRAPHS=row["graphs"], GGML_CUDA_FORCE_MMQ=row["mmq"],
                            GGML_CUDA_FORCE_CUBLAS=row["cublas"],
                            GGML_CUDA_NO_PEER_COPY=row["nopeer"])
            work.configure(row.get("win", "0"), opts)
            if row["kind"] == "flags":
                value = strip_xcompiler(unwrap_flags(work.read("flags.txt")[0]))
            elif row["kind"] == "final":
                value = unwrap_flags(work.read("flags.txt")[0])
            elif row["kind"] == "defs":
                value = ";".join(x for x in work.read("defs.txt") if x)
            else:
                value = ";".join(x for x in work.read("link.txt") if x)
            print(f"{n}\t{value.encode().hex()}")
    finally:
        work.cleanup()
    return 0


if __name__ == "__main__":
    sys.exit(main())
