#!/usr/bin/env python3
"""Compose canonical Goose modules into one translation unit.

Goose has no re-export: `import x` needs x.goose next to the importer, so the
canonical tree carries byte-identical copies (jsonparse in four directories).
A single runtime therefore needs a composition step. This tool concatenates the
requested modules in import order, emitting each file once and dropping the
local `import` lines that it resolved.

Usage:
    python3 tools/compose-goose.py OUT.goose goose/server/mcp-core.goose ...
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOOSE = ROOT / "goose"

# Module name -> canonical path when the name has no sibling file (dashed file
# names cannot be imported, so the copies exist).
# Modules whose generic helper names collide with another module's: rename the
# helper family inside that module only, so both can share one unit.
RENAMES = {
    "tools-pure.goose": {
        "PNode": "TPNode",
        "pget": "tp_pget",
        "pget_int": "tp_pget_int",
        "pget_bool": "tp_pget_bool",
        "pget_str": "tp_pget_str",
        "pget_str": "tp_pget_str",
        "PJ_K_BOOL": "TP_K_BOOL",
        "PJ_K_INT": "TP_K_INT",
        "PJ_K_UINT": "TP_K_UINT",
        "PJ_K_STRING": "TP_K_STRING",
        "PJ_K_ARRAY": "TP_K_ARRAY",
        "PJ_K_OBJECT": "TP_K_OBJECT",
    },
    "goose-opt.goose": {
        "sqrt": "op_sqrt",
    },
    "quant-rows2.goose": {
        "st_f16": "st2_f16",
        "st_u8": "st2_u8",
        "st_u32": "st2_u32",
        "fabs32": "fabs2_32",
        "ld_f32": "ld2_f32",
        "fail": "qz2_fail",
    },
    "builder.goose": {
        "K_SEQ": "JB_K_SEQ",
        "K_ALT": "JB_K_ALT",
        "K_OPT": "JB_K_OPT",
        "K_REP": "JB_K_REP",
        "K_LIT": "JB_K_LIT",
        "K_REF": "JB_K_REF",
    },
    "format-grammar.goose": {
        # Private helpers whose names collide with exported entries elsewhere;
        # the exports keep their public names, so these move (EXP677).
        "format_literal": "fg_format_literal",
        "add_rule": "fg_add_rule",
        "object_branch": "fg_object_branch",
        "K_NULL": "FG_K_NULL",
        "K_STRING": "FG_K_STRING",
        "K_ARRAY": "FG_K_ARRAY",
        "K_TUPLE": "FG_K_TUPLE",
        "K_OBJECT": "FG_K_OBJECT",
        "I64_MAX": "FG_I64_MAX",
    },
    # Native helper entry points bound through extern symbols: the symbol string
    # stays, only the local name is scoped to the module so two modules can share
    # one unit without touching the ABI.
    "goose-model-loader.goose": {
        "fail": "fgl_fail",
        "ensure": "fgl_ensure",
        "st_i64": "fgl_st_i64",
    },
    "goose-model-graph.goose": {
        "fail": "fgg_fail",
        "ensure": "fgg_ensure",
    },
    "file-io.goose": {
        "st_i64": "fio_st_i64",
    },
    # gguf-offsets.goose binds the i64 accessors to fg457_* while file-io.goose
    # binds the same names to fg458_*; the two symbols are separate primitives, so
    # the names must be scoped per module rather than merged.
    # refs.goose exports sanitize_into(s, start, out, dst); format-grammar.goose
    # has a private sanitize_into(s, dst, at) with a different meaning. Scope the
    # refs family so the exported entry keeps its own name.
    # visit.goose is an independent JSON visitor; its private helpers collide with
    # format-grammar.goose's by name but not by signature or meaning.
    "visit.goose": {
        # visit.goose's sanitize_into(s, dst, dst_off) differs from the exported
        # refs.goose one (s, start, out, dst) and from format-grammar's private
        # (s, dst, at); all three are separate helpers with one name.
        "sanitize_into": "vis_sanitize_into",
        "append_s": "vis_append_s",
        "append_c": "vis_append_c",
        "append_dec": "vis_append_dec",
        "rep_impl": "vis_rep_impl",
        "object_rule": "vis_object_rule",
    },
    "refs.goose": {
        "is_reserved": "refs_is_reserved",
        "rule_name": "refs_rule_name",
        "sanitize_into": "refs_sanitize_into",
        "sanitize_into_impl": "refs_sanitize_into_impl",
        # add_rule here is the exported table builder; format-grammar.goose has a
        # private add_rule with a different signature.
    },
    # mmq-dispatch binds its own fail (fg203d_*) and ensure; both collide by name
    # with earlier modules that bind different symbols.
    "goose-mmq-dispatch.goose": {
        "fail": "mmqd_fail",
        "ensure": "mmqd_ensure",
    },
    # shards.goose binds its own storage primitives (fg459_*) and has a private
    # put(out, i, v) distinct from the offsets one.
    "shards.goose": {
        "put": "shards_put",
        "st_u8": "shards_st_u8",
        "st_i64": "shards_st_i64",
        "ld_i64": "shards_ld_i64",
    },
    "gguf-offsets.goose": {
        "st_i64": "off_st_i64",
        "ld_i64": "off_ld_i64",
        "put": "off_put",
    },
    # The Jinja runtime's K_* are AST node kinds (13..18); the JSON parser's K_*
    # are value kinds (0..7). Same names, different domains: scope the runtime's
    # family so both can share one unit.
    "runtime.goose": {
        "K_NULL": "JR_K_NULL",
        "K_BOOL": "JR_K_BOOL",
        "K_INT": "JR_K_INT",
        "K_UINT": "JR_K_UINT",
        "K_FLOAT": "JR_K_FLOAT",
        "K_STRING": "JR_K_STRING",
        "K_ARRAY": "JR_K_ARRAY",
        "K_OBJECT": "JR_K_OBJECT",
        "K_TUPLE": "JR_K_TUPLE",
    },
    # quant-weighted re-declares helper families that quant-rows/rows2 and the
    # schema converter already own; its exports are qw_*, so scope the internals
    # the same way.
    "quant-weighted.goose": {
        "iq2_qgrid": "qw_iq2_qgrid",
        "iq3_qgrid": "qw_iq3_qgrid",
        "f32_bits": "qw_f32_bits",
        "f32_of_bits": "qw_f32_of_bits",
        "floor32": "qw_floor32",
        "iq2_grid": "qw_iq2_grid",
        "iq2_map": "qw_iq2_map",
        "iq2_nb": "qw_iq2_nb",
        "iq3_grid": "qw_iq3_grid",
        "iq3_map": "qw_iq3_map",
        "iq3_nb": "qw_iq3_nb",
        "ldexp32": "qw_ldexp32",
        "ld_f16": "qw_ld_f16",
        "ld_u8": "qw_ld_u8",
        "log232": "qw_log232",
        "lround32": "qw_lround32",
        "make_q3": "qw_make_q3",
        "make_qkx2": "qw_make_qkx2",
        "make_qx": "qw_make_qx",
        "nearest": "qw_nearest",
        "sgetf": "qw_sgetf",
        "sort16": "qw_sort16",
        "sqrt32": "qw_sqrt32",
        "ssetb": "qw_ssetb",
        "st_f32": "qw_st_f32",
        "st_u16": "qw_st_u16",
        "tmp": "qw_tmp",
        "best_index_mxfp4": "qw_best_index_mxfp4",
        "iq4_nl_block": "qw_iq4_nl_block",
        "qz_rows": "qw_qz_rows",
        "iq3_best_nb": "qw_iq3_best_nb",
        "iq2_best_nb": "qw_iq2_best_nb",
        "iq1_best_nb2": "qw_iq1_best_nb2",
        "fail": "qw_fail",
        "e8m0_half": "qw_e8m0_half",
        "ld_u32v": "qw_ld_u32v",
        "sa": "qw_sa",
        "scale_min_k4": "qw_scale_min_k4",
        "sgetb": "qw_sgetb",
        "ssetf": "qw_ssetf",
        "ue4m3_from_f32": "qw_ue4m3_from_f32",
        "ue4m3_to_f32": "qw_ue4m3_to_f32",
    },
    # The CLI protocol's escaper takes a byte slice; the Jinja one escapes a code
    # point. Same private name, different functions.
    "cli-protocol.goose": {
        "cp_escape": "cp_escape_text",
    },
    "quant-dequant2.goose": {
        "iq2_grid": "qd_iq2_grid",
        "iq3_grid": "qd_iq3_grid",
        "e8m0_half": "qd_e8m0_half",
        "scale_min_k4": "qd_scale_min_k4",
    },
}

ALIASES = {
    "jsonparse": GOOSE / "json" / "json-parser.goose",
}

IMPORT = re.compile(r"(?m)^import\s+([A-Za-z0-9_.]+)\s*;\s*$")


def resolve(name: str, near: Path) -> Path | None:
    if name in ("std", "math"):
        return None
    sibling = near.parent / (name.replace(".", "/") + ".goose")
    if sibling.is_file():
        return sibling
    if name in ALIASES and ALIASES[name].is_file():
        return ALIASES[name]
    return None


BUILTINS: set[str] = set()


def collect(path: Path, out: list[Path], seen: set[Path]) -> None:
    path = path.resolve()
    if path in seen:
        return
    seen.add(path)
    text = path.read_text()
    for name in IMPORT.findall(text):
        dep = resolve(name, path)
        if dep is not None:
            collect(dep, out, seen)
        elif name in ("std", "math"):
            BUILTINS.add(name)
    out.append(path)


START = re.compile(r'^(?:export\s+|recursive\s+|extern\s+"[^"]*"\s+)?(fn|struct|const|type)\s+([A-Za-z_]\w*)')
KIND_NAMES = {"fn": "fn", "struct": "struct", "const": "const", "type": "type"}


def declarations(text: str):
    """Split a module into (kind, name, block) declarations, keeping comments."""
    out = []
    lines = text.splitlines(keepends=True)
    i = 0
    lead = []
    while i < len(lines):
        line = lines[i]
        m = START.match(line)
        if not m:
            lead.append(line)
            i += 1
            continue
        kind, name = m.group(1), m.group(2)
        block = lead + [line]
        lead = []
        if kind == "const" or kind == "type":
            # A const/type may continue while the next line is indented.
            i += 1
            while i < len(lines) and lines[i][:1] in (" ", "\t"):
                block.append(lines[i])
                i += 1
        elif line.lstrip().startswith("extern"):
            i += 1
            while i < len(lines) and not block[-1].rstrip().endswith(";"):
                block.append(lines[i])
                i += 1
        else:
            depth = line.count("{") - line.count("}")
            i += 1
            while i < len(lines) and depth <= 0:
                block.append(lines[i])
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
            while i < len(lines) and depth > 0:
                block.append(lines[i])
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
        out.append((kind, name, "".join(block)))
    if lead:
        out.append(("tail", "", "".join(lead)))
    return out


EXPORT = re.compile(r"(?m)^export fn (\w+)")


STRING = re.compile(r'"(?:[^"\\]|\\.)*"')


def rename_text(text: str, old: str, new: str) -> str:
    """Rename an identifier outside string literals and comments.

    String literals keep their bytes because an extern symbol may appear inside
    one; comments are skipped because a stray quote in prose (for example
    `escape \' \\ \"`) used to make the string scanner swallow the code that
    followed it, so a declaration after such a comment kept its old name while
    its callers were renamed.
    """
    out = []
    for line in text.splitlines(keepends=True):
        # Split the code part from a trailing comment.
        code, sep, comment = line.partition("//")
        parts = []
        at = 0
        for m in STRING.finditer(code):
            parts.append(re.sub(r"\b" + re.escape(old) + r"\b", new, code[at:m.start()]))
            parts.append(m.group(0))
            at = m.end()
        parts.append(re.sub(r"\b" + re.escape(old) + r"\b", new, code[at:]))
        out.append("".join(parts) + sep + comment)
    return "".join(out)


def emit(path: Path, seen: dict, conflicts: list, expected: set) -> str:
    text = path.read_text()
    for old, new in RENAMES.get(path.name, {}).items():
        text = rename_text(text, old, new)
    text = IMPORT.sub("", text)
    expected.update(EXPORT.findall(text))
    rel = path.relative_to(ROOT) if path.is_relative_to(ROOT) else path
    kept = []
    for kind, name, block in declarations(text):
        if kind == "fn" and name == "main":
            continue
        if kind == "tail":
            kept.append(block)
            continue
        key = (kind, name)
        code = []
        for line in block.splitlines():
            line = re.sub(r"//.*$", "", line).strip()
            if line:
                code.append(re.sub(r"\s+", " ", line))
        norm = "\n".join(code)
        # An extern declaration binds a native symbol; it carries no body and must
        # not collide with a Goose function of the same name (fg158_fail is bound
        # as `fail`, while format-grammar.goose defines its own `fail`). Identical
        # bindings are emitted once, because a second identical declaration makes
        # the call ambiguous in the typechecker.
        # A leading comment may precede the declaration inside the same block.
        first_code = ""
        for l in block.splitlines():
            l = re.sub(r"//.*$", "", l).strip()
            if l:
                first_code = l
                break
        if first_code.startswith("extern"):
            # A binding lives in its own namespace: it never defines the name, so
            # it must neither collide with a Goose function of the same name nor
            # be reported as one.
            bkey = ("extern", name)
            if bkey not in seen:
                seen[bkey] = norm
                kept.append(block)
            elif seen[bkey] != norm:
                conflicts.append(f"extern {name}: {rel} binds a different signature")
            continue
        if key in seen:
            if seen[key] != norm:
                conflicts.append(f"{kind} {name}: {rel} differs from an earlier module")
            continue
        seen[key] = norm
        kept.append(block)
    return f"// ---- composed from {rel} ----\n" + "".join(kept).rstrip() + "\n"


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    dest = Path(sys.argv[1])
    order: list[Path] = []
    seen: set[Path] = set()
    for arg in sys.argv[2:]:
        collect(Path(arg), order, seen)
    seen: dict = {}
    conflicts: list = []
    expected: set = set()
    header = "".join(f"import {b};\n" for b in sorted(BUILTINS))
    body = header + "\n".join(emit(p, seen, conflicts, expected) for p in order)
    actual = set(EXPORT.findall(body))
    if expected != actual:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        print(f"interface check failed: missing={missing} extra={extra}", file=sys.stderr)
        return 1
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(body)
    if conflicts:
        print("conflicts:", file=sys.stderr)
        for c in conflicts:
            print("  " + c, file=sys.stderr)
        print("conflicting bodies must be unified before this set can be one unit", file=sys.stderr)
        return 1
    print(f"composed {len(order)} module(s) -> {dest}")
    for p in order:
        print("  " + str(p.relative_to(ROOT) if p.is_relative_to(ROOT) else p))
    return 0


if __name__ == "__main__":
    sys.exit(main())
