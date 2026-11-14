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


def emit(path: Path, seen: dict, conflicts: list) -> str:
    text = path.read_text()
    for old, new in RENAMES.get(path.name, {}).items():
        text = re.sub(r"\b" + re.escape(old) + r"\b", new, text)
    text = IMPORT.sub("", text)
    rel = path.relative_to(ROOT) if path.is_relative_to(ROOT) else path
    kept = []
    for kind, name, block in declarations(text):
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
    header = "".join(f"import {b};\n" for b in sorted(BUILTINS))
    body = header + "\n".join(emit(p, seen, conflicts) for p in order)
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
