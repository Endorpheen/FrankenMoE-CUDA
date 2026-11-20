#!/usr/bin/env bash
# A runtime set must carry every library it needs.
#
# Found by accident on 2026-11-19: builds/checkpoint-20261119-EXP1476 has 23 files while the release has
# 24, and the missing one - libmoe_mmid_tiled.so - is required by libggml-cpu.so.0. The checkpoint
# therefore does not load on its own, although every hash it contains is correct. A hash-matching set that
# cannot start is the kind of "verified" this project keeps having to correct.
#
# The check reads DT_NEEDED from every ELF in the set and asks whether the name exists in the set. System
# libraries (libc, libm, libstdc++ and friends) are exempt: a set is not expected to carry the C runtime.
#
# Tier: fast - readelf and ls, no build.
#
# Usage: bash tests/regression/set-self-contained/run.sh [set-dir]
#        default: the newest /home/end0/AI/LlamaServer/frankenmoe-release-* directory
set -uo pipefail
export LC_ALL=C
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"

SET=${1:-}
if [ -z "$SET" ]; then
    for c in /home/end0/AI/LlamaServer/frankenmoe-release-*; do [ -d "$c" ] && SET=$c; done
fi
[ -n "${SET:-}" ] && [ -d "$SET" ] || { printf 'skip: no runtime set given or found\n' | tee "$LOG"; exit 77; }

{
    printf 'set: %s\n' "$SET"
    printf '%s\n' '$ python3 - "$SET"'
    python3 - "$SET" <<'PY'
import pathlib, re, subprocess, sys

s = pathlib.Path(sys.argv[1]).resolve()
# libssl and libcrypto are system libraries in the ordinary sense: a runtime set is not expected to ship
# OpenSSL, but the dependency is real and was missing from F1-03's first pass, which read ldd of libllama
# only. Found here, recorded there.
# The names need a boundary after them. Without one, "libm" matches "libmoe_mmid_tiled.so" and the very
# dependency this check exists for is written off as a system library - which is exactly what the first
# version of this file did.
SYSTEM = re.compile(r"^(libc|libm|libdl|libpthread|librt|libgcc_s|libstdc\+\+|libgomp|ld-linux|linux-vdso|"
                    r"libssl|libcrypto|libz|"
                    r"libcuda|libcudart|libcublas|libcublasLt|libnvidia)([.-]|$)")
members = sorted(p for p in s.iterdir() if p.suffix == ".so" or ".so." in p.name or p.name == "llama-server")
if not members:
    print("no ELF files in the set"); sys.exit(77)

elfs = {}
for p in members:
    current = p
    seen = set()
    while current.is_symlink():
        target = current.readlink()
        if current in seen or target.parent != pathlib.Path(".") or not target.name:
            print(f"INVALID: {p.name} does not link to a local member of the flat set")
            sys.exit(1)
        seen.add(current)
        current = s / target
    if not current.is_file():
        print(f"INVALID: {p.name} does not resolve to a regular file")
        sys.exit(1)
    result = subprocess.run(["readelf", "-d", str(p)], capture_output=True, text=True)
    if result.returncode:
        print(f"INVALID: {p.name}: {result.stderr.strip()}")
        sys.exit(1)
    if "Dynamic section at offset" not in result.stdout:
        print(f"INVALID: {p.name} has no ELF dynamic section")
        sys.exit(1)
    elfs[p.name] = result.stdout

missing, checked = [], 0
for owner, output in elfs.items():
    for m in re.finditer(r"\(NEEDED\).*?\[([^\]]+)\]", output):
        name = m.group(1)
        checked += 1
        if SYSTEM.match(name) or name in elfs:
            continue
        missing.append((owner, name))

print(f"elf files: {len(elfs)}, NEEDED entries: {checked}")
if missing:
    print(f"MISSING: {len(missing)} dependenc(ies) the set does not carry")
    for owner, name in missing[:10]:
        print(f"  {owner} needs {name}")
    sys.exit(1)
print("ok: every non-system dependency the set needs is in the set")
PY
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
