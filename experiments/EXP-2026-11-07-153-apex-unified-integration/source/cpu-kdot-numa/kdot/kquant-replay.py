#!/usr/bin/env python3
"""Replay normal ggml_backend Q4_K/Q6_K GEMV with the Goose vec_dot binding."""
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--native", required=True)
    ap.add_argument("--integrated", required=True)
    ap.add_argument("--rollback", required=True)
    ap.add_argument("--integrated-log", required=True)
    ap.add_argument("--rollback-log", required=True)
    ap.add_argument("--dot", default="dot-corpus.jsonl")
    ap.add_argument("--out", default="kquant-replay.json")
    args = ap.parse_args()

    native = {r["name"]: r for r in load(args.native) if "name" in r}
    integ = {r["name"]: r for r in load(args.integrated) if "name" in r}
    roll = {r["name"]: r for r in load(args.rollback) if "name" in r}
    failures = []
    for name, rec in native.items():
        if integ.get(name, {}).get("hash") != rec.get("hash"):
            failures.append(f"integrated:{name}")
        if roll.get(name, {}).get("hash") != rec.get("hash"):
            failures.append(f"rollback:{name}")

    def read_log(path):
        for candidate in (Path(path), ROOT / path, ROOT.parent / path):
            if candidate.exists():
                return candidate.read_text(encoding="utf-8", errors="replace")
        return ""
    ilog = read_log(args.integrated_log)
    rlog = read_log(args.rollback_log)
    m = re.search(r"FG006_KDOT enabled=1 q4=(\d+) q6=(\d+)", ilog)
    if not m or int(m.group(1)) == 0 or int(m.group(2)) == 0:
        failures.append("integrated: vec_dot binding not used")
    if "FG006_KDOT enabled=0" not in rlog:
        failures.append("rollback: binding did not disable")
    for mode in (0, 1):
        if not re.search(rf"FG006_KQUANT mode={mode} calls=\d+", ilog):
            failures.append(f"witness: mode={mode} missing")
    source_hashes = {}
    for name in ("kquant.goose", "kquant-generated.c", "kquant-unit.c",
                 "kquant-binding.c", "backend-kquant-fixture.c", "dot-fixture.c"):
        source_hashes[name] = hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
    root = hashlib.sha256("".join(f"{k}:{v}\n" for k, v in sorted(source_hashes.items())).encode()).hexdigest()

    dot_rows = load(ROOT / args.dot) if (ROOT / args.dot).exists() else []
    dot_cases = 0
    xor_hash = 0
    for rec in dot_rows:
        if "name" not in rec:
            continue
        dot_cases += 1
        if rec.get("negative"):
            if not rec.get("pass", False):
                failures.append(f"dot:{rec['name']}")
            canonical = rec["name"] + "|negative"
        else:
            if not rec.get("q4_equal", False) or not rec.get("q6_equal", False):
                failures.append(f"dot:{rec['name']}")
            canonical = "|".join(str(rec.get(k, "")) for k in
                                 ("name", "type4", "type6", "q4_native", "q4_goose",
                                  "q6_native", "q6_goose", "q4_equal", "q6_equal"))
        bound = hashlib.sha256((root + "|" + canonical).encode()).digest()
        xor_hash ^= int.from_bytes(bound, "big")

    result = {
        "schema": "fg006.kquant-replay/1",
        "status": "PASS" if not failures else "FAILED",
        "source_root": root,
        "source_hashes": source_hashes,
        "dot_cases": dot_cases,
        "dot_commutative_hash": f"{xor_hash:064x}",
        "native_cases": len(native),
        "integrated_cases": len(integ),
        "rollback_cases": len(roll),
        "integrated_binding": m.group(0) if m else "",
        "rollback_log_has_disabled": "FG006_KDOT enabled=0" in rlog,
        "failures": failures,
    }
    (ROOT / args.out).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps({k: result[k] for k in ("status", "native_cases", "integrated_cases",
          "rollback_cases", "dot_cases", "dot_commutative_hash", "integrated_binding",
          "failures")}, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
