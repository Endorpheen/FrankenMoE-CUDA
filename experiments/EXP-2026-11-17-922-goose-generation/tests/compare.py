#!/usr/bin/env python3
"""Compare a Goose generation log with a native control log.

    python3 compare.py <goose.log> <native.log>

Reports the prompt ids, the generated ids and the text of both sides, and exits
non-zero on any difference.
"""
import re
import sys
from pathlib import Path


def parse(path):
    text = Path(path).read_text(errors="replace")
    def ids(tag):
        m = re.search(rf"^{tag} ids=\d+:(.*)$", text, re.M)
        return [int(v) for v in m.group(1).split()] if m else None
    m = re.search(r"^TEXT_BEGIN\n(.*?)\nTEXT_END$", text, re.M | re.S)
    stop = re.search(r"^STOP eog id=(\d+) at step (\d+)$", text, re.M)
    return {"prompt": ids("PROMPT"), "generated": ids("GENERATED"),
            "text": m.group(1) if m else None,
            "stop": (int(stop.group(1)), int(stop.group(2))) if stop else None}


def main():
    a, b = parse(sys.argv[1]), parse(sys.argv[2])
    ok = True
    for key in ("prompt", "generated"):
        same = a[key] == b[key]
        print(f"{key}: {'same' if same else 'DIFFERENT'}")
        print(f"  goose : {a[key]}")
        print(f"  native: {b[key]}")
        ok = ok and same
    same = a["text"] == b["text"]
    print(f"text: {'same' if same else 'DIFFERENT'}")
    print(f"  goose : {a['text']!r}")
    print(f"  native: {b['text']!r}")
    ok = ok and same
    print(f"stop: goose={a['stop']} native={b['stop']}")
    ok = ok and a["stop"] == b["stop"]
    print("RESULT", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
