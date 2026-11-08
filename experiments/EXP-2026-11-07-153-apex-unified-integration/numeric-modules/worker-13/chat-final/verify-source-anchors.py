#!/usr/bin/env python3
"""Recompute the source-lock and verify the anchored patch is exact."""
import hashlib
import json
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent
lock = json.loads((BASE / "source-lock.json").read_text())


def sha(text: str) -> str:
    return hashlib.sha256(text.encode()).hexdigest()


src = Path(lock["source"])
src_text = src.read_text()
if sha(src_text) != lock["source_sha256"]:
    print("SOURCE_HASH_MISMATCH", file=sys.stderr)
    raise SystemExit(1)

patched = Path("chat-final/patched/chat-apx028.cpp")
if not patched.exists():
    print("PATCHED_MISSING", file=sys.stderr)
    raise SystemExit(1)
patched_text = patched.read_text()
if sha(patched_text) != lock["patched_sha256"]:
    print("PATCHED_HASH_MISMATCH", file=sys.stderr)
    raise SystemExit(1)
if "#include \"apx028-chat-native.h\"" not in patched_text:
    print("INCLUDE_MISSING", file=sys.stderr)
    raise SystemExit(1)
if lock["dispatch"].strip() not in patched_text:
    print("DISPATCH_MISSING", file=sys.stderr)
    raise SystemExit(1)
if patched_text.count("apx028_chat_enabled()") != 1:
    print("DISPATCH_COUNT", patched_text.count("apx028_chat_enabled()"), file=sys.stderr)
    raise SystemExit(1)

missing = [p for p, h in lock["dependencies"].items() if h == "MISSING"]
if missing:
    print("DEPENDENCY_MISSING " + ",".join(missing), file=sys.stderr)
    raise SystemExit(1)

diff = Path("chat-final/patched/chat-apx028.diff")
if not diff.exists() or "+" not in diff.read_text():
    print("DIFF_MISSING", file=sys.stderr)
    raise SystemExit(1)

print(f"ANCHORS_OK source={lock['source_sha256'][:16]} patched={lock['patched_sha256'][:16]} deps={len(lock['dependencies'])}")
