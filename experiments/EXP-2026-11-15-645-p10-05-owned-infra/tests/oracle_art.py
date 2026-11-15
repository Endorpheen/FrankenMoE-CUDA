#!/usr/bin/env python3
"""Native oracle for P10-05 S3 (generated-artifact and canonical-tree policy).

Native arms:

    generated  tools/check-generated-files.py is_generated_goose_c(), imported
               from the copied script and called on the same path and head that
               index_head() would give it (the first 512 bytes)
    limit      the script's own `int(args.mb * 1024 * 1024)` expression, eval'd
               from the copied text
    gate       the script's own `if args.all or size >= limit:` condition, eval'd
               from the copied text, then the failed/warned branch
    required   tools/check-goose-tree.py check_required(), imported and called on
               a synthetic tree
    manifest   the manifest row problem order of check-goose-tree.py main()
    verdict    the exit rule of check-goose-tree.py main()
    exports    re.findall(r"(?m)^export fn (\\w+)", text) with the pattern taken
               from the copied script

Usage: oracle_art.py CASES
Results: "<row>\\t<value>"; integers decimal, text hex of the raw bytes.
"""

from __future__ import annotations

import importlib.util
import re
import sys

sys.dont_write_bytecode = True
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
NATIVE = HERE / "native"


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


GEN = load("native_check_generated", NATIVE / "check-generated-files.py")
TREE = load("native_check_goose_tree", NATIVE / "check-goose-tree.py")
GEN_TEXT = (NATIVE / "check-generated-files.py").read_text()
TREE_TEXT = (NATIVE / "check-goose-tree.py").read_text()

LIMIT_EXPR = re.search(r"limit = (int\(args\.mb \* 1024 \* 1024\))", GEN_TEXT).group(1)
GATE_COND = re.search(r"if (args\.all or size >= limit):", GEN_TEXT).group(1)
EXPORT_RE = re.search(r're\.findall\(r"([^"]+)"', TREE_TEXT).group(1)


class Args:
    def __init__(self, all_flag: int, mb: float) -> None:
        self.all = bool(all_flag)
        self.mb = mb


def decode(field: str) -> str:
    if "~512~" in field:
        field = field.replace("~512~", "x" * 512)
    if "\\" in field:
        return field.replace("\\n", "\n").replace("\\t", "\t").replace("\\r", "\r")
    return field


def main() -> int:
    rows = [l for l in Path(sys.argv[1]).read_text().splitlines() if l and not l.startswith("#")]
    out: list[str] = []
    for idx, raw in enumerate(rows, 1):
        f = raw.split("\t")
        kind = f[0]
        if kind == "generated":
            path, head = decode(f[1]), decode(f[2]).encode()
            r = GEN.is_generated_goose_c(path, head[:512])
            out.append(f"{idx}\t{1 if r else 0}")
        elif kind == "limit":
            mb = int(f[1]) / 1000.0
            limit = eval(LIMIT_EXPR, {"args": Args(0, mb)})  # the native expression
            out.append(f"{idx}\t{limit}")
        elif kind == "gate":
            all_flag, size, limit = int(f[1]), int(f[2]), int(f[3])
            ns = {"args": Args(all_flag, 1.0), "size": size, "limit": limit}
            failed = bool(eval(GATE_COND, ns))
            out.append(f"{idx}\t{2 if failed else 1}")
        elif kind == "required":
            line, file_present, snippet_found = decode(f[1]), f[2], f[3]
            with tempfile.TemporaryDirectory() as td:
                root = Path(td)
                goose = root / "goose"
                goose.mkdir()
                required = root / "required.txt"
                required.write_text(line + "\n")
                stripped = line.strip()
                rel = stripped.partition("\t")[0] if stripped and not stripped.startswith("#") else ""
                if rel and file_present == "1":
                    target = goose / rel
                    target.parent.mkdir(parents=True, exist_ok=True)
                    body = stripped.partition("\t")[2] if snippet_found == "1" else ""
                    target.write_text(body)
                old_goose, old_required = TREE.GOOSE, TREE.REQUIRED
                TREE.GOOSE, TREE.REQUIRED = goose, required
                problems: list[str] = []
                try:
                    TREE.check_required(problems)
                finally:
                    TREE.GOOSE, TREE.REQUIRED = old_goose, old_required
                stripped = line.strip()
                if not stripped or stripped.startswith("#"):
                    code = 0
                elif any(p.startswith("required snippet file missing") for p in problems):
                    code = 1
                elif any(p.startswith("required snippet missing") for p in problems):
                    code = 2
                else:
                    code = 3
                out.append(f"{idx}\t{code}")
        elif kind == "required_parts":
            stripped = decode(f[1]).strip()
            if not stripped or stripped.startswith("#"):
                out.append(f"{idx}\t-")
            else:
                rel, _, snip = stripped.partition("\t")
                out.append(f"{idx}\t{(rel + chr(28) + snip).encode().hex()}")
        elif kind == "orphan":
            in_manifest = f[1]
            out.append(f"{idx}\t{0 if in_manifest == '1' else 1}")
        elif kind == "manifest":
            file_present, hash_match, exp_present = f[1], f[2], f[3]
            if file_present == "0":
                mask = 1
            else:
                mask = (0 if hash_match == "1" else 2) + (0 if exp_present == "1" else 4)
            out.append(f"{idx}\t{mask}")
        elif kind == "verdict":
            problems, warnings = int(f[1]), int(f[2])
            out.append(f"{idx}\t{1 if problems else 0}")
        elif kind == "exports":
            text = decode(f[1])
            names = re.findall(EXPORT_RE, text)
            out.append(f"{idx}\t{chr(10).join(names).encode().hex()}")
        else:
            raise SystemExit(f"unknown case kind: {kind}")
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
