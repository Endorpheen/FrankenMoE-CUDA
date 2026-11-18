#!/usr/bin/env python3
"""Selective, bounded ABI/numerical gates with immutable logs and artifacts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

E = Path(__file__).resolve().parents[1]
ROOT = E.parents[1]
MODULES = [ROOT / "goose/cpu/goose-graph-builders.goose",
           E.parent / "EXP-2026-11-17-909-edge0-layout/source/runtime.goose", E / "source/runtime.goose"]


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--support", type=Path, required=True)
    ap.add_argument("--pack", type=Path, required=True)
    ap.add_argument("--step", choices=["check", "build", "fixture", "gate", "mutations"], required=True)
    ap.add_argument("--fixture-name", default="gate")
    a = ap.parse_args()
    out, support = a.out.resolve(), a.support.resolve()
    if not out.is_relative_to(Path("/home/end0/AI/LlamaServer")):
        raise SystemExit("artifacts must remain inside LlamaServer")
    if not a.fixture_name.replace("-", "").isalnum():
        raise SystemExit("invalid fixture name")
    out.mkdir(parents=True, exist_ok=True)
    pin = json.loads((ROOT / "runtime/compiler/current.json").read_text())
    goose = Path(pin["compiler"])
    assert sha(goose) == pin["sha256"], "compiler pin mismatch"
    binding = ROOT / "integration/goose-standalone/graph-bindings.c"
    inputs = {str(p): sha(p) for p in [*MODULES, binding, ROOT / "tools/compose-goose.py"]}
    inputs["compiler"] = pin["sha256"]
    for name in ["libggml-base.so.0", "libggml-cpu.so.0"]:
        inputs[str(support / "bin" / name)] = sha(support / "bin" / name)
    keyfile = out / "build-inputs.json"
    if keyfile.exists() and json.loads(keyfile.read_text()) != inputs:
        raise SystemExit("module/backend inputs changed; use a new candidate directory")
    keyfile.write_text(json.dumps(inputs, indent=2) + "\n")
    stem = a.step + ("-" + a.fixture_name if a.step in ("fixture", "gate", "mutations") else "")
    (out / (stem + ".pid")).write_text(str(os.getpid()) + "\n")
    include = ROOT / "vendor/flint/ggml/include"
    lib = out / "latent.so"
    binary = out / a.fixture_name
    with (out / (stem + ".log")).open("x") as log:
        def run(cmd, reject=False, env=None):
            cmd = list(map(str, cmd))
            label = "$ " + ("env LD_PRELOAD=" + shlex.quote(env["LD_PRELOAD"]) + " " if env and "LD_PRELOAD" in env else "") + shlex.join(cmd)
            print(label, flush=True); log.write(label + "\n"); log.flush()
            result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, timeout=180, env=env)
            print(result.stdout, end="", flush=True)
            log.write(result.stdout + f"\nexit_code={result.returncode}\n"); log.flush()
            if reject:
                if result.returncode != 3 or "FAIL " not in result.stdout:
                    raise RuntimeError("mutation must fail a semantic assertion")
            elif result.returncode:
                raise RuntimeError("command failed: " + str(result.returncode))

        def compile_unit(directory, source):
            target = directory / "latent.so"
            if target.exists():
                raise RuntimeError("refusing to overwrite a library")
            run([goose, "--standalone", "-o", directory / "runtime.c", "--header", directory / "runtime.h", source])
            run(["gcc-13", "-O1", "-g", "-fPIC", "-shared", "-Wl,-z,defs", directory / "runtime.c", out / "bindings.o",
                 support / "bin/libggml-base.so.0", "-Wl,-rpath," + str(support / "bin"), "-lm", "-o", target])

        if a.step == "check":
            run([sys.executable, ROOT / "tools/compose-goose.py", out / "unit.goose", *MODULES])
            run([goose, "check", out / "unit.goose"])
        elif a.step == "build":
            run(["gcc-13", "-O1", "-g", "-fPIC", "-std=c11", "-I", include, "-c", binding, "-o", out / "bindings.o"])
            compile_unit(out, out / "unit.goose")
        elif a.step == "fixture":
            if binary.exists():
                raise RuntimeError("refusing to overwrite a fixture")
            (out / (a.fixture_name + "-inputs.json")).write_text(json.dumps({str(E / "tests/gate.cpp"): sha(E / "tests/gate.cpp")}, indent=2) + "\n")
            run(["g++-13", "-std=c++17", "-O1", "-g", "-I", include, "-I", out,
                 E / "tests/gate.cpp", lib, support / "bin/libggml-cpu.so.0", support / "bin/libggml-base.so.0",
                 "-lcrypto", "-lm", "-Wl,--export-dynamic", "-Wl,-rpath," + str(support / "bin"), "-o", binary])
        elif a.step == "gate":
            before = {str(p): [p.stat().st_size, p.stat().st_mtime_ns, p.stat().st_ino]
                      for p in a.pack.rglob("*") if p.is_file()}
            run(["/usr/bin/time", "-f", "component_maxrss_KiB=%M", binary, a.pack])
            after = {str(p): [p.stat().st_size, p.stat().st_mtime_ns, p.stat().st_ino]
                     for p in a.pack.rglob("*") if p.is_file()}
            if before != after:
                raise RuntimeError("pack changed during numerical gate")
            (out / "pack-stats.json").write_text(json.dumps(after, indent=2) + "\n")
            log.write("pack_stats_unchanged=1\n")
        else:
            text = (out / "unit.goose").read_text()
            mutants = [("inverse-scale", "inverse[i] = 1.0 / scales[i];", "inverse[i] = scales[i];"),
                       ("missing-scale", "nodes[1] = el_mul(c, nodes[0], inverse, work);", "nodes[1] = nodes[0];"),
                       ("activation", "el_begin(work, 3, c, nodes[2], 0, 10);", "el_begin(work, 3, c, nodes[2], 0, 7);"),
                       ("gate-up-order", "nodes[2] = el_mm(c, weights[2], nodes[1], work);", "nodes[2] = el_mm(c, weights[3], nodes[1], work);"),
                       ("group-offset", "matrix * group_bytes * 8 + group * group_bytes;", "matrix * group_bytes * 8 + group * group_bytes + 64;")]
            for name, old, new in mutants:
                if text.count(old) != 1:
                    raise RuntimeError("mutation anchor mismatch: " + name)
                directory = out / name; directory.mkdir(exist_ok=False)
                source = directory / "unit.goose"; source.write_text(text.replace(old, new))
                run([goose, "check", source]); compile_unit(directory, source)
                run([binary, a.pack], reject=True, env={**os.environ, "LD_PRELOAD": str(directory / "latent.so")})
                log.write("KILLED " + name + "\n"); print("KILLED", name, flush=True)
        log.write("RESULT PASS\n")
    print("RESULT PASS", a.step, flush=True)


if __name__ == "__main__":
    main()
