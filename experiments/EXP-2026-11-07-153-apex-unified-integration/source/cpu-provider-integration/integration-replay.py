#!/usr/bin/env python3
"""Replay normal ggml_backend execution with and without the provider integration."""
import argparse, hashlib, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def load(path):
    rows = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            rows.append(json.loads(line))
    return rows


def cases(rows):
    return {r["name"]: r for r in rows if "name" in r}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--native", required=True)
    ap.add_argument("--integrated", required=True)
    ap.add_argument("--rollback", required=True)
    ap.add_argument("--fallback", default="")
    ap.add_argument("--integrated-log", default="")
    ap.add_argument("--fallback-log", default="")
    ap.add_argument("--out", default="integration-replay.json")
    args = ap.parse_args()

    native = load(args.native)
    integ = load(args.integrated)
    roll = load(args.rollback)
    fallback = load(args.fallback) if args.fallback and Path(args.fallback).exists() else []
    ncase, icase, rcase = cases(native), cases(integ), cases(roll)
    fcase = cases(fallback)
    failures = []

    for name, rec in ncase.items():
        if name not in icase or icase[name].get("hash") != rec.get("hash"):
            failures.append(f"integrated:{name}")
        if name not in rcase or rcase[name].get("hash") != rec.get("hash"):
            failures.append(f"rollback:{name}")
        if args.fallback and (name not in fcase or fcase[name].get("hash") != rec.get("hash")):
            failures.append(f"fallback:{name}")

    def summary(rows):
        for r in rows:
            if "summary" in r:
                return r
        return {}

    si, sr = summary(integ), summary(roll)
    if not si.get("integration_present"):
        failures.append("integrated: preload stats missing")
    if not si.get("dispatcher", 0):
        failures.append("integrated: dispatcher not used")
    if not si.get("planner", 0):
        failures.append("integrated: planner not used")
    if not (si.get("n8", 0) > si.get("n1", 0)):
        failures.append("integrated: workspace n8 <= n1")
    if not si.get("planner_need", 0):
        failures.append("integrated: provider workspace need missing")
    if si.get("planner_old", 0) < si.get("planner_need", 0):
        failures.append("integrated: planner did not cover provider workspace")
    if not si.get("control", 0):
        failures.append("integrated: control runtime not used")
    if si.get("control_threads", 0) < 8:
        failures.append("integrated: control did not see 8 threads")
    if sr.get("dispatcher", 0) or sr.get("planner_increases", 0) or sr.get("control", 0):
        failures.append("rollback: integration still active")
    sf = summary(fallback) if fallback else {}
    if fallback:
        if not sf.get("planner_increases", 0):
            failures.append("fallback: planner did not increase workspace")
        if not (sf.get("planner_old", 0) < sf.get("planner_need", 0)):
            failures.append("fallback: planner old >= need")

    def read_log(path):
        if not path:
            return ""
        for candidate in (Path(path), ROOT / path, ROOT.parent / path):
            if candidate.exists():
                return candidate.read_text(encoding="utf-8", errors="replace")
        return ""

    log = read_log(args.integrated_log)
    flog = read_log(args.fallback_log)
    witness_ops = sorted({int(m.group(1)) for m in re.finditer(r"FG006_GOOSE op=(\d+)", log)})
    for op in (40, 100, 34):
        if op not in witness_ops:
            failures.append(f"witness: FG006_GOOSE op={op} missing")
    plan_lines = [line for line in (flog or log).splitlines() if line.startswith("FG006_PLAN")]
    if not plan_lines:
        failures.append("witness: FG006_PLAN missing")

    source = {}
    for name in ("provider-integration.c", "backend-integration-fixture.c"):
        source[name] = hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
    result = {
        "schema": "fg006.integration-replay/1",
        "status": "PASS" if not failures else "FAILED",
        "native_cases": len(ncase),
        "integrated_cases": len(icase),
        "rollback_cases": len(rcase),
        "fallback_cases": len(fcase),
        "integrated_summary": si,
        "rollback_summary": sr,
        "fallback_summary": sf if fallback else {},
        "witness_ops": witness_ops,
        "witness_plan_lines": len(plan_lines),
        "witness_fallback_plan_lines": len([line for line in flog.splitlines() if line.startswith("FG006_PLAN")]),
        "source_hashes": source,
        "failures": failures,
    }
    (ROOT / args.out).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps({k: result[k] for k in ("status", "native_cases", "integrated_cases",
          "rollback_cases", "fallback_cases", "witness_ops", "witness_plan_lines",
          "witness_fallback_plan_lines", "failures")}, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
