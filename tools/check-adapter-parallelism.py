#!/usr/bin/env python3
"""Check that no adapter re-implements a Goose decision under the same name.

The project rule is that an adapter is translation only: it moves bytes and calls the
substrate, and the decisions live in Goose. That rule was asserted in a comment. This
turns it into something checkable, because a second implementation with the same name
is either duplication or a plan to bypass the first one.

A hit is not automatically wrong - a host may legitimately need a shim - but it has to
be looked at, so it fails loudly instead of passing quietly.

Usage: tools/check-adapter-parallelism.py [--verbose]
Exit:  0 no parallel definition, 1 at least one
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ADAPTER_DIRS = ["integration/goose-standalone", "integration/m1/host"]

EXPORT_RE = re.compile(r"^\s*export\s+fn\s+([A-Za-z_][A-Za-z0-9_]*)", re.M)


def goose_exports():
    names = {}
    for base, _, files in os.walk(os.path.join(ROOT, "goose")):
        if "_archive" in base:
            continue
        for f in files:
            if not f.endswith(".goose"):
                continue
            path = os.path.join(base, f)
            try:
                with open(path, encoding="utf-8", errors="replace") as fh:
                    text = fh.read()
            except OSError:
                continue
            for m in EXPORT_RE.finditer(text):
                names.setdefault(m.group(1), os.path.relpath(path, ROOT))
    return names


def adapter_sources():
    out = subprocess.run(
        ["find"] + ADAPTER_DIRS + ["-name", "*.c", "-o", "-name", "*.cpp"],
        cwd=ROOT, capture_output=True, text=True, check=False).stdout.split()
    return [p for p in out if "/tests/" not in p]


# A definition, not a call: the name followed by its parameters, at the start of a
# line, optionally preceded by a type. Declarations ending in ';' are not definitions.
DEF_RE = r"^[A-Za-z_][A-Za-z0-9_ \*]*\b{name}\s*\([^;]*\)\s*\{{"


def main():
    verbose = "--verbose" in sys.argv
    exports = goose_exports()
    hits = []
    for src in adapter_sources():
        with open(os.path.join(ROOT, src), encoding="utf-8", errors="replace") as f:
            text = f.read()
        for name, module in exports.items():
            if re.search(DEF_RE.format(name=re.escape(name)), text, re.M):
                hits.append((src, name, module))
    print("goose exports: %d | adapter files: %d" % (len(exports), len(adapter_sources())))
    if verbose:
        for src in adapter_sources():
            print("  adapter %s" % src)
    if not hits:
        print("PASS no adapter defines a function that shares a Goose export name")
        return 0
    for src, name, module in hits:
        print("FAIL %s defines %s, which is exported by %s" % (src, name, module))
    print("result: %d parallel definition(s)" % len(hits))
    return 1


if __name__ == "__main__":
    sys.exit(main())
