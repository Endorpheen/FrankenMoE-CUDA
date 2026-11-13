#!/usr/bin/env python3
"""Rebuild goose/ from the accepted experiment slices.

Layout: goose/<native directory>/<experiment>/<path inside the experiment>.
The native directory comes from the ledger (docs/goose-migration/tasks.json):
the sources of every task whose evidence names the experiment. When the ledger
has no entry, the experiment README is scanned for a native path, then a topic
fallback is used; anything unresolved lands in goose/_unmapped/.

The six complete Jinja modules already have canonical files (for example
goose/common/jinja/lexer.goose); their experiments are not copied again.

Usage:
  python3 tools/sync-goose-tree.py           # rewrite goose/ and the manifest
  python3 tools/sync-goose-tree.py --check   # report drift, change nothing
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOOSE = ROOT / "goose"
EXPERIMENTS = ROOT / "experiments"
LEDGER = ROOT / "docs/goose-migration/tasks.json"

CANONICAL = {
    "EXP-2026-11-13-502-p7-08-jinja-lexer",
    "EXP-2026-11-13-503-p7-08-jinja-parser",
    "EXP-2026-11-13-504-p7-08-jinja-values",
    "EXP-2026-11-13-505-p7-08-value-json",
    "EXP-2026-11-13-506-p7-08-builtins-scalar",
    "EXP-2026-11-13-508-p7-08-jinja-runtime-stmt",
}

EXTERNAL_ROOTS = [
    Path("/home/end0/AI/LlamaServer"),
]
EXTERNAL_SKIP = ("toolchain/", "/.git/", "third_party/")

NATIVE_RE = re.compile(
    r"((?:common|src|ggml|ggml-cpu|ggml-cuda|examples|tools)/"
    r"[A-Za-z0-9_./*+-]+\.(?:c|cc|cpp|h|hpp|cu|cuh))"
)
EXP_RE = re.compile(r"EXP[- ]?(?:20\d{2}[- ]?\d{2}[- ]?\d{2}[- ]?)?(\d{3,4})")

TOPICS = (
    (("cuda",), "ggml/src/ggml-cuda"),
    (("jinja", "template", "chat"), "common/jinja"),
    (("json", "schema", "peg", "gbnf", "grammar"), "common"),
    (("server", "http", "api", "slot"), "tools/server"),
    (("cpu", "moe", "quant", "allocator", "buffer", "graph", "tensor", "thread", "job"), "ggml/src/ggml-cpu"),
    (("kv", "batch", "sched", "context", "sampl", "token", "gguf", "model", "attn", "rope", "mtp"), "src"),
)


def normalise(path: str) -> str | None:
    path = path.strip().lstrip("./")
    if not path or path.startswith("source/") or path.startswith("qwen/"):
        return None
    if not re.match(r"^(common|src|ggml|examples|tools)/", path):
        return None
    return path


def ledger_map() -> dict[int, dict]:
    doc = json.loads(LEDGER.read_text())
    out: dict[int, dict] = {}
    for task in doc["tasks"]:
        numbers = {int(x) for x in EXP_RE.findall(task.get("evidence", ""))}
        targets = [t for t in (normalise(s) for s in task.get("sources", [])) if t]
        for number in numbers:
            row = out.setdefault(number, {"task": task["id"], "status": task["status"], "targets": []})
            for target in targets:
                if target not in row["targets"]:
                    row["targets"].append(target)
    return out


def readme_targets(folder: Path) -> list[str]:
    text = ""
    readme = folder / "README.md"
    if readme.exists():
        text += readme.read_text(errors="replace")
    for goose_file in list(folder.rglob("*.goose"))[:8]:
        text += "\n" + goose_file.read_text(errors="replace")[:4000]
    return [t for t in (normalise(x) for x in NATIVE_RE.findall(text)) if t]


def topic_target(name: str) -> str | None:
    low = name.lower()
    for keys, area in TOPICS:
        if any(key in low for key in keys):
            return area
    return None


def choose_target(folder: Path, ledger: dict[int, dict]) -> tuple[list[str], str]:
    match = re.search(r"EXP-\d{4}-\d{2}-\d{2}-(\d+)", folder.name)
    number = int(match.group(1)) if match else -1
    if number in ledger and ledger[number]["targets"]:
        return ledger[number]["targets"], ledger[number]["task"]
    found = readme_targets(folder)
    if found:
        return found[:4], ""
    hint = topic_target(folder.name)
    if hint:
        return [hint + "/"], ""
    return [], ""


def destination(area: str, experiment: str, relative: Path) -> Path:
    base = area
    if base.endswith("/**"):
        base = base[:-3]
    if base.endswith(".c") or base.endswith(".cpp") or base.endswith(".h") or base.endswith(".cu") or base.endswith(".cuh"):
        base = str(Path(base).parent)
    base = base.strip("/")
    return GOOSE / base / experiment / relative


def build(check: bool) -> int:
    ledger = ledger_map()
    existing = []
    if (GOOSE / "MANIFEST.json").exists():
        existing = json.loads((GOOSE / "MANIFEST.json").read_text()).get("entries", [])
    canonical_paths = {
        "common/jinja/lexer.goose", "common/jinja/parser.goose",
        "common/jinja/value.goose", "common/jinja/value-json.goose",
        "common/jinja/builtins.goose", "common/jinja/runtime.goose",
    }
    canonical = {row["goose"]: row for row in existing if row["goose"] in canonical_paths}

    entries = []
    written = 0
    if not check:
        for stale in sorted(GOOSE.rglob("EXP-*")):
            if stale.is_dir():
                shutil.rmtree(stale)
        for stale in (GOOSE / "_external", GOOSE / "_unmapped"):
            if stale.exists():
                shutil.rmtree(stale)
    for folder in sorted(EXPERIMENTS.iterdir()):
        if not folder.is_dir() or not folder.name.startswith("EXP-"):
            continue
        files = sorted(folder.rglob("*.goose"))
        if not files or folder.name in CANONICAL:
            continue
        targets, task = choose_target(folder, ledger)
        area = targets[0] if targets else "_unmapped"
        for source in files:
            relative = source.relative_to(folder)
            target = destination(area, folder.name, relative)
            rel = target.relative_to(GOOSE).as_posix()
            entries.append({
                "goose": rel,
                "native": targets[0] if targets else "",
                "native_targets": targets,
                "experiment": folder.name,
                "task": task,
                "status": "slice",
                "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            })
            if not check:
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
                written += 1

    merged = list(canonical.values()) + entries

    if not check:
        seen = {row["sha256"] for row in merged}
        imported = 0
        for base in EXTERNAL_ROOTS:
            if not base.is_dir():
                continue
            for source in base.rglob("*.goose"):
                rel = source.as_posix()
                if any(part in rel for part in EXTERNAL_SKIP):
                    continue
                digest = hashlib.sha256(source.read_bytes()).hexdigest()
                if digest in seen:
                    continue
                seen.add(digest)
                checkout = source.relative_to(base).parts[0]
                inner = source.relative_to(base / checkout)
                target = GOOSE / "_external" / checkout / inner
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
                merged.append({
                    "goose": target.relative_to(GOOSE).as_posix(),
                    "native": "",
                    "native_targets": [],
                    "experiment": "",
                    "task": "",
                    "source": str(source),
                    "status": "external",
                    "sha256": digest,
                })
                imported += 1
        print(f"external sources imported: {imported}")

        branch_imported = 0
        for branch in ("experiments/goose-runtime-20261107",
                       "codex/apex-goose-preserved-20261112"):
            try:
                listing = subprocess.check_output(
                    ["git", "-C", str(ROOT), "ls-tree", "-r", branch, "--", "experiments"],
                    text=True, stderr=subprocess.DEVNULL)
            except subprocess.CalledProcessError:
                continue
            for line in listing.splitlines():
                meta, path = line.split("\t", 1)
                _mode, kind, sha = meta.split()
                if kind != "blob" or not path.endswith(".goose"):
                    continue
                data = subprocess.check_output(["git", "-C", str(ROOT), "cat-file", "blob", sha])
                digest = hashlib.sha256(data).hexdigest()
                if digest in seen:
                    continue
                seen.add(digest)
                target = GOOSE / "_external" / "_branches" / branch.replace("/", "__") / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                merged.append({
                    "goose": target.relative_to(GOOSE).as_posix(),
                    "native": "",
                    "native_targets": [],
                    "experiment": "",
                    "task": "",
                    "source": f"git:{branch}:{path}",
                    "status": "external",
                    "sha256": digest,
                    "git_blob": sha,
                })
                branch_imported += 1
        print(f"branch-only sources imported: {branch_imported}")
    if check:
        problems = []
        for row in entries:
            path = GOOSE / row["goose"]
            if not path.is_file():
                problems.append("missing: " + row["goose"])
            elif hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
                problems.append("drift: " + row["goose"])
        print(f"goose sync check: {len(entries)} slice file(s), {len(problems)} problem(s)")
        for item in problems[:20]:
            print("  " + item)
        return 1 if problems else 0

    doc = {
        "schema": 2,
        "updated_at": datetime.now(timezone.utc).isoformat(timespec="microseconds"),
        "rule": ("goose/ is the working tree; each experiment stays the frozen proof. "
                 "Canonical modules sit at their native path, slice experiments under the "
                 "native directory named by the ledger. Regenerate with "
                 "tools/sync-goose-tree.py."),
        "native_tree": "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda",
        "entries": merged,
    }
    (GOOSE / "MANIFEST.json").write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n")
    print(f"goose tree: {len(merged)} entries, {written} slice file(s) copied")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    return build(args.check)


if __name__ == "__main__":
    sys.exit(main())
