#!/usr/bin/env python3
"""Selective persistent-Q4 KV component gates, never a model server."""
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
MODULES = [ROOT / "goose/cpu/goose-graph-builders.goose", ROOT / "goose/model/qgate-split.goose",
           ROOT / "goose/model/qwen35-attention.goose", E / "source/cache.goose"]
MODEL_SHA = "c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--support", type=Path, required=True)
    parser.add_argument("--model-proof", type=Path, required=True)
    parser.add_argument("--step", choices=["check", "build", "fixture", "gate", "state", "mutations", "main-check"], required=True)
    args = parser.parse_args()
    out, support = args.out.resolve(), args.support.resolve()
    if not out.is_relative_to(Path("/home/end0/AI/LlamaServer")):
        raise SystemExit("output must be inside LlamaServer")
    out.mkdir(parents=True, exist_ok=True)
    (out / (args.step + ".pid")).write_text(str(os.getpid()) + "\n")
    pin = json.loads((ROOT / "runtime/compiler/current.json").read_text())
    goose = Path(pin["compiler"])
    if sha(goose) != pin["sha256"]:
        raise SystemExit("compiler pin mismatch")
    inputs = [*MODULES, ROOT / "integration/goose-standalone/graph-bindings.c"]
    key = {str(p): sha(p) for p in inputs}; key["compiler"] = pin["sha256"]
    keypath = out / "build-inputs.json"
    if keypath.exists() and json.loads(keypath.read_text()) != key:
        raise SystemExit("module/binding inputs changed; preserve artifacts and use a new directory")
    keypath.write_text(json.dumps(key, indent=2) + "\n")
    with (out / (args.step + ".log")).open("x") as log:
        def run(command, env=None, reject=False):
            command = list(map(str, command))
            label = "$ " + shlex.join(command)
            if env:
                controlled = [key + "=" + env[key] for key in ("GGML_CPU_DISABLE_FUSION", "LD_PRELOAD") if key in env]
                label = "$ env " + " ".join(shlex.quote(v) for v in controlled) + " " + shlex.join(command)
            print(label, flush=True); log.write(label + "\n"); log.flush()
            result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180)
            print(result.stdout, end="", flush=True); log.write(result.stdout + f"\nexit_code={result.returncode}\n"); log.flush()
            if reject and (result.returncode not in (1, 3) or not any(s in result.stdout for s in ("FAIL ", "AssertionError"))):
                raise RuntimeError("mutation must fail a semantic assertion")
            if not reject and result.returncode:
                raise RuntimeError(f"command failed: {result.returncode}")

        include = ROOT / "vendor/flint/ggml/include"
        library = out / "cached.so"
        if args.step == "check":
            run([sys.executable, ROOT / "tools/compose-goose.py", out / "unit.goose", *MODULES])
            run([goose, "check", out / "unit.goose"])
        elif args.step == "build":
            if library.exists(): raise RuntimeError("refusing to overwrite a library")
            run([goose, "--standalone", "-o", out / "runtime.c", "--header", out / "runtime.h", out / "unit.goose"])
            run(["gcc-13", "-O1", "-g", "-fPIC", "-std=c11", "-I", include, "-c",
                 ROOT / "integration/goose-standalone/graph-bindings.c", "-o", out / "bindings.o"])
            run(["gcc-13", "-O1", "-g", "-fPIC", "-shared", "-Wl,-z,defs", out / "runtime.c", out / "bindings.o",
                 support / "bin/libggml-base.so.0", "-Wl,-rpath," + str(support / "bin"), "-lm", "-o", library])
        elif args.step == "fixture":
            if (out / "gate").exists(): raise RuntimeError("refusing to overwrite a fixture")
            run(["g++-13", "-std=c++17", "-O1", "-g", "-I", include, "-I", out, E / "tests/gate.cpp", library,
                 support / "bin/libggml-cpu.so.0", support / "bin/libggml-base.so.0", "-lcrypto", "-lm", "-Wl,--export-dynamic",
                 "-Wl,-rpath," + str(support / "bin"), "-o", out / "gate"])
        elif args.step in ("gate", "mutations"):
            proof = json.loads(args.model_proof.read_text())
            model = Path(proof["model"])
            before = model.stat()
            if proof["result"] != "PASS" or proof["sha256"] != MODEL_SHA or proof["inode"] != before.st_ino or proof["mtime_ns"] != before.st_mtime_ns:
                raise RuntimeError("model proof changed; do not silently accept a new model")
            log.write("reused_model_proof_sha256=" + sha(args.model_proof) + "\n")
            if args.step == "gate":
                run(["/usr/bin/time", "-f", "gate_maxrss_KiB=%M", out / "gate", model], env={**os.environ, "GGML_CPU_DISABLE_FUSION": "0"})
            else:
                text = (out / "unit.goose").read_text()
                mutations = [
                    ("write-dependency", "view4(c, nodes[31], head, cfg[1], read_rows", "view4(c, cache_k, head, cfg[1], read_rows", "graph"),
                    ("cursor", "out[i] = state[2] + i;", "out[i] = state[2] + i + 1;", "graph"),
                    ("causal-mask", "if j <= state[2] + i", "if j < state[2] + i", "graph"),
                    ("restore-prefix", "state[2] = meta[2]; state[4] = 0;", "state[2] = meta[2] - 1; state[4] = 0;", "state"),
                    ("restore-values", "values[i] = bytes[n + i];", "values[i] = bytes[i];", "state"),
                    ("cache-layout", "pred(0, cache_k, 0) == 0 || pred(0, cache_v, 0) == 0 ||", "false || false ||", "graph"),
                ]
                for name, old, new, gate in mutations:
                    if text.count(old) != 1: raise RuntimeError("mutation anchor mismatch: " + name)
                    directory = out / name; directory.mkdir(exist_ok=False)
                    source = directory / "unit.goose"; source.write_text(text.replace(old, new))
                    run([goose, "check", source])
                    run([goose, "--standalone", "-o", directory / "runtime.c", "--header", directory / "runtime.h", source])
                    mutant = directory / "cached.so"
                    run(["gcc-13", "-O0", "-g", "-fPIC", "-shared", "-Wl,-z,defs", directory / "runtime.c", out / "bindings.o",
                         support / "bin/libggml-base.so.0", "-Wl,-rpath," + str(support / "bin"), "-lm", "-o", mutant])
                    if gate == "state":
                        run([sys.executable, E / "tests/state.py", mutant], reject=True)
                    else:
                        run([out / "gate", model], env={**os.environ, "GGML_CPU_DISABLE_FUSION": "0", "LD_PRELOAD": str(mutant)}, reject=True)
                    log.write("KILLED " + name + "\n"); print("KILLED", name, flush=True)
            after = model.stat()
            if (before.st_size, before.st_mtime_ns, before.st_ino) != (after.st_size, after.st_mtime_ns, after.st_ino):
                raise RuntimeError("model changed during gate")
        elif args.step == "state":
            run([sys.executable, E / "tests/state.py", library])
        elif args.step == "main-check":
            block = (ROOT / "tools/build-main-runtime.sh").read_text().split("MODULES=(", 1)[1].split("\n)", 1)[0]
            modules = [ROOT / name for name in shlex.split(block, comments=True)]
            run([sys.executable, ROOT / "tools/compose-goose.py", out / "main.goose", *modules])
            run([goose, "check", out / "main.goose"])
        log.write("RESULT PASS\n")
    print("RESULT PASS", args.step, flush=True)


if __name__ == "__main__":
    main()
