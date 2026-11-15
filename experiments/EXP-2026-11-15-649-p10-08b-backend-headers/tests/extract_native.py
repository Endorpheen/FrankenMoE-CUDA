#!/usr/bin/env python3
"""P10-08 backend/GGUF/SONAME oracle.

Derives the native side of three gates on every build from the base-commit
sources and from the reference shared objects. Nothing here is a hand
transcription of a value that a compiler could resolve:

  bkh  - the split-axis switch in ggml/src/ggml-backend-meta.cpp
  ggh  - GGUF_TYPE_NAME / GGUF_TYPE_SIZE in ggml/src/gguf.cpp
  son  - the version variables in the three CMakeLists, the SONAME / symlink
         chain / DT_NEEDED entries read out of the reference .so files with
         readelf, the dynamic symbol table with nm, the public declarations
         parsed out of the five headers, the export-macro expansion obtained by
         running the real preprocessor over the real headers, and the ledger
         counts from docs/goose-migration/abi-ledger.json

Usage: extract_native.py <llama.cpp checkout> <output dir> [repo root]
Env:   FG_SO_DIR (reference shared objects), CC/CXX for the preprocessor,
       FG_LEDGER (default <repo>/docs/goose-migration/abi-ledger.json)
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HEADERS = {
    "llama": ("include/llama.h", "LLAMA_API"),
    "gguf": ("ggml/include/gguf.h", "GGML_API"),
    "backend": ("ggml/include/ggml-backend.h", "GGML_API"),
    "opt": ("ggml/include/ggml-opt.h", "GGML_API"),
    "alloc": ("ggml/include/ggml-alloc.h", "GGML_API"),
}

# Reference shared objects, stem order matches the port's lib table.
LIBS = [
    ("llama", "libllama.so.0.3.0"),
    ("llama-common", "libllama-common.so.0.3.0"),
    ("mtmd", "libmtmd.so.0.3.0"),
    ("ggml", "libggml.so.0.22.0"),
    ("ggml-base", "libggml-base.so.0.22.0"),
    ("ggml-cpu", "libggml-cpu.so.0.22.0"),
    ("ggml-cuda", "libggml-cuda.so.0.22.0"),
]


def die(msg: str) -> None:
    print(f"oracle: {msg}", file=sys.stderr)
    raise SystemExit(1)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def c_str(s: str) -> str:
    out = s.replace("\\", "\\\\").replace('"', '\\"')
    return '"' + out + '"'


# ── bkh: the split-axis switch ──────────────────────────────────────────────
def split_axes(native: Path) -> list[tuple[int, str]]:
    src = strip_comments((native / "ggml/src/ggml-backend-meta.cpp").read_text())
    m = re.search(
        r"ggml_backend_meta_split_axis_name\s*\([^)]*\)\s*\{(.*?)\n\}", src, re.S
    )
    if not m:
        die("ggml_backend_meta_split_axis_name not found")
    body = m.group(1)
    rows = re.findall(
        r"case\s+([A-Z0-9_]+)\s*:\s*return\s*\"([^\"]*)\"\s*;", body
    )
    # Resolve the enumerator values from the header, not from the source.
    header = strip_comments((native / "ggml/include/ggml-backend.h").read_text())
    enum_vals: dict[str, int] = {}
    m2 = re.search(r"enum ggml_backend_meta_split_axis\s*\{(.*?)\}", header, re.S)
    if not m2:
        die("enum ggml_backend_meta_split_axis not found")
    nxt = 0
    for item in m2.group(1).split(","):
        item = item.strip()
        if not item or item.startswith("//"):
            continue
        if "=" in item:
            name, val = item.split("=", 1)
            name = name.strip()
            val = val.split("//")[0].strip()
            nxt = int(val, 0)
        else:
            name = item
        enum_vals[name] = nxt
        nxt += 1
    out = []
    for name, text in rows:
        if name not in enum_vals:
            die(f"{name} missing from the split-axis enum")
        out.append((enum_vals[name], text))
    if len(out) != 8:
        die(f"split-axis switch has {len(out)} cases, expected 8")
    if set(enum_vals) - {n for n, _ in rows}:
        die("split-axis enum has a member with no name text")
    return out


# ── ggh: the two gguf type maps ─────────────────────────────────────────────
def gguf_types(native: Path) -> list[tuple[int, str, int]]:
    src = strip_comments((native / "ggml/src/gguf.cpp").read_text())
    header = strip_comments((native / "ggml/include/gguf.h").read_text())
    enum_vals: dict[str, int] = {}
    body = re.search(r"enum gguf_type\s*\{(.*?)\}", header, re.S)
    if not body:
        die("enum gguf_type not found")
    nxt = 0
    for item in body.group(1).split(","):
        item = item.strip()
        if not item:
            continue
        if "=" in item:
            name, val = item.split("=", 1)
            name = name.strip()
            nxt = int(val.split("//")[0].strip(), 0)
        else:
            name = item
        enum_vals[name] = nxt
        nxt += 1

    names: dict[str, str] = {}
    m = re.search(r"GGUF_TYPE_NAME\s*=\s*\{(.*?)\n\};", src, re.S)
    if not m:
        die("GGUF_TYPE_NAME not found")
    for name, text in re.findall(r"\{([A-Z0-9_]+),\s*\"([^\"]*)\"\}", m.group(1)):
        names[name] = text

    sizes: dict[str, str] = {}
    m = re.search(r"GGUF_TYPE_SIZE\s*=\s*\{(.*?)\n\};", src, re.S)
    if not m:
        die("GGUF_TYPE_SIZE not found")
    for name, expr in re.findall(r"\{([A-Z0-9_]+),\s*([^}]+?)\}", m.group(1)):
        sizes[name] = expr.strip()

    c_sizes = {
        "sizeof(uint8_t)": 1,
        "sizeof(int8_t)": 1,
        "sizeof(uint16_t)": 2,
        "sizeof(int16_t)": 2,
        "sizeof(uint32_t)": 4,
        "sizeof(int32_t)": 4,
        "sizeof(float)": 4,
        "sizeof(uint64_t)": 8,
        "sizeof(int64_t)": 8,
        "sizeof(double)": 8,
        "0": 0,
    }
    rows = []
    for name, val in sorted(enum_vals.items(), key=lambda kv: kv[1]):
        if name == "GGUF_TYPE_COUNT":
            continue
        if name not in names:
            die(f"{name} has no name text")
        if name not in sizes:
            die(f"{name} has no size entry")
        if sizes[name] not in c_sizes:
            die(f"{name} size expression {sizes[name]} is not a plain sizeof")
        rows.append((val, names[name], c_sizes[sizes[name]]))
    if len(rows) != 13:
        die(f"{len(rows)} gguf types, expected 13")
    if enum_vals.get("GGUF_TYPE_COUNT") != 13:
        die("GGUF_TYPE_COUNT is not 13")
    return rows


# ── son: CMake versions ─────────────────────────────────────────────────────
def cmake_version(native: Path, path: str, prefix: str) -> tuple[int, int, int]:
    text = (native / path).read_text()
    vals = []
    for part in ("MAJOR", "MINOR", "PATCH"):
        m = re.search(rf"set\(\s*{prefix}_VERSION_{part}\s+(\d+)\s*\)", text)
        if not m:
            die(f"{prefix}_VERSION_{part} not found in {path}")
        vals.append(int(m.group(1)))
    return tuple(vals)  # type: ignore[return-value]


def readelf_field(path: Path, field: str) -> list[str]:
    """Return the bracketed values of one dynamic-section tag."""
    out = subprocess.run(
        ["readelf", "-d", str(path)], capture_output=True, text=True, check=True
    ).stdout
    vals = []
    for line in out.splitlines():
        if f"({field})" not in line:
            continue
        m = re.search(r"\[(.*)\]", line)
        if m:
            vals.append(m.group(1))
    return vals


def dynamic_symbols(path: Path) -> set[str]:
    out = subprocess.run(
        ["nm", "-D", "--defined-only", str(path)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return {line.split()[-1] for line in out.splitlines() if line.strip()}


def public_decls(native: Path, header: str, macro: str) -> list[str]:
    text = strip_comments((native / HEADERS[header][0]).read_text())
    names: list[str] = []
    for m in re.finditer(rf"\b{macro}\b\s+([^;{{}}]*?)\(", text):
        ids = re.findall(r"[A-Za-z_]\w*", m.group(1))
        if not ids:
            continue
        name = ids[-1]
        # macro-definition lines and attribute tokens are not declarations
        if name.startswith("__") or name in ("DEPRECATED", "define"):
            continue
        if name not in names:
            names.append(name)
    return names


def macro_expansions(native: Path, cc: str) -> list[tuple[int, int, int]]:
    """(macro id, shared, outcome code) from the real preprocessor."""
    ggml_inc = native / "ggml/include"
    llama_inc = native / "include"
    probe = (
        '#include "ggml-backend.h"\n'
        "GGML_API int p_ggml;\n"
        "GGML_BACKEND_API int p_backend;\n"
    )
    probe_llama = '#include "llama.h"\nLLAMA_API int p_llama;\n'
    rows: list[tuple[int, int, int]] = []
    tmp = Path(os.environ.get("TMPDIR", "/tmp")) / "fg651-oracle"
    tmp.mkdir(parents=True, exist_ok=True)
    p1 = tmp / "p_ggml.c"
    p2 = tmp / "p_llama.c"
    p1.write_text(probe)
    p2.write_text(probe_llama)
    try:
        for shared in (0, 1):
            flags = ["-DGGML_SHARED"] if shared else []
            if shared:
                flags.append("-DGGML_BACKEND_SHARED")
            text = subprocess.run(
                [cc, "-E", "-P", *flags, f"-I{ggml_inc}", str(p1)],
                capture_output=True,
                text=True,
                check=True,
            ).stdout
            for marker, mid, token in (
                ("p_ggml", 0, "GGML_API"),
                ("p_backend", 1, "GGML_BACKEND_API"),
            ):
                line = next(
                    (ln for ln in text.splitlines() if marker in ln), None
                )
                if line is None:
                    die(f"expansion of {token} not found")
                code = 0
                if "visibility" in line:
                    code = 1
                rows.append((mid, shared, code))
            lflags = ["-DLLAMA_SHARED"] if shared else []
            ltext = subprocess.run(
                [cc, "-E", "-P", *lflags, f"-I{llama_inc}", f"-I{ggml_inc}", str(p2)],
                capture_output=True,
                text=True,
                check=True,
            ).stdout
            line = next((ln for ln in ltext.splitlines() if "p_llama" in ln), None)
            if line is None:
                die("expansion of LLAMA_API not found")
            code = 2
            if "visibility" in line:
                code = 3
            rows.append((2, shared, code))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return rows


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    env_native = os.environ.get("FG_NATIVE")
    native = Path(env_native or sys.argv[1]).resolve()
    out = Path(sys.argv[2]).resolve()
    repo = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else native.parent
    so_dir = Path(
        os.environ.get("FG_SO_DIR", repo / "build/exp054-dflash/bin")
    ).resolve()
    ledger_path = Path(
        os.environ.get("FG_LEDGER", repo / "docs/goose-migration/abi-ledger.json")
    ).resolve()
    cc = os.environ.get("CC", "gcc-13")

    out.mkdir(parents=True, exist_ok=True)

    axes = split_axes(native)
    types = gguf_types(native)
    ggml_ver = cmake_version(native, "ggml/CMakeLists.txt", "GGML")
    llama_ver = cmake_version(native, "CMakeLists.txt", "LLAMA")
    macros = macro_expansions(native, cc)

    decls = {name: public_decls(native, name, HEADERS[name][1]) for name in HEADERS}

    lib_facts = []
    for stem, fname in LIBS:
        path = so_dir / fname
        if not path.exists():
            die(f"reference object missing: {path} (set FG_SO_DIR)")
        soname = readelf_field(path, "SONAME")
        needed = readelf_field(path, "NEEDED")
        if len(soname) != 1:
            die(f"{fname} has {len(soname)} SONAME entries")
        lib_facts.append(
            {
                "stem": stem,
                "file": fname,
                "soname": soname[0],
                "needed": needed,
                "syms": dynamic_symbols(path),
            }
        )

    # symlink chain, read from the directory itself
    chains = []
    for stem, fname in LIBS:
        ver = fname[len(f"lib{stem}.so."):]
        sov = ver.split(".")[0]
        short = so_dir / f"lib{stem}.so"
        mid = so_dir / f"lib{stem}.so.{sov}"
        if not short.is_symlink() or not mid.is_symlink():
            die(f"{stem}: expected symlinks lib{stem}.so and lib{stem}.so.{sov}")
        chains.append((short.readlink().name, mid.readlink().name))

    exports: set[str] = set()
    for fact in lib_facts:
        exports |= fact["syms"]
    legacy = sorted(
        s for s in lib_facts[0]["syms"] if s.startswith("llama_") and s not in set(decls["llama"])
    )
    unresolved = {
        name: [d for d in decls[name] if d not in exports] for name in HEADERS
    }
    allow = sorted({d for miss in unresolved.values() for d in miss})

    ledger = json.loads(ledger_path.read_text())

    # ── write the C tables ──────────────────────────────────────────────────
    h = [
        "/* Generated by tests/extract_native.py. Do not edit. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "typedef struct { int64_t val; const char * text; } nat_txt;",
        "typedef struct { int64_t val; const char * text; int64_t size; } nat_type;",
        "typedef struct { const char * lib; const char * name; } nat_named;",
        "typedef struct { int64_t macro_id; int64_t shared; int64_t code; } nat_macro;",
        "typedef struct { const char * header; const char * name; } nat_decl;",
        "",
        f"#define NAT_AXIS_N {len(axes)}",
        "extern const nat_txt NAT_AXIS[NAT_AXIS_N];",
        f"#define NAT_GGH_TYPE_N {len(types)}",
        "extern const nat_type NAT_GGH_TYPE[NAT_GGH_TYPE_N];",
        f"#define NAT_LIB_N {len(lib_facts)}",
        "extern const char * const NAT_LIB_FILE[NAT_LIB_N];",
        "extern const char * const NAT_LIB_SONAME[NAT_LIB_N];",
        "extern const char * const NAT_LIB_NEEDED[NAT_LIB_N];",
        "extern const char * const NAT_LIB_CHAIN_SHORT[NAT_LIB_N];",
        "extern const char * const NAT_LIB_CHAIN_MID[NAT_LIB_N];",
        "extern const char * const NAT_LIB_SYM[NAT_LIB_N];",
        f"#define NAT_SO_DIR {c_str(str(so_dir))}",
        f"#define NAT_GGML_VERSION {c_str('.'.join(map(str, ggml_ver)))}",
        f"#define NAT_LLAMA_VERSION {c_str('.'.join(map(str, llama_ver)))}",
        f"#define NAT_GGML_SOVERSION {ggml_ver[0]}",
        f"#define NAT_LLAMA_SOVERSION {llama_ver[0]}",
        f"#define NAT_MACRO_N {len(macros)}",
        "extern const nat_macro NAT_MACRO[NAT_MACRO_N];",
        "extern const char * const NAT_MACRO_NAME[3];",
        f"#define NAT_DECL_N {sum(len(v) for v in decls.values())}",
        "extern const nat_decl NAT_DECL[NAT_DECL_N];",
        f"#define NAT_UNRESOLVED_N {len(allow)}",
        "extern const char * const NAT_UNRESOLVED[NAT_UNRESOLVED_N];",
        f"#define NAT_LEGACY_N {len(legacy)}",
        "extern const char * const NAT_LEGACY[NAT_LEGACY_N];",
        f"#define NAT_LEDGER_EXPORTS {ledger['exports']['count']}",
        f"#define NAT_LEDGER_DECLS {ledger['declarations']['count']}",
        f"#define NAT_LEDGER_IMPORTS {ledger['primitive_imports']['count']}",
        f"#define NAT_LEDGER_STRUCTS {ledger['size_contracts']['structs_crossing_the_abi']}",
        f"#define NAT_LEDGER_LIB {c_str(Path(ledger['library']).name)}",
        "",
    ]

    c = [
        "/* Generated by tests/extract_native.py. Do not edit. */",
        '#include "native_abi.h"',
        "",
        "const nat_txt NAT_AXIS[NAT_AXIS_N] = {",
    ]
    for val, text in axes:
        c.append(f"    {{ {val}, {c_str(text)} }},")
    c += ["};", "", "const nat_type NAT_GGH_TYPE[NAT_GGH_TYPE_N] = {"]
    for val, text, size in types:
        c.append(f"    {{ {val}, {c_str(text)}, {size} }},")
    c += ["};", ""]
    c.append("const char * const NAT_LIB_FILE[NAT_LIB_N] = {")
    c += [f"    {c_str(f['file'])}," for f in lib_facts]
    c.append("};")
    c.append("const char * const NAT_LIB_SONAME[NAT_LIB_N] = {")
    c += [f"    {c_str(f['soname'])}," for f in lib_facts]
    c.append("};")
    c.append("const char * const NAT_LIB_NEEDED[NAT_LIB_N] = {")
    for f in lib_facts:
        c.append(f"    {c_str('|'.join(f['needed']))},")
    c.append("};")
    c.append("const char * const NAT_LIB_CHAIN_SHORT[NAT_LIB_N] = {")
    c += [f"    {c_str(a)}," for a, _ in chains]
    c.append("};")
    c.append("const char * const NAT_LIB_CHAIN_MID[NAT_LIB_N] = {")
    c += [f"    {c_str(b)}," for _, b in chains]
    c.append("};")
    c.append("const char * const NAT_LIB_SYM[NAT_LIB_N] = {")
    for f in lib_facts:
        c.append(f"    {c_str('|'.join(sorted(f['syms'])))},")
    c.append("};")
    c.append("")
    c.append("const nat_macro NAT_MACRO[NAT_MACRO_N] = {")
    for mid, shared, code in macros:
        c.append(f"    {{ {mid}, {shared}, {code} }},")
    c.append("};")
    c.append("")
    c.append("const char * const NAT_MACRO_NAME[3] = { \"GGML_API\", \"GGML_BACKEND_API\", \"LLAMA_API\" };")
    c.append("")
    c.append("const nat_decl NAT_DECL[NAT_DECL_N] = {")
    for name in HEADERS:
        for d in decls[name]:
            c.append(f"    {{ {c_str(HEADERS[name][0].split(chr(47))[-1])}, {c_str(d)} }},")
    c.append("};")
    c.append("")
    c.append("const char * const NAT_UNRESOLVED[NAT_UNRESOLVED_N] = {")
    c += [f"    {c_str(n)}," for n in allow]
    c.append("};")
    c.append("const char * const NAT_LEGACY[NAT_LEGACY_N] = {")
    c += [f"    {c_str(n)}," for n in legacy]
    c.append("};")
    c.append("")

    (out / "native_abi.h").write_text("\n".join(h))
    (out / "native_abi.c").write_text("\n".join(c))

    print(
        f"axes={len(axes)} gguf_types={len(types)} libs={len(lib_facts)} "
        f"decls={sum(len(v) for v in decls.values())} unresolved={len(allow)} "
        f"legacy={len(legacy)} macros={len(macros)}"
    )
    print(
        f"ggml_version={'.'.join(map(str, ggml_ver))} "
        f"llama_version={'.'.join(map(str, llama_ver))} "
        f"so_dir={so_dir}"
    )
    if allow:
        print("unresolved allowance: " + ", ".join(allow))
    return 0


if __name__ == "__main__":
    sys.exit(main())
