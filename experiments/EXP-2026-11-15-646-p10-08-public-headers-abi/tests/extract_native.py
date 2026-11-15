#!/usr/bin/env python3
"""Derive the native ABI oracle from the llama.cpp checkout at the base commit.

The three slices of this experiment freeze facts that live in two places:

  * public headers (``include/llama.h``, ``ggml/include/ggml.h``,
    ``ggml/include/ggml-backend.h``) -- the fixture compiles those directly, so
    no extraction is needed for their macros and enum values;
  * tables in the implementation files (``ggml/src/ggml.c``,
    ``src/llama.cpp``, ``src/llama-model.cpp``,
    ``src/llama-model-loader.cpp``) -- those files are far too heavy to link
    into a model-free gate, so this script parses the tables out of them and
    emits C data tables. The fixture resolves the remaining compile-time
    expressions (``sizeof(block_q4_K)``, ``QK_K``, enum constants) against the
    real headers.

Nothing here is transcribed by hand: every string, block size and status code
is read from the checkout on each gate build, so a change in the canon changes
the oracle and the gate fails instead of agreeing with a stale copy.

Usage:
    python3 tests/extract_native.py <llama.cpp-root> <out-dir>

Writes ``native_abi.h``, ``native_nm.h``, ``native_limits.h`` into <out-dir>.
Every parser asserts its own expected shape; a silent partial parse is a hard
error, never an empty table.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

RE_ENTRY = re.compile(r"^\s*\[([^\]]+)\]\s*=\s*\{\s*(?://.*)?$")
RE_TYPENAME = re.compile(r'^\s*\.type_name\s*=\s*"((?:[^"\\]|\\.)*)"\s*,\s*$')
RE_BLCK = re.compile(r"^\s*\.blck_size\s*=\s*([A-Za-z0-9_]+)\s*,\s*$")
RE_TSIZE = re.compile(r"^\s*\.type_size\s*=\s*(sizeof\([A-Za-z0-9_]+\)|[A-Za-z0-9_]+)\s*,\s*$")
RE_ISQ = re.compile(r"^\s*\.is_quantized\s*=\s*(true|false)\s*,\s*$")
RE_CASE_RET = re.compile(r'^\s*case\s+([A-Za-z0-9_]+):\s*return\s+"((?:[^"\\]|\\.)*)"\s*;\s*$')
RE_CASE_NAME = re.compile(
    r'^\s*case\s+([A-Za-z0-9_]+):\s*name\s*=\s*LLAMA_FTYPE_PREFIX\s+"((?:[^"\\]|\\.)*)"\s*;\s*break;\s*$'
)
RE_DEFAULT_NAME = re.compile(
    r'^\s*default:\s*name\s*=\s*LLAMA_FTYPE_PREFIX\s+"((?:[^"\\]|\\.)*)"\s*;\s*break;\s*$'
)
RE_CASE_NULL = re.compile(r"^\s*default:\s*return\s+nullptr;\s*$")
RE_STRCMP = re.compile(
    r'^\s*if\s*\(\s*std::strcmp\(str,\s*"((?:[^"\\]|\\.)*)"\)\s*==\s*0\s*\)\s*\{\s*'
    r"return\s+([A-Za-z0-9_]+);\s*\}\s*$"
)
RE_FTYPE_PREFIX = re.compile(r'^#define\s+LLAMA_FTYPE_PREFIX\s+"((?:[^"\\]|\\.)*)"\s*$')
RE_SIMPLE_RETURN = re.compile(r"^size_t\s+%s\(void\)\s*\{\s*\n\s*return\s+(\d+);\s*\n\}\s*$", re.M)


class ExtractError(RuntimeError):
    pass


def fail(msg: str) -> None:
    raise ExtractError(msg)


def slice_between(text: str, start_marker: str, end_marker: str, col0: bool = True) -> tuple[str, int]:
    """Return the text after start_marker up to the next closing line.

    With col0 the terminator must start in column 0, which is how a function or
    a file-scope table ends; nested blocks close with indentation and are
    skipped. Header enums are indented, so they pass col0=False.
    """
    at = text.find(start_marker)
    if at < 0:
        fail(f"start marker not found: {start_marker!r}")
    at += len(start_marker)
    pos = at
    while True:
        nl = text.find("\n", pos)
        if nl < 0:
            fail(f"end marker not found for {start_marker!r}")
        line = text[pos:nl]
        if line.strip() == end_marker and (not col0 or line == end_marker):
            ln = text[:at].count("\n") + 1
            return text[at:pos], ln
        pos = nl + 1


def parse_type_traits(src: str) -> list[tuple[str, str | None, str, str, bool]]:
    body, line0 = slice_between(
        src, "static const struct ggml_type_traits type_traits[GGML_TYPE_COUNT] = {\n", "};"
    )
    entries: list[tuple[str, str | None, str, str, bool]] = []
    cur: dict | None = None
    for line in body.splitlines():
        m = RE_ENTRY.match(line)
        if m:
            if cur is not None:
                fail("nested entry in type_traits")
            cur = {"idx": m.group(1), "name": None, "blck": None, "size": None, "isq": None}
            continue
        if cur is None:
            if line.strip() in ("",):
                continue
            fail(f"unexpected line outside an entry: {line!r}")
        if line.strip() == "},":
            for key in ("name", "blck", "size", "isq"):
                if cur[key] is None:
                    fail(f"entry [{cur['idx']}] lacks {key}")
            entries.append((cur["idx"], cur["name"], cur["blck"], cur["size"], cur["isq"]))
            cur = None
            continue
        m = RE_TYPENAME.match(line)
        if m:
            cur["name"] = m.group(1)
            continue
        m = RE_BLCK.match(line)
        if m:
            cur["blck"] = m.group(1)
            continue
        m = RE_TSIZE.match(line)
        if m:
            cur["size"] = m.group(1)
            continue
        m = RE_ISQ.match(line)
        if m:
            cur["isq"] = m.group(1) == "true"
            continue
    if cur is not None:
        fail("unterminated entry in type_traits")
    if len(entries) != 43:
        fail(f"type_traits: expected 43 explicit entries, parsed {len(entries)}")
    return entries, line0


RE_ENUM_LINE = re.compile(r"^\s*([A-Z0-9_]+)\s*(?:=\s*([^,]+?))?\s*,(?:\s*//.*)?$")


def parse_ggml_type_enum(src: str) -> dict[str, int]:
    """Values of enum ggml_type, so every table slot can be checked for coverage."""
    values = parse_enum(src, "ggml_type")
    if values.get("GGML_TYPE_COUNT") != 43:
        fail(f"GGML_TYPE_COUNT parses as {values.get('GGML_TYPE_COUNT')}")
    return values


def parse_enum(src: str, enum_name: str) -> dict[str, int]:
    """Resolve a public enum to {member: value}.

    Handles implicit members, integer literals and the two expression forms the
    public headers actually use: ``1 << n`` and a previously defined member.
    """
    body, _ = slice_between(src, f"enum {enum_name} {{", "};", col0=False)
    values: dict[str, int] = {}
    nxt = 0
    for raw in body.splitlines():
        line = re.sub(r"//.*$", "", raw).strip()
        if not line:
            continue
        m = re.match(r"^([A-Za-z0-9_]+)\s*(?:=\s*(.+?))?\s*,\s*$", line)
        if not m:
            fail(f"unparsable {enum_name} line: {raw!r}")
        name, expr = m.group(1), m.group(2)
        if expr is None:
            value = nxt
        else:
            expr = expr.strip()
            mshift = re.fullmatch(r"(\d+)\s*<<\s*(\d+)", expr)
            if re.fullmatch(r"-?\d+", expr):
                value = int(expr)
            elif mshift:
                value = int(mshift.group(1)) << int(mshift.group(2))
            elif expr in values:
                value = values[expr]
            else:
                fail(f"{enum_name}.{name}: cannot resolve {expr!r}")
        values[name] = value
        nxt = value + 1
    return values


def emit_abi(entries, out: Path, type_enum: dict[str, int]) -> None:
    def index_of(idx_text: str) -> int:
        return int(idx_text) if idx_text.isdigit() else type_enum[idx_text]

    ordered = sorted(entries, key=lambda e: index_of(e[0]))
    lines = [
        "/* Generated by tests/extract_native.py from ggml/src/ggml.c -- do not edit. */",
        "#pragma once",
        "#include <stdint.h>",
        "#define GGML_COMMON_DECL_CPP",
        '#include "ggml.h"',
        '#include "ggml-common.h"',
        "",
        "typedef struct { const char * name; int64_t blck; int64_t tsize; int64_t isq; } gs_nat_type;",
        "",
        "// Positional: the extractor verified that every slot 0..GGML_TYPE_COUNT-1 is",
        "// present in the native table, so the index is the position.",
        "static const gs_nat_type NAT_ABI[GGML_TYPE_COUNT] = {",
    ]
    for idx, name, blck, size, isq in ordered:
        nm = "NULL" if name is None else f'"{name}"'
        lines.append(f"    {{ {nm}, {blck}, (int64_t) {size}, {1 if isq else 0} }}, // [{idx}]")
    lines.append("};")
    lines.append("")
    (out / "native_abi.h").write_text("\n".join(lines))


def parse_cases(src: str, func_sig: str, what: str, expect: int | None = None) -> list[tuple[str, str]]:
    """Parse ``case X: return "y";`` in either one-line or two-line form."""
    body, _ = slice_between(src, func_sig, "}")
    found: list[tuple[str, str]] = []
    pending: str | None = None
    for line in body.splitlines():
        m = RE_CASE_RET.match(line)
        if m:
            found.append((m.group(1), m.group(2)))
            pending = None
            continue
        m = re.match(r"^\s*case\s+([A-Za-z0-9_]+)\s*:\s*$", line)
        if m:
            pending = m.group(1)
            continue
        m = re.match(r'^\s*return\s+"((?:[^"\\]|\\.)*)"\s*;\s*$', line)
        if m and pending is not None:
            found.append((pending, m.group(1)))
            pending = None
            continue
    if not found:
        fail(f"{what}: no case/return pairs parsed")
    if expect is not None and len(found) != expect:
        fail(f"{what}: expected {expect} cases, parsed {len(found)}")
    return found


def emit_simple_table(out: Path, name: str, rows: list[tuple[str, str]], header: str) -> None:
    body = "\n".join(f'    {{ {k}, "{v}" }},' for k, v in rows)
    (out / header).write_text(body + "\n", encoding="utf-8")
    _ = name


def parse_status(src: str) -> tuple[list[tuple[str, str]], str]:
    body, _ = slice_between(src, "const char * ggml_status_to_string(enum ggml_status status) {", "}")
    rows = []
    default = None
    for line in body.splitlines():
        m = RE_CASE_RET.match(line)
        if m:
            rows.append((m.group(1), m.group(2)))
            continue
        m = re.match(r'^\s*return\s+"((?:[^"\\]|\\.)*)"\s*;\s*$', line)
        if m:
            default = m.group(1)
    if len(rows) != 4 or default is None:
        fail(f"ggml_status_to_string: parsed {len(rows)} cases and default={default!r}")
    return rows, default


def parse_ftype(src: str) -> tuple[list[tuple[str, str]], str, str]:
    m = RE_FTYPE_PREFIX.search(src) if False else re.search(RE_FTYPE_PREFIX.pattern, src, re.M)
    if not m:
        fail("LLAMA_FTYPE_PREFIX not found")
    prefix = m.group(1)
    body, _ = slice_between(src, "const char * llama_ftype_name(llama_ftype ftype) {", "}")
    rows = []
    default = None
    for line in body.splitlines():
        m = RE_CASE_NAME.match(line)
        if m:
            rows.append((m.group(1), m.group(2)))
            continue
        m = RE_DEFAULT_NAME.match(line)
        if m:
            default = m.group(1)
    if len(rows) != 36 or default is None:
        fail(f"llama_ftype_name: parsed {len(rows)} cases, default={default!r}")
    return rows, default, prefix


def parse_meta(src: str) -> tuple[list[tuple[str, str]], bool]:
    body, _ = slice_between(src, "const char * llama_model_meta_key_str(llama_model_meta_key key) {", "}")
    rows = []
    null_default = False
    for line in body.splitlines():
        m = RE_CASE_RET.match(line)
        if m:
            rows.append((m.group(1), m.group(2)))
            continue
        if RE_CASE_NULL.match(line):
            null_default = True
    if len(rows) != 12 or not null_default:
        fail(f"llama_model_meta_key_str: parsed {len(rows)} cases, null_default={null_default}")
    return rows, null_default


def parse_from_str(src: str) -> list[tuple[str, str]]:
    body, _ = slice_between(src, "enum llama_load_mode llama_load_mode_from_str(const char * str) {", "}")
    rows = []
    for line in body.splitlines():
        m = RE_STRCMP.match(line)
        if m:
            rows.append((m.group(2), m.group(1)))
    if len(rows) != 6:
        fail(f"llama_load_mode_from_str: parsed {len(rows)} pairs")
    return rows


def parse_limit(src: str, func: str) -> int:
    m = re.search(r"^size_t\s+" + func + r"\((?:void)?\)\s*\{\s*\n\s*return\s+(\d+);\s*\n\}\s*$", src, re.M)
    if not m:
        fail(f"{func} not found")
    return int(m.group(1))


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    root = Path(sys.argv[1])
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    ggml_c = root / "ggml" / "src" / "ggml.c"
    llama_cpp = root / "src" / "llama.cpp"
    loader_cpp = root / "src" / "llama-model-loader.cpp"
    model_cpp = root / "src" / "llama-model.cpp"
    for p in (ggml_c, llama_cpp, loader_cpp, model_cpp):
        if not p.is_file():
            fail(f"missing native source: {p}")

    entries, line0 = parse_type_traits(ggml_c.read_text(errors="replace"))
    type_enum = parse_ggml_type_enum((root / "ggml" / "include" / "ggml.h").read_text(errors="replace"))
    covered = set()
    for idx, _n, _b, _s, _q in entries:
        covered.add(type_enum[idx] if not idx.isdigit() else int(idx))
    if covered != set(range(type_enum["GGML_TYPE_COUNT"])):
        fail(f"type_traits does not cover every slot: {sorted(covered)}")
    emit_abi(entries, out, type_enum)

    status_rows, status_default = parse_status(ggml_c.read_text(errors="replace"))
    ftype_rows, ftype_default, ftype_prefix = parse_ftype(loader_cpp.read_text(errors="replace"))
    flash_rows = parse_cases(
        llama_cpp.read_text(errors="replace"),
        "const char * llama_flash_attn_type_name(enum llama_flash_attn_type flash_attn_type) {",
        "llama_flash_attn_type_name",
        3,
    )
    load_rows = parse_cases(
        llama_cpp.read_text(errors="replace"),
        "const char * llama_load_mode_name(enum llama_load_mode load_mode) {",
        "llama_load_mode_name",
        6,
    )
    from_str_rows = parse_from_str(llama_cpp.read_text(errors="replace"))
    meta_rows, _ = parse_meta(model_cpp.read_text(errors="replace"))

    def table(name: str, rows, with_value: bool) -> None:
        body = []
        for k, v in rows:
            body.append(f'    {{ {k}, "{v}" }},')
        (out / name).write_text("\n".join(body) + "\n", encoding="utf-8")

    table("nat_status.inc", status_rows, True)
    table("nat_ftype.inc", ftype_rows, True)
    table("nat_flash.inc", flash_rows, True)
    table("nat_load.inc", load_rows, True)
    # parsed rows are (text, value): the reverse order of the name tables
    (out / "nat_from_str.inc").write_text(
        "\n".join(f'    {{ "{text}", {val} }},' for val, text in from_str_rows) + "\n",
        encoding="utf-8",
    )
    table("nat_meta.inc", meta_rows, True)

    (out / "native_nm.h").write_text(
        "\n".join([
            "/* Generated by tests/extract_native.py from the llama.cpp sources -- do not edit. */",
            "#pragma once",
            "#include <stdint.h>",
            '#include "llama.h"',
            f'static const char * const NAT_STATUS_DEFAULT = "{status_default}";',
            f'static const char * const NAT_FTYPE_DEFAULT = "{ftype_default}";',
            f'static const char * const NAT_FTYPE_PREFIX = "{ftype_prefix}";',
            "",
            "typedef struct { int64_t key; const char * text; } gs_nat_named;",
            "typedef struct { const char * text; int64_t value; } gs_nat_parsed;",
            "",
            "static const gs_nat_named NAT_STATUS[] = {",
            '#include "nat_status.inc"',
            "};",
            "static const gs_nat_named NAT_FTYPE[] = {",
            '#include "nat_ftype.inc"',
            "};",
            "static const gs_nat_named NAT_FLASH[] = {",
            '#include "nat_flash.inc"',
            "};",
            "static const gs_nat_named NAT_LOAD[] = {",
            '#include "nat_load.inc"',
            "};",
            "static const gs_nat_named NAT_META[] = {",
            '#include "nat_meta.inc"',
            "};",
            "static const gs_nat_parsed NAT_FROM_STR[] = {",
            '#include "nat_from_str.inc"',
            "};",
            "",
            "static const int NAT_STATUS_N = (int) (sizeof(NAT_STATUS) / sizeof(NAT_STATUS[0]));",
            "static const int NAT_FTYPE_N = (int) (sizeof(NAT_FTYPE) / sizeof(NAT_FTYPE[0]));",
            "static const int NAT_FLASH_N = (int) (sizeof(NAT_FLASH) / sizeof(NAT_FLASH[0]));",
            "static const int NAT_LOAD_N = (int) (sizeof(NAT_LOAD) / sizeof(NAT_LOAD[0]));",
            "static const int NAT_META_N = (int) (sizeof(NAT_META) / sizeof(NAT_META[0]));",
            "static const int NAT_FROM_STR_N = (int) (sizeof(NAT_FROM_STR) / sizeof(NAT_FROM_STR[0]));",
            "",
        ]),
        encoding="utf-8",
    )

    max_dev = parse_limit(llama_cpp.read_text(errors="replace"), "llama_max_devices")
    max_tbo = parse_limit(llama_cpp.read_text(errors="replace"), "llama_max_tensor_buft_overrides")
    (out / "native_limits.h").write_text(
        "\n".join([
            "/* Generated by tests/extract_native.py from src/llama.cpp -- do not edit. */",
            "#pragma once",
            "#include <stdint.h>",
            f"static const int64_t NAT_LLAMA_MAX_DEVICES = {max_dev};",
            f"static const int64_t NAT_LLAMA_MAX_TENSOR_BUFT_OVERRIDES = {max_tbo};",
            "",
        ]),
        encoding="utf-8",
    )

    print(
        f"extract: type_traits@{line0} entries={len(entries)} status={len(status_rows)} "
        f"ftype={len(ftype_rows)} flash={len(flash_rows)} load={len(load_rows)} "
        f"from_str={len(from_str_rows)} meta={len(meta_rows)} "
        f"max_devices={max_dev} max_buft_overrides={max_tbo}"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ExtractError as exc:
        print(f"extract: FAILED: {exc}", file=sys.stderr)
        sys.exit(1)
