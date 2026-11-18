#!/usr/bin/env python3
"""Selective local gates; generated files never enter the source checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

E = Path(__file__).resolve().parents[1]
ROOT = E.parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--step", choices=["check", "build", "gate", "mutations"], required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    if not out.is_relative_to(Path("/home/end0/AI/LlamaServer")):
        raise SystemExit("output must be inside LlamaServer")
    out.mkdir(parents=True, exist_ok=True)
    pin = json.loads((ROOT / "runtime/compiler/current.json").read_text())
    goose = Path(pin["compiler"])
    if hashlib.sha256(goose.read_bytes()).hexdigest() != pin["sha256"]:
        raise SystemExit("compiler pin mismatch")
    source = E / "source/runtime.goose"
    key = hashlib.sha256(source.read_bytes()).hexdigest() + pin["sha256"]
    keyfile = out / "input-key"
    if keyfile.exists() and keyfile.read_text() != key:
        raise SystemExit("source/compiler changed; use a new output directory")
    keyfile.write_text(key)
    library = out / "layout.so"
    with (out / (args.step + ".log")).open("x") as log:
        def run(command, expected=0):
            command = list(map(str, command))
            line = "$ " + shlex.join(command)
            print(line, flush=True)
            log.write(line + "\n")
            log.flush()
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
            print(result.stdout, end="", flush=True)
            log.write(result.stdout + f"\nexit_code={result.returncode}\n")
            log.flush()
            if expected == 0 and result.returncode != 0:
                raise RuntimeError(f"command failed: {result.returncode}")
            if expected != 0 and (result.returncode == 0 or "AssertionError" not in result.stdout):
                raise RuntimeError("mutation must fail the semantic oracle, not merely crash")
            return result

        def compile_module(src, directory, optimization):
            directory.mkdir(parents=True, exist_ok=True)
            lib = directory / "layout.so"
            if lib.exists():
                raise RuntimeError("refusing to overwrite a binary")
            run([goose, "--standalone", "-o", directory / "runtime.c", "--header", directory / "runtime.h", src])
            run(["gcc-13", optimization, "-g", "-fPIC", "-shared", "-std=c11", directory / "runtime.c", "-lm", "-o", lib])
            return lib

        gate = [sys.executable, E / "tests/gate.py"]
        if args.step == "check":
            run([goose, "check", source])
        elif args.step == "build":
            compile_module(source, out, "-O1")
        elif args.step == "gate":
            run([*gate, library, "--pack", args.pack])
        else:
            text = source.read_text()
            mutations = [
                ("exponent-bias", "exponent - 7", "exponent - 8"),
                ("sign", "value >= 128", "value > 128"),
                ("subnormal", "0.001953125", "0.00390625"),
                ("special-values", "exponent == 15 && mantissa == 7", "exponent == 15 && mantissa == 6"),
                ("table-scale", "0.00019931793212890625", "0.0002002716064453125"),
                ("scale-guard", "scale != e0_ple_supported_scale()", "scale == 0.0"),
            ]
            for name, old, new in mutations:
                if text.count(old) != 1:
                    raise RuntimeError(f"mutation anchor mismatch: {name}")
                directory = out / name
                directory.mkdir(exist_ok=False)
                mutant = directory / "runtime.goose"
                mutant.write_text(text.replace(old, new))
                lib = compile_module(mutant, directory, "-O0")
                run([*gate, lib, "--pack", args.pack], expected=1)
                print("KILLED", name, flush=True)
                log.write("KILLED " + name + "\n")
        log.write("RESULT PASS\n")
    print("RESULT PASS", args.step, flush=True)


if __name__ == "__main__":
    main()
