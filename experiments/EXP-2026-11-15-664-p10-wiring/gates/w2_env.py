#!/usr/bin/env python3
"""W2 gate: the build preflight in scripts/build.sh.

Arm A (corpus, 27 rows): the accepted P10-05 S2 rows for the five decisions this
script owns — `dir`, `jobs`, `args`, `build_type`, `subdirs` — compared with the
executable oracle (bash and `cmake -P` running the pinned native copies) and with
the host. The `pin`/`integration`/`link`/`markers`/`options`/`upstream` rows belong
to prepare_upstreams.sh and runtime/CMakeLists.txt, which this wiring does not
touch.

Arm B (end to end, 12 environments): the real script, run twice per environment,
with the upstream clone step neutralised and a recording `cmake` on PATH — the two
modes must produce the same cmake argv, the same stdout and the same exit code.

Usage: w2_env.py
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from host import CORPUS, ROOT, Host, Tally, cases, decode, field, hx, oracle  # noqa: E402

ORACLE = CORPUS / "oracle_env.py"
SCRIPT = ROOT / "scripts" / "build.sh"
SUBDIR_BITS = {"core": 1, "cli": 2, "tests": 4, "tools": 8}


def mask_names(mask: int) -> str:
    names = [n for n in ("core", "cli", "tests", "tools") if mask & SUBDIR_BITS[n]]
    return ",".join(names)


def arm_corpus(host) -> Tally:
    t = Tally("FG663_ENV_CORPUS")
    rows = cases(CORPUS / "cases" / "env.tsv")
    want = oracle(ORACLE, str(CORPUS / "cases" / "env.tsv"))
    for i, row in enumerate(rows, 1):
        kind = row[0]
        if kind == "dir":
            set_, val, root = field(row, 1), decode(field(row, 2)), decode(field(row, 3))
            t.rows += 1
            t.eq(i, kind, bytes.fromhex(want[i]), host.text(f"DIR {hx(root)} {set_} {hx(val)}"))
        elif kind == "jobs":
            set_, val = field(row, 1), decode(field(row, 2))
            t.rows += 1
            t.eq(i, kind, bytes.fromhex(want[i]), host.text(f"JOBS {set_} {hx(val)}"))
        elif kind == "args":
            cc_set, cc, cxx_set, cxx, cu_set, cu, ar_set, ar, bd_set, bd, root = row[1:12]
            t.rows += 1
            root, bd = decode(root), decode(bd)
            # build.sh resolves BUILD_DIR first, then hands it to the vector.
            build_dir = host.text(f"DIR {hx(root)} {bd_set} {hx(bd)}")
            got = host.text(f"ARGS {hx(root)} {hx(build_dir)} {cc_set} {hx(decode(cc))} "
                            f"{cxx_set} {hx(decode(cxx))} {cu_set} {hx(decode(cu))} "
                            f"{ar_set} {hx(decode(ar))}")
            # A command substitution drops the trailing newlines; the oracle's
            # text keeps one, so both sides are compared without it.
            t.eq(i, kind, bytes.fromhex(want[i]).rstrip(b"\n"), got.rstrip(b"\n"))
        elif kind == "build_type":
            set_, val, multi = field(row, 1), decode(field(row, 2)), field(row, 3)
            t.rows += 1
            t.eq(i, kind, bytes.fromhex(want[i]), host.text(f"TYPE {set_} {hx(val)} {multi}"))
        elif kind == "subdirs":
            args = [field(row, k) for k in range(1, 5)]
            t.rows += 1
            mask = int(host.ask(f"SUBDIRS {' '.join(args)}"))
            t.eq(i, kind, wanted_names(want[i]), mask_names(mask))
    return t


def wanted_names(value: str) -> str:
    """The oracle returns hex of the comma-joined list."""
    try:
        return bytes.fromhex(value).decode()
    except ValueError:
        return value


def arm_end_to_end() -> Tally:
    t = Tally("FG663_ENV_E2E")
    envs = [
        ("defaults", {}),
        ("build_dir", {"BUILD_DIR": "/tmp/b"}),
        ("build_dir_space", {"BUILD_DIR": "/tmp/b with space"}),
        ("build_dir_relative", {"BUILD_DIR": "rel/build"}),
        ("build_dir_empty", {"BUILD_DIR": ""}),
        ("jobs_four", {"JOBS": "4"}),
        ("jobs_empty", {"JOBS": ""}),
        ("jobs_text", {"JOBS": "abc"}),
        ("compilers", {"CC": "clang-19", "CXX": "clang++-19", "CUDACXX": "/opt/nvcc"}),
        ("arch", {"CUDA_ARCH": "90"}),
        ("compiler_empty", {"CC": ""}),
        ("everything", {"BUILD_DIR": "/tmp/x y", "JOBS": "3", "CC": "gcc-14",
                        "CXX": "g++-14", "CUDACXX": "/usr/local/cuda/bin/nvcc",
                        "CUDA_ARCH": "86"}),
    ]
    with tempfile.TemporaryDirectory() as td:
        fake = Path(td) / "bin"
        fake.mkdir()
        cmake = fake / "cmake"
        cmake.write_text("#!/usr/bin/env bash\nprintf 'CMAKE'\n"
                         "for a in \"$@\"; do printf ' [%s]' \"$a\"; done\nprintf '\\n'\n")
        cmake.chmod(0o755)

        # The gate runs the script with the upstream clone step neutralised: the
        # clone needs the network and is not a decision this wiring moved.
        derived = ROOT / "scripts" / ".w2-build-gate.sh"
        text = SCRIPT.read_text()
        text = text.replace('"${ROOT_DIR}/scripts/prepare_upstreams.sh"', ": # upstream step neutralised by the gate")
        derived.write_text(text)
        try:
            for name, extra in envs:
                results = {}
                for mode in ("0", "1"):
                    env = {k: v for k, v in os.environ.items()
                           if k not in ("BUILD_DIR", "JOBS", "CC", "CXX", "CUDACXX", "CUDA_ARCH")}
                    env["PATH"] = f"{fake}:{env.get('PATH', '')}"
                    env["BMOE_GOOSE_ENV"] = mode
                    env.update(extra)
                    p = subprocess.run(["bash", str(derived)], env=env, capture_output=True,
                                       text=True, check=False)
                    results[mode] = (p.returncode, p.stdout, p.stderr)
                t.rows += 1
                t.eq(t.rows, f"{name} exit", results["0"][0], results["1"][0])
                t.eq(t.rows, f"{name} argv", results["0"][1], results["1"][1])
                t.eq(t.rows, f"{name} stderr", scrub(results["0"][2], mode=False),
                     scrub(results["1"][2], mode=True))
        finally:
            derived.unlink(missing_ok=True)
    return t


def scrub(text: str, mode: bool) -> str:
    """Drop the one legitimate difference: the missing-host notice."""
    if not mode:
        return text
    return "\n".join(l for l in text.splitlines() if "goose-hosts/env" not in l)


def main() -> int:
    host = Host("env")
    try:
        a = arm_corpus(host)
    finally:
        host.close()
    b = arm_end_to_end()
    return a.done() | b.done()


if __name__ == "__main__":
    raise SystemExit(main())
