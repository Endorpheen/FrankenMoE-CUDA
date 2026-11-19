#!/usr/bin/env python3
"""Audit what a shipped set actually links, and where each dependency comes from.

P10-06 lists a compile/link manifest audit as its own item. The question it answers is the one a
release reader asks: which of these libraries are the project's, which are the platform's, and is
anything asked for that the set does not contain.

Usage: python3 tools/audit-link-manifest.py <set-dir> [--json] [--strict-runpath]

A dependency the set does not contain is always a failure. A RUNPATH into a build tree is a warning,
because the accepted set of 2026-11-19 carries one and the fix belongs to the next build; pass
--strict-runpath to make it a failure too, which is what a release gate should do.
"""
import json
import re
import subprocess
import sys
from pathlib import Path


def needed(path: Path) -> list[str]:
    out = subprocess.run(["readelf", "-d", str(path)], capture_output=True, text=True).stdout
    return re.findall(r"\(NEEDED\).*\[([^\]]+)\]", out)


def runpath(path: Path) -> str | None:
    out = subprocess.run(["readelf", "-d", str(path)], capture_output=True, text=True).stdout
    m = re.search(r"\((?:RUNPATH|RPATH)\).*\[([^\]]*)\]", out)
    return m.group(1) if m else None


def exported(path: Path) -> int:
    out = subprocess.run(["nm", "-D", "--defined-only", str(path)], capture_output=True, text=True).stdout
    return sum(1 for line in out.splitlines() if line.strip())


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__.strip())
        return 2
    as_json = "--json" in sys.argv
    strict_runpath = "--strict-runpath" in sys.argv
    d = Path(sys.argv[1]).resolve()
    if not d.is_dir():
        print(f"no such set: {d}")
        return 2

    shipped = {p.name for p in d.glob("*.so*")} | {p.name for p in d.glob("llama-server")}
    report = {"set": str(d), "libraries": [], "warnings": []}
    for lib in sorted(d.glob("*.so*")) + sorted(d.glob("llama-server")):
        deps = needed(lib)
        rows = []
        for dep in deps:
            if dep in shipped:
                origin = "shipped"
            elif Path("/usr/lib/x86_64-linux-gnu", dep).exists() or Path("/lib/x86_64-linux-gnu", dep).exists():
                origin = "system"
            else:
                origin = "MISSING"
                report["warnings"].append(f"{lib.name} needs {dep}, which the set does not contain")
            rows.append({"name": dep, "origin": origin})
        rp = runpath(lib)
        if rp and "/home/" in rp:
            report["warnings"].append(f"{lib.name} carries a RUNPATH into a build tree: {rp}")
        report["libraries"].append({
            "name": lib.name,
            "bytes": lib.stat().st_size,
            "exported_symbols": exported(lib),
            "runpath": rp,
            "needs": rows,
        })

    missing = [w for w in report["warnings"] if "does not contain" in w]
    foreign = [w for w in report["warnings"] if "RUNPATH" in w]
    if as_json:
        print(json.dumps(report, indent=1))
        return 1 if missing or (strict_runpath and foreign) else 0

    print(f"link manifest audit: {len(report['libraries'])} artefact(s) in {d}")
    print(f"{'artefact':32} {'bytes':>10} {'exports':>8}  needs")
    for row in report["libraries"]:
        kinds = {}
        for dep in row["needs"]:
            kinds[dep["origin"]] = kinds.get(dep["origin"], 0) + 1
        summary = ", ".join(f"{k}={v}" for k, v in sorted(kinds.items())) or "-"
        print(f"{row['name']:32} {row['bytes']:10} {row['exported_symbols']:8}  {summary}")
    if foreign:
        print(f"\n{len(foreign)} artefact(s) advertise a build path (RUNPATH); "
              "the next build should link with -Wl,-rpath,'$ORIGIN':")
        for w in foreign[:3]:
            print(f"  {w}")
        if len(foreign) > 3:
            print(f"  ... and {len(foreign) - 3} more")
    if missing:
        print("\nMISSING dependencies:")
        for w in missing:
            print(f"  {w}")
    if not report["warnings"]:
        print("\nno warnings")
    return 1 if (missing or (strict_runpath and foreign)) else 0


if __name__ == "__main__":
    raise SystemExit(main())
