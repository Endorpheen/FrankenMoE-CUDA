#!/usr/bin/env python3
"""Inventory a recorded runtime link without loading a model or a library.

Read-only inputs: compiler command records, Ninja rules, object symbol tables,
repository sources, and an immutable run directory. The JSON describes linked
translation units and undefined symbol references, not observed execution or a
percentage of migration completion. No build, compiler, dlopen or inference runs.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NATIVE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".cu"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(path):
    return str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)


def records(path):
    text = path.read_text()
    try:
        rows = json.loads(text)
    except json.JSONDecodeError:
        rows = [json.loads(line) for line in text.splitlines() if line.strip()]
    if rows and isinstance(rows[0], str):
        rows = [rows]
    return rows


def output_of(command, directory):
    if "-o" not in command:
        return None
    return (directory / command[command.index("-o") + 1]).resolve()


def compile_index(paths, record_cwds):
    index, generators = {}, {}
    for path in paths:
        for row in records(path):
            directory = Path(row["directory"]) if isinstance(row, dict) else record_cwds.get(str(path), ROOT)
            command = (row.get("arguments") or shlex.split(row["command"])) if isinstance(row, dict) else row
            out = output_of(command, directory)
            if out is None:
                continue
            if "-c" in command:
                source = (directory / command[command.index("-c") + 1]).resolve()
                index[out] = (source, command, directory)
            elif Path(command[0]).name == "goose" and command[-1].endswith(".goose"):
                generators[out] = (directory / command[-1]).resolve()
    return index, generators


def ninja_links(path):
    links = {}
    for line in path.read_text().splitlines():
        if not line.startswith("build ") or ": " not in line:
            continue
        lhs, rhs = line[6:].split(": ", 1)
        fields = rhs.split()
        if not any(x in fields[0] for x in ("LIBRARY_LINKER", "EXECUTABLE_LINKER")):
            continue
        # Implicit .a dependencies are link inputs as well; order-only inputs are not.
        inputs = fields[1:fields.index("||")] if "||" in fields else fields[1:]
        links[lhs.split()[0]] = [x for x in inputs if x.endswith((".o", ".a"))]
    return links


def symbols(path, cache):
    cache_path = cache / (digest(path) + ".text-v2.json")
    if cache_path.exists():
        return json.loads(cache_path.read_text())
    result = subprocess.run(["nm", "--format=posix", str(path)], check=True,
                            capture_output=True, text=True)
    defined, referenced, linkable = [], [], []
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        name, kind = parts[:2]
        if kind in ("T", "t", "W", "w") and len(parts) >= 3:
            defined.append(name)
            if kind in ("T", "W"):
                linkable.append(name)
        elif kind in ("U", "w", "v") and len(parts) == 2:
            referenced.append(name)
    value = [sorted(set(defined)), sorted(set(referenced)), sorted(set(linkable))]
    cache_path.write_text(json.dumps(value))
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--commands", action="append", type=Path, required=True)
    parser.add_argument("--core-link", type=Path, required=True)
    parser.add_argument("--component-link", action="append", default=[], metavar="NAME=RECORD")
    parser.add_argument("--record-cwd", action="append", default=[], metavar="RECORD=DIRECTORY")
    parser.add_argument("--object-source", action="append", default=[], metavar="OBJECT=SOURCE",
                        help="Explicit recipe-only mapping when the compile command was not recorded")
    parser.add_argument("--symbol-cache", type=Path, required=True)
    parser.add_argument("--ninja", type=Path, required=True)
    parser.add_argument("--runtime-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path, help="Also write a compact, reviewable source inventory")
    args = parser.parse_args()
    overrides = {"libllama.so.0": args.core_link}
    for item in args.component_link:
        name, path = item.split("=", 1)
        overrides[name] = Path(path)
    record_cwds = {p: Path(d).resolve() for p, d in (x.split("=", 1) for x in args.record_cwd)}
    index, generators = compile_index(args.commands, record_cwds)
    recipe_objects = set()
    for item in args.object_source:
        obj, src = (Path(x).resolve() for x in item.split("=", 1))
        if obj in index:
            raise ValueError(f"recipe cannot override a recorded compiler command: {obj}")
        index[obj] = (src, [], ROOT)
        recipe_objects.add(obj)
    args.symbol_cache.mkdir(parents=True, exist_ok=True)
    ninja = ninja_links(args.ninja)
    support = args.ninja.parent.resolve()
    components = {}
    missing = []
    for artifact in sorted(args.runtime_dir.iterdir()):
        if artifact.is_symlink() or not artifact.is_file():
            continue
        with artifact.open("rb") as stream:
            if stream.read(4) != b"\x7fELF":
                continue
        if artifact.name in overrides:
            record = overrides[artifact.name]
            link = [r for r in records(record) if "-shared" in r][-1]
            link_cwd = record_cwds.get(str(record), ROOT)
            produced = output_of(link, link_cwd)
            if produced is None or digest(produced) != digest(artifact):
                raise ValueError(f"recorded link output does not match runtime bytes: {artifact.name}")
            inputs = [(link_cwd / x).resolve() for x in link if x.endswith(".o")]
        else:
            target = [name for name in ninja if Path(name).name == artifact.name or
                      Path(name).name.startswith(artifact.name + ".")]
            if len(target) != 1:
                raise ValueError(f"ambiguous or missing Ninja link for {artifact.name}: {target}")
            if digest(support / target[0]) != digest(artifact):
                raise ValueError(f"Ninja artifact differs from runtime bytes; provide --component-link: {artifact.name}")
            inputs = [(support / name).resolve() for name in ninja[target[0]]]
        components[artifact.name] = {"sha256": digest(artifact), "inputs": inputs}

    # Resolve static archives through their recorded Ninja object lists.
    def objects(inputs, external):
        out = []
        for path in inputs:
            if path.suffix == ".a":
                if not path.is_relative_to(support):
                    external.append({"path": str(path), "sha256": digest(path)})
                    continue
                key = str(path.relative_to(support))
                if key not in ninja:
                    raise ValueError(f"unrecorded archive: {path}")
                out.extend(objects([(support / name).resolve() for name in ninja[key]], external))
            else:
                out.append(path)
        return out

    units = {}
    for name, component in components.items():
        component["external_archives"] = []
        component["objects"] = [str(p) for p in objects(component.pop("inputs"), component["external_archives"])]
        for obj in map(Path, component["objects"]):
            if obj in units:
                units[obj]["components"].append(name)
                continue
            if obj not in index:
                missing.append(str(obj))
                continue
            source, command, directory = index[obj]
            text = source.read_text(errors="replace")
            defined, referenced, linkable = symbols(obj, args.symbol_cache)
            # Unity files are compiler inputs, but their included .cpp files own the code.
            unity = [(source.parent / x).resolve() for x in re.findall(r'^#include "([^"]+\.cpp)"', text, re.M)]
            sources = unity or [source]
            source_rows = []
            for src in sources:
                body = src.read_text(errors="replace")
                calls = [{"symbol": m.group(1), "line": body.count("\n", 0, m.start()) + 1}
                         for m in re.finditer(r'\b((?:fg\d{3}[a-z]?_|fg_)[A-Za-z0-9_]+|slru_plan)\s*\(', body)]
                source_rows.append({"path": relative(src), "sha256": digest(src),
                                    "lines": len(body.splitlines()),
                                    "includes": re.findall(r'^\s*#\s*include\s*[<"]([^>"\n]+)', body, re.M),
                                    "goose_identifiers_lexical": calls})
            units[obj] = {"object": str(obj), "object_sha256": digest(obj),
                          "compile_provenance": "recipe only; command record missing" if obj in recipe_objects else "recorded command",
                          "components": [name], "sources": source_rows,
                          "generated_from": relative(generators[source]) if source in generators else None,
                          "defined_text_symbols": defined, "linkable_text_symbols": linkable,
                          "undefined_symbol_references": referenced}
    if missing:
        raise ValueError("objects without compile provenance: " + ", ".join(missing))
    providers = collections.defaultdict(list)
    for obj, unit in units.items():
        for symbol in unit["linkable_text_symbols"]:
            providers[symbol].append(str(obj))
    for unit in units.values():
        unit["linked_symbol_providers"] = {s: providers[s] for s in unit["undefined_symbol_references"] if s in providers}

    native = {s["path"]: s for u in units.values() for s in u["sources"] if Path(s["path"]).suffix in NATIVE_SUFFIXES}
    tracked = set(subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True).stdout.decode().split("\0"))
    for row in native.values():
        row["tracked"] = row["path"] in tracked
    summary = dict(sorted(collections.Counter(Path(p).suffix for p in native).items()))
    result = {"schema": 1, "method": "recorded link closure + nm text symbols; no model or library loaded",
              "limitations": ["Undefined references are link dependencies, not a dynamic call graph.",
                              "Optimized/inlined/stripped functions need source analysis; symbol counts are not coverage.",
                              "Lexical Goose identifiers include declarations/comments and do not prove execution.",
                              "An object hash is provenance, not proof that its current source was used to compile it.",
                              "Generated C includes and headers can contain additional native code; no deletion is authorized."],
              "inputs": [{"path": str(p), "sha256": digest(p)} for p in dict.fromkeys([*args.commands, *overrides.values(), args.ninja])],
              "components": components, "native_source_counts_by_extension": summary,
              "native_source_files": len(native),
              "untracked_repository_sources": sorted(p for p, row in native.items() if not Path(p).is_absolute() and not row["tracked"]),
              "units": list(units.values())}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    if args.summary:
        compact = {key: result[key] for key in ("schema", "method", "limitations", "inputs", "native_source_counts_by_extension",
                                              "native_source_files", "untracked_repository_sources")}
        compact["components"] = {name: {"sha256": c["sha256"], "objects": len(c["objects"]),
                                       "external_archives": c["external_archives"]} for name, c in components.items()}
        compact["sources"] = [{"object": u["object"], "object_sha256": u["object_sha256"],
                               "components": u["components"], "compile_provenance": u["compile_provenance"],
                               "generated_from": u["generated_from"],
                               "sources": [{k: s[k] for k in ("path", "sha256", "lines", "tracked")} for s in u["sources"]],
                               "text_symbols": len(u["defined_text_symbols"]),
                               "goose_references": [s for s in u["undefined_symbol_references"] if s.startswith("fg") or s == "slru_plan"]}
                              for u in units.values()]
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(json.dumps(compact, indent=2) + "\n")
    print(json.dumps({"components": len(components), "objects": len(units), "native_sources": len(native),
                      "by_extension": summary, "untracked_repository_sources": result["untracked_repository_sources"],
                      "output": str(args.output)}, indent=2))


if __name__ == "__main__":
    main()
