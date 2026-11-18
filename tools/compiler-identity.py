#!/usr/bin/env python3
"""Name the compiler that built the runtime artifact, and say whether it is versioned.

A runtime artifact whose compiler is unknown cannot be reproduced, and the compiler
used to have no --version. This reads runtime/compiler/current.json, cross-references
the Fried-Goose version registry, and prints an identity that a report can quote
without asserting anything it did not measure.

The registry lives with the compiler it describes:
  /home/end0/AI/Fried-Goose/.cache/selfhosted-builds/catalog.json
  /home/end0/AI/Fried-Goose/CHANGELOG.md

Exit codes:
  0  the compiler was identified (versioned or explicitly unversioned)
  1  with --strict, the compiler is not in the registry
"""
import argparse
import hashlib
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_CATALOG = "/home/end0/AI/Fried-Goose/.cache/selfhosted-builds/catalog.json"


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load(path):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def find_by_sha(cat, sha):
    if not cat:
        return None, None
    for name, ch in (cat.get("channels") or {}).items():
        cur = ch.get("current") or {}
        if cur.get("sha256") == sha:
            return name, cur
    for v in cat.get("versions") or []:
        if v.get("sha256") == sha:
            return "selfhosted_subset", v
    return None, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--compiler-json",
                    default=os.path.join(ROOT, "runtime", "compiler", "current.json"))
    ap.add_argument("--catalog", default=os.environ.get("FG_GOOSE_CATALOG", DEFAULT_CATALOG))
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    rec = load(a.compiler_json)
    if not rec:
        print("compiler identity: no record at %s" % a.compiler_json, file=sys.stderr)
        return 1

    path = rec.get("compiler") or ""
    sha = rec.get("sha256") or ""
    if not sha and path and os.path.isfile(path):
        sha = sha256_file(path)

    cat = load(a.catalog)
    channel, entry = find_by_sha(cat, sha)
    label = (entry or {}).get("label") if entry else None
    version_id = (entry or {}).get("version") if entry else None

    if entry:
        verdict = "VERSIONED"
    elif cat is None:
        verdict = "REGISTRY_UNREADABLE"
    else:
        verdict = "NOT_IN_REGISTRY"

    out = {
        "compiler": path,
        "sha256": sha,
        "declared_version": rec.get("version") or "",
        "declared_status": rec.get("status") or "",
        "source_commit": rec.get("source_commit") or "",
        "registry": a.catalog,
        "channel": channel,
        "registry_version": version_id,
        "label": label,
        "verdict": verdict,
    }

    if a.json:
        print(json.dumps(out, indent=2))
    else:
        print("compiler   %s" % path)
        print("sha256     %s" % sha)
        print("declared   %s (%s)" % (out["declared_version"], out["declared_status"]))
        print("source     %s" % out["source_commit"])
        print("registry   %s" % a.catalog)
        if entry:
            print("label      %s  (%s, channel %s)" % (label, version_id, channel))
            print("verdict    VERSIONED")
        else:
            print("label      -")
            print("verdict    NOT_IN_REGISTRY - the registry does not know this compiler,")
            print("           so no artifact built with it can be traced to a version.")

    if a.strict and verdict != "VERSIONED":
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
