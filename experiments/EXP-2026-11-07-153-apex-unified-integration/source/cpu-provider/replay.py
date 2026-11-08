#!/usr/bin/env python3
"""Commutative, source-bound replay for the APX013 worker-6 CPU provider.

The native arm runs the qualified runtime with GGML_GOOSE_CPU_BACKEND=0. The
provider arm is the generated Goose unit plus the default-on dispatcher. The
optional bridge arm records the currently installed Goose bridge so its
divergences from native are visible rather than hidden.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE_FILES = [
    "provider.goose",
    "codec/decode.goose",
    "provider-generated.c",
    "provider-unit.c",
    "provider-dispatch.c",
    "native-vs-goose-fixture.c",
]


def load_jsonl(path):
    records = []
    with Path(path).open("r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line:
                records.append(json.loads(line))
    return records


def source_hashes():
    return {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
            for name in SOURCE_FILES}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--native", default="corpus-native.jsonl")
    ap.add_argument("--repeat", default="")
    ap.add_argument("--bridge", default="corpus-bridge.jsonl")
    ap.add_argument("--out", default="replay.json")
    args = ap.parse_args()

    native = load_jsonl(ROOT / args.native)
    repeat = load_jsonl(ROOT / args.repeat) if args.repeat and (ROOT / args.repeat).exists() else []
    bridge = load_jsonl(ROOT / args.bridge) if (ROOT / args.bridge).exists() else []
    sources = source_hashes()
    root = hashlib.sha256("".join(f"{k}:{v}\n" for k, v in sorted(sources.items())).encode()).hexdigest()

    failures = []
    provider_cases = 0
    ops = set()
    negatives = 0
    xor_hash = 0
    per_case = {}

    for rec in native:
        if "name" not in rec or rec.get("negative"):
            continue
        if "native_direct" not in rec:
            continue
        provider_cases += 1
        ops.add(rec.get("op"))
        if not rec.get("native_direct", False):
            failures.append(f"{rec['name']}: native != direct")
        if not rec.get("native_binding", False):
            failures.append(f"{rec['name']}: native != dispatcher")
        canonical = "|".join(str(rec.get(k, "")) for k in
                             ("name", "op", "threads", "bytes", "native", "direct", "binding"))
        bound = hashlib.sha256((root + "|" + canonical).encode()).digest()
        value = int.from_bytes(bound, "big")
        xor_hash ^= value
        per_case[rec["name"]] = bound.hex()

    for rec in native:
        if rec.get("negative"):
            negatives += 1
            if not rec.get("pass", False):
                failures.append(f"{rec['name']}: negative case failed")
        if rec.get("name") == "workspace-callback" and not rec.get("legacy_insufficient"):
            failures.append("workspace-callback: corrected size did not exceed legacy size")
        if rec.get("name") == "use-ref-fallback" and not rec.get("pass", False):
            failures.append("use-ref-fallback: use_ref did not fall back cleanly")
        if rec.get("internal") and not rec.get("pass", False):
            failures.append(f"{rec['name']}: internal stride invariant failed")

    repeat_mismatches = []
    repeat_by_name = {r.get("name"): r for r in repeat if "name" in r}
    for rec in native:
        other = repeat_by_name.get(rec.get("name"))
        if not other:
            continue
        for key in ("native", "direct", "binding"):
            if rec.get(key) != other.get(key):
                repeat_mismatches.append(f"{rec['name']}:{key}")
                break

    bridge_mismatches = []
    bridge_by_name = {r.get("name"): r for r in bridge if "name" in r}
    for rec in native:
        other = bridge_by_name.get(rec.get("name"))
        if not other or "native" not in rec or "native" not in other:
            continue
        if rec["native"] != other["native"]:
            bridge_mismatches.append(rec["name"])

    summary = {
        "schema": "fg006.commutative-replay/1",
        "source_root": root,
        "source_hashes": sources,
        "provider_cases": provider_cases,
        "negative_cases": negatives,
        "ops_covered": sorted(x for x in ops if x is not None),
        "commutative_hash": f"{xor_hash:064x}",
        "case_hashes": per_case,
        "provider_failures": failures,
        "repeat_cases": len(repeat_by_name),
        "repeat_mismatches": repeat_mismatches,
        "current_bridge_cases": len(bridge_by_name),
        "current_bridge_mismatches": bridge_mismatches,
        "status": "PASS" if not failures and not repeat_mismatches else "FAILED",
    }
    if repeat_mismatches:
        failures.extend("repeat:" + item for item in repeat_mismatches)
    (ROOT / args.out).write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")

    print(json.dumps({
        "status": summary["status"],
        "provider_cases": provider_cases,
        "negative_cases": negatives,
        "ops_covered": summary["ops_covered"],
        "commutative_hash": summary["commutative_hash"],
        "repeat_cases": len(repeat_by_name),
        "repeat_mismatches": len(repeat_mismatches),
        "current_bridge_cases": summary["current_bridge_cases"],
        "current_bridge_mismatches": len(bridge_mismatches),
        "provider_failures": len(failures),
    }, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
