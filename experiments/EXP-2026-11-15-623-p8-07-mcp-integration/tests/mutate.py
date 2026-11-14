#!/usr/bin/env python3
"""Mutation gate for the MCP stdio integration (P8-07 S5b). 12 edits in two halves.

Usage: python3 tests/mutate.py [lo hi]

Every mutation rewrites one decision in the composed units and rebuilds at -O0;
a mutation is caught when the gate exits non-zero (a build failure counts).
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC_INT = os.path.join(ROOT, "source", "mcp-integration.goose")
SRC_STDIO = os.path.join(ROOT, "source", "mcp-stdio.goose")
BUILD = os.path.join(ROOT, "tests", "build.sh")

# (title, file, old, new)
M = [
    # half 1: framing and order
    ("the NDJSON frame ends with CRLF",
     SRC_INT,
     """    let p = ms_puts(out, 0, msg);
    return ms_putc(out, p, 10);""",
     """    var p = ms_puts(out, 0, msg);
    p = ms_putc(out, p, 13);
    return ms_putc(out, p, 10);"""),
    ("a pump line keeps its carriage return",
     SRC_INT,
     """            if ll > 0 && buf[ll - 1] == 13 {""",
     """            if false {"""),
    ("the resync drops the rest of the buffer",
     SRC_INT,
     """            bn = ms_erase(buf, bn, nl + 1);
            skipping = 0;""",
     """            bn = 0;
            skipping = 0;"""),
    ("a notification frame is taken as the reply",
     SRC_INT,
     """    if idn < 0 {
        return 0; // notification
    }""",
     """    if idn < 0 {
        return 1; // notification delivered as the reply
    }"""),
    ("the initialized notification is never sent",
     SRC_INT,
     """    if stage == 1 {
        return ms_puts(out, 0, "notifications/initialized");
    }""",
     """    if stage == 1 {
        return ms_puts(out, 0, "tools/list");
    }"""),
    ("any numeric id matches the request",
     SRC_INT,
     """        if p.jv[idn] == req_id {
            return 1;
        }""",
     """        if p.jv[idn] == p.jv[idn] {
            return 1;
        }"""),
    # half 2: timeout, cooldown and errors
    ("a timeout is reported as a closed transport",
     SRC_INT,
     """    if timed_out != 0 {
        return ms_puts(out, 0, "request timed out");
    }""",
     """    if timed_out != 0 {
        return ms_puts(out, 0, "transport closed");
    }"""),
    ("an error reply is taken as a result",
     SRC_INT,
     """    if pget(p, root, "error") >= 0 {
        return 0;
    }""",
     """    if pget(p, root, "error") >= 0 {
        return 1;
    }"""),
    ("diagnostics drop the stderr tail",
     SRC_INT,
     """    if tail.len > 0 {""",
     """    if tail.len < 0 {"""),
    ("the stderr tail keeps the first bytes",
     SRC_INT,
     """    if total > c {
        skip = total - c;
    }""",
     """    if total > c {
        skip = 0;
    }"""),
    ("request ids never advance",
     SRC_INT,
     """export fn mi_next_id(cur: i64) -> i64 {
    return cur + 1;
}""",
     """export fn mi_next_id(cur: i64) -> i64 {
    return cur;
}"""),
    ("a failed start records no cooldown",
     SRC_STDIO,
     """            cool = now + 5000;""",
     """            cool = 0;"""),
]


def run(lo, hi):
    caught = 0
    for i in range(lo, hi):
        title, path, old, new = M[i]
        text = open(path).read()
        if old not in text:
            print("MISS %2d %s: anchor not found in %s" % (i, title, os.path.basename(path)))
            continue
        shutil.copyfile(path, path + ".orig")
        try:
            open(path, "w").write(text.replace(old, new, 1))
            p = subprocess.run(["bash", BUILD], capture_output=True, text=True,
                               env=dict(os.environ, FG_OPT="-O0"))
            out = p.stdout + p.stderr
            summary = ""
            for line in out.splitlines():
                if line.startswith("FG_MCPINT") or "error" in line.lower() and not summary:
                    summary = summary or line
            first = ""
            for line in out.splitlines():
                if line.startswith("FAIL "):
                    first = line.split()[1]
                    break
            if p.returncode != 0:
                caught += 1
                extra = first or ("build failed" if "FG_MCPINT" not in out else summary)
                print("CAUGHT %2d %-52s %s" % (i, title, extra))
            else:
                print("MISSED %2d %s" % (i, title))
        finally:
            os.replace(path + ".orig", path)
    print("mutations %d/%d caught" % (caught, hi - lo))
    return caught == hi - lo


if __name__ == "__main__":
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    sys.exit(0 if run(lo, hi) else 1)
