#!/usr/bin/env python3
"""Compose once, check cheaply, then test only the small provider ABI."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

E = Path(__file__).resolve().parents[1]
ROOT = E.parents[1]
MODULES = [E.parent / "EXP-2026-11-17-909-edge0-layout/source/runtime.goose",
           E.parent / "EXP-2026-11-17-910-edge0-ple-fp8/source/runtime.goose", E / "source/runtime.goose"]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--step", choices=["check", "build", "fault-build", "gate", "gate-retry", "mutations"], required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    if not out.is_relative_to(Path("/home/end0/AI/LlamaServer")):
        raise SystemExit("output must be inside LlamaServer")
    out.mkdir(parents=True, exist_ok=True)
    pin = json.loads((ROOT / "runtime/compiler/current.json").read_text())
    goose = Path(pin["compiler"])
    if hashlib.sha256(goose.read_bytes()).hexdigest() != pin["sha256"]:
        raise SystemExit("compiler pin mismatch")
    inputs = [*MODULES, E / "source/pread.c", E / "tests/io_faults.c", ROOT / "tools/compose-goose.py"]
    key = pin["sha256"] + "".join(hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs)
    keyfile = out / "input-key"
    if keyfile.exists() and keyfile.read_text() != key:
        raise SystemExit("build inputs changed; use a new output directory")
    keyfile.write_text(key)
    composed = out / "composed.goose"
    with (out / (args.step + ".log")).open("x") as log:
        def run(command, reject=False):
            command = list(map(str, command))
            line = "$ " + shlex.join(command)
            print(line, flush=True)
            log.write(line + "\n")
            log.flush()
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
            print(result.stdout, end="", flush=True)
            log.write(result.stdout + f"\nexit_code={result.returncode}\n")
            log.flush()
            if not reject and result.returncode != 0:
                raise RuntimeError(f"command failed: {result.returncode}")
            if reject and (result.returncode == 0 or "AssertionError" not in result.stdout):
                raise RuntimeError("mutation must fail a semantic assertion")

        def compile_faults(directory, optimization, name="faults.so"):
            target = directory / name
            if target.exists():
                raise RuntimeError("refusing to overwrite a binary")
            stem = "test-pread-" + Path(name).stem
            shim = directory / (stem + ".c")
            text = (E / "source/pread.c").read_text()
            assert text.count("= pread(") == 1
            text = text.replace('#include "runtime.h"', '#include "runtime.h"\nextern ssize_t e0_test_pread(int, void *, size_t, off_t);')
            shim.write_text(text.replace("= pread(", "= e0_test_pread("))
            cc = ["gcc-13", optimization, "-g", "-fPIC", "-std=c11", "-I", directory]
            run([*cc, "-c", shim, "-o", directory / (stem + ".o")])
            run([*cc, "-shared", "-Wl,-z,defs", directory / "runtime.c", directory / (stem + ".o"), E / "tests/io_faults.c", "-lm", "-o", target])

        def compile_module(source, directory, optimization):
            if (directory / "layout.so").exists() or (directory / "faults.so").exists():
                raise RuntimeError("refusing to overwrite a binary")
            run([goose, "--standalone", "-o", directory / "runtime.c", "--header", directory / "runtime.h", source])
            cc = ["gcc-13", optimization, "-g", "-fPIC", "-std=c11", "-I", directory]
            run([*cc, "-shared", "-Wl,-z,defs", directory / "runtime.c", E / "source/pread.c", "-lm", "-o", directory / "layout.so"])
            compile_faults(directory, optimization)

        gate = [sys.executable, E / "tests/gate.py"]
        if args.step == "check":
            if composed.exists():
                raise RuntimeError("refusing to overwrite a composed source")
            run([sys.executable, ROOT / "tools/compose-goose.py", composed, *MODULES])
            run([goose, "check", composed])
        elif args.step == "build":
            compile_module(composed, out, "-O1")
        elif args.step == "fault-build":
            compile_faults(out, "-O1", "faults-v2.so")
        elif args.step in ("gate", "gate-retry"):
            extra = ["--fault-library", out / "faults-v2.so"] if (out / "faults-v2.so").exists() else []
            run([*gate, out / "layout.so", "--pack", args.pack, *extra])
        else:
            text = composed.read_text()
            validation = "while i < rows.len {\n        if e0_ple_desc(rows[i], desc) != 0 {\n            return -2;\n        }\n        i = i + 1;\n    }\n    i = 0;"
            commit = "if done != 160 {\n            return -5;\n        }\n        i = i + 1;"
            early = "if done != 160 {\n            return -5;\n        }\n        if e0_ple_decode_row(scratch[i * 160 .. i * 160 + 160], scale, out[i * 160 .. i * 160 + 160]) != 0 { return -6; }\n        i = i + 1;"
            mutations = [("offset", "desc[1] + done", "desc[1] + done + 1"),
                         ("short-read", "done = done + got;", "done = 160;"),
                         ("EINTR", "got != -4", "got != -5"),
                         ("retry-bound", "attempts < 192", "attempts < 193"),
                         ("partial-output", commit, early),
                         ("validation-order", validation, "i = 0;")]
            for name, old, new in mutations:
                if text.count(old) != 1:
                    raise RuntimeError(f"mutation anchor mismatch: {name}")
                directory = out / name
                directory.mkdir(exist_ok=False)
                source = directory / "composed.goose"
                source.write_text(text.replace(old, new))
                compile_module(source, directory, "-O0")
                run([*gate, directory / "layout.so", "--pack", args.pack], reject=True)
                log.write("KILLED " + name + "\n")
                print("KILLED", name, flush=True)
        log.write("RESULT PASS\n")
    print("RESULT PASS", args.step, flush=True)


if __name__ == "__main__":
    main()
