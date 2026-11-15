#!/usr/bin/env python3
"""Twelve semantic mutations over the three P8-06 slices.

Each entry rewrites one decision in a module, rebuilds that slice at -O0 and
runs its gate. A mutation is caught when the gate exits non-zero; a mutation
that breaks the build counts as caught too, because a module that does not
compile cannot serve the runtime.

Usage:
    python3 tests/mutate.py 0 6      # first half
    python3 tests/mutate.py 6 12     # second half
    python3 tests/mutate.py          # all twelve

Every source is restored after its mutation and the tree is rebuilt green at the
end, so an interrupted run leaves at most one edited file: rerun `git diff` /
`sha256sum` to check.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# (slice, excerpt that must occur exactly once, replacement, what the gate sees)
MUTATIONS: list[tuple[str, str, str, str]] = [
    (
        "model-manager",
        """fn mg_is_running(st: i64) -> i64 {
    if st == MG_LOADED || st == MG_LOADING || st == MG_SLEEPING {""",
        """fn mg_is_running(st: i64) -> i64 {
    if st == MG_LOADED || st == MG_LOADING {""",
        "a sleeping child stops counting as running",
    ),
    (
        "model-manager",
        """            if status == MG_LOADING {
                terminate = 1;
                send_exit = 0;""",
        """            if status == MG_LOADING {
                terminate = 0;
                send_exit = 0;""",
        "unloading a loading model no longer force-kills it",
    ),
    (
        "model-manager",
        "if victim < 0 || last_used[r] < victim_last_used {",
        "if victim < 0 || last_used[r] <= victim_last_used {",
        "the LRU walk takes the last of equal timestamps",
    ),
    (
        "model-manager",
        """    out[3] = 0;
    if args_status == MG_UNLOADED {
        out[3] = 1;
    }""",
        """    out[3] = 1;""",
        "the SSE status change always carries exit_code",
    ),
    (
        "hot-reload",
        """            if hr_eq_lit(opts, pos, kl, "LLAMA_ARG_ALIAS") {
                skip = 1;
            }
""",
        "",
        "the preset comparison keeps the alias option",
    ),
    (
        "hot-reload",
        """        } else if status[r] == HR_DOWNLOADED {
            erase = 1; // the download finished and was added back by this load""",
        """        } else if status[r] == HR_DOWNLOADED {
            erase = 0;""",
        "a finished download is never erased",
    ),
    (
        "hot-reload",
        """                            if hr_has(masks[o], b) != 0 {
                                clash = 1;
                            }
""",
        "",
        "an alias held by another model no longer clashes",
    ),
    (
        "hot-reload",
        "if count > models_max {",
        "if count >= models_max {",
        "the startup guard refuses a list that fits",
    ),
    (
        "connectors",
        """        if cn_space(key[i]) == 0 {""",
        """        if key[i] != 0 {""",
        "a pasted share code keeps its whitespace",
    ),
    (
        "connectors",
        """            if cn_eq_lit(hosts, h, l, "::") {
                return cn_puts(out, 0, "[::1]");
            }
""",
        "",
        "a wildcard IPv6 bind has no loopback address",
    ),
    (
        "connectors",
        """    if subproc_supported == 0 {
        return 1;
    }
    if has_code != 0 && code_valid == 0 {
        return 2;
    }""",
        """    if has_tcp == 0 {
        return 3;
    }
    if has_code != 0 && code_valid == 0 {
        return 2;
    }""",
        "the tunnel reasons lose their order",
    ),
    (
        "connectors",
        """            if cn_kind_draft(k) == 0 {
                mmproj = 1;""",
        """            if cn_kind_draft(k) == 0 {
                mmproj = 0;""",
        "the mmproj sidecar no longer turns MMPROJ_AUTO on",
    ),
]


def run(args: list[str]) -> int:
    return subprocess.run(args, cwd=str(ROOT), capture_output=True, text=True).returncode


def main() -> int:
    lo = int(sys.argv[1]) if len(sys.argv) > 2 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(MUTATIONS)

    missed = 0
    for i in range(lo, hi):
        name, old, new, why = MUTATIONS[i]
        path = ROOT / "source" / f"{name}.goose"
        src = path.read_text()
        if src.count(old) != 1:
            print(f"{i:2d} BAD-PATTERN ({src.count(old)} matches) {why}")
            missed += 1
            continue
        path.write_text(src.replace(old, new))
        try:
            rc = run(["bash", "tests/build.sh", "mutate", name])
        finally:
            path.write_text(src)
        if rc != 0:
            print(f"{i:2d} caught   rc={rc}  {name}: {why}")
        else:
            missed += 1
            print(f"{i:2d} MISSED   rc=0  {name}: {why}")

    # leave the tree green again
    for name in ("model-manager", "hot-reload", "connectors"):
        run(["bash", "tests/build.sh", "final", name])
    print(f"mutations={hi - lo} missed={missed}")
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
