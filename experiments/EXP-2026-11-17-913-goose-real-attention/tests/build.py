#!/usr/bin/env python3
"""Selective real-weight attention gates; no server/full-runtime build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import subprocess
import sys
import time

E = Path(__file__).resolve().parents[1]
ROOT = E.parents[1]
MODULES = [ROOT / "goose/cpu/goose-graph-builders.goose", ROOT / "goose/model/qgate-split.goose", E / "source/attention.goose"]
MODEL_SHA = "c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--support", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--step", choices=["model-hash", "check", "build", "fixture", "gate", "mutations", "integrate", "main-check"], required=True)
    args = parser.parse_args()
    parent_out, support, model = args.out.resolve(), args.support.resolve(), args.model.resolve()
    out = parent_out / "integrated" if args.step in ("integrate", "main-check") else parent_out
    if not out.is_relative_to(Path("/home/end0/AI/LlamaServer")):
        raise SystemExit("outputs must be inside LlamaServer")
    out.mkdir(parents=True, exist_ok=True)
    (out / (args.step + ".pid")).write_text(str(os.getpid()) + "\n")
    pin = json.loads((ROOT / "runtime/compiler/current.json").read_text())
    goose = Path(pin["compiler"])
    if sha(goose) != pin["sha256"]:
        raise SystemExit("compiler pin mismatch")
    key = {str(p): sha(p) for p in [*MODULES, E / "source/projections.c", ROOT / "integration/goose-standalone/graph-bindings.c"]}
    key["compiler"] = pin["sha256"]
    keypath = out / "build-inputs.json"
    if keypath.exists() and json.loads(keypath.read_text()) != key:
        raise SystemExit("module/binding inputs changed; use a new output directory")
    keypath.write_text(json.dumps(key, indent=2) + "\n")
    with (out / (args.step + ".log")).open("x") as log:
        def run(command, env=None, reject=False):
            command = list(map(str, command))
            label = "$ " + shlex.join(command)
            if env is not None:
                controlled = [key + "=" + env[key] for key in ("GGML_CPU_DISABLE_FUSION", "LD_PRELOAD") if key in env]
                label = "$ env " + " ".join(shlex.quote(word) for word in controlled) + " " + shlex.join(command)
            print(label, flush=True)
            log.write(label + "\n")
            log.flush()
            result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, timeout=180)
            print(result.stdout, end="", flush=True)
            log.write(result.stdout + f"\nexit_code={result.returncode}\n")
            log.flush()
            if reject and (result.returncode != 3 or "FAIL " not in result.stdout):
                raise RuntimeError("mutation must fail a semantic assertion")
            if not reject and result.returncode:
                raise RuntimeError(f"command failed: {result.returncode}")

        include = ROOT / "vendor/flint/ggml/include"
        library = out / "attention.so"
        if args.step == "model-hash":
            before = model.stat()
            digest = hashlib.sha256()
            started = time.monotonic()
            with model.open("rb") as file:
                os.posix_fadvise(file.fileno(), 0, 0, os.POSIX_FADV_NOREUSE)
                while block := file.read(8 * 2**20):
                    digest.update(block)
            after = model.stat()
            stable = (before.st_size, before.st_mtime_ns, before.st_ino) == (after.st_size, after.st_mtime_ns, after.st_ino)
            valid = stable and digest.hexdigest() == MODEL_SHA and after.st_size == 21713463136
            receipt = {"model": str(model), "bytes": after.st_size, "sha256": digest.hexdigest(),
                       "expected_sha256": MODEL_SHA, "stable": stable, "inode": after.st_ino,
                       "mtime_ns": after.st_mtime_ns, "seconds": time.monotonic() - started,
                       "result": "PASS" if valid else "FAIL"}
            (out / "model-proof.json").write_text(json.dumps(receipt, indent=2) + "\n")
            log.write(json.dumps(receipt) + "\n")
            if not valid:
                raise RuntimeError("model integrity failed")
        elif args.step == "check":
            run([sys.executable, ROOT / "tools/compose-goose.py", out / "unit.goose", *MODULES])
            run([goose, "check", out / "unit.goose"])
        elif args.step == "build":
            if library.exists():
                raise RuntimeError("refusing to overwrite a library")
            run([goose, "--standalone", "-o", out / "runtime.c", "--header", out / "runtime.h", out / "unit.goose"])
            run(["gcc-13", "-O1", "-g", "-fPIC", "-std=c11", "-I", include,
                 "-c", ROOT / "integration/goose-standalone/graph-bindings.c", "-o", out / "bindings.o"])
            run(["gcc-13", "-O1", "-g", "-fPIC", "-shared", "-Wl,-z,defs", out / "runtime.c", out / "bindings.o",
                 support / "bin/libggml-base.so.0", "-Wl,-rpath," + str(support / "bin"), "-lm", "-o", library])
        elif args.step == "integrate":
            if library.exists() or (out / "gate").exists():
                raise RuntimeError("refusing to overwrite an integration artifact")
            # Reuse the already emitted unit; only the binding location changed.
            run(["gcc-13", "-O1", "-g", "-fPIC", "-c", parent_out / "runtime.c", "-o", out / "runtime.o"])
            run(["gcc-13", "-O1", "-g", "-fPIC", "-std=c11", "-I", include, "-c",
                 ROOT / "integration/goose-standalone/graph-bindings.c", "-o", out / "bindings.o"])
            run(["gcc-13", "-shared", "-Wl,-z,defs", out / "runtime.o", out / "bindings.o", support / "bin/libggml-base.so.0",
                 "-Wl,-rpath," + str(support / "bin"), "-lm", "-o", library])
            run(["g++-13", "-std=c++17", "-O1", "-g", "-I", include, "-I", parent_out, E / "tests/gate.cpp", library,
                 support / "bin/libggml-cpu.so.0", support / "bin/libggml-base.so.0", "-lcrypto", "-lm", "-Wl,--export-dynamic",
                 "-Wl,-rpath," + str(support / "bin"), "-o", out / "gate"])
            proof = json.loads((parent_out / "model-proof.json").read_text())
            current = model.stat()
            if proof["result"] != "PASS" or proof["inode"] != current.st_ino or proof["mtime_ns"] != current.st_mtime_ns:
                raise RuntimeError("model integrity receipt is stale")
            run(["/usr/bin/time", "-f", "gate_maxrss_KiB=%M", out / "gate", model],
                env={**os.environ, "GGML_CPU_DISABLE_FUSION": "0"})
        elif args.step == "main-check":
            text = (ROOT / "tools/build-main-runtime.sh").read_text()
            block = text.split("MODULES=(", 1)[1].split("\n)", 1)[0]
            modules = [ROOT / name for name in shlex.split(block, comments=True)]
            run([sys.executable, ROOT / "tools/compose-goose.py", out / "main.goose", *modules])
            run([goose, "check", out / "main.goose"])
        elif args.step == "fixture":
            binary = out / "gate"
            if binary.exists():
                raise RuntimeError("refusing to overwrite a fixture")
            run(["g++-13", "-std=c++17", "-O1", "-g", "-I", include, "-I", out, E / "tests/gate.cpp", library,
                 support / "bin/libggml-cpu.so.0", support / "bin/libggml-base.so.0", "-lcrypto", "-lm", "-Wl,--export-dynamic",
                 "-Wl,-rpath," + str(support / "bin"), "-o", binary])
        elif args.step in ("gate", "mutations"):
            proof = json.loads((out / "model-proof.json").read_text())
            current = model.stat()
            if proof["result"] != "PASS" or proof["sha256"] != MODEL_SHA or proof["inode"] != current.st_ino or proof["mtime_ns"] != current.st_mtime_ns:
                raise RuntimeError("model proof is stale; recheck the changed model")
            if args.step == "gate":
                run([out / "gate", model], env={**os.environ, "GGML_CPU_DISABLE_FUSION": "0"})
                log.write("child_maxrss_KiB=" + str(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss) + "\n")
            else:
                text = (out / "unit.goose").read_text()
                mutations = [
                    ("pre-norm-scale", "nodes[1] = qa_binary(c, nodes[0], weights[0], 7, work);", "nodes[1] = nodes[0];"),
                    ("QG-split", "head_stride, tok_stride, seq_stride, elem * head_dim);", "head_stride, tok_stride, seq_stride, 0);"),
                    ("RoPE-sections", "work[15] = sections;", "work[15] = 0;"),
                    ("sigmoid-gate", "qa_begin(work, 3, c, gate, 0, 0, 7);", "qa_begin(work, 3, c, gate, 0, 0, 6);"),
                    ("GQA-permutation", "let axis: i64 = if i == 1 { 2 } else { if i == 2 { 1 } else { i } };", "let axis: i64 = i;"),
                    ("causal-mask", "work[7] = mask;", "work[7] = 0;"),
                ]
                for name, old, new in mutations:
                    if text.count(old) != 1:
                        raise RuntimeError("mutation anchor mismatch: " + name)
                    directory = out / name
                    directory.mkdir(exist_ok=False)
                    source = directory / "unit.goose"
                    source.write_text(text.replace(old, new))
                    run([goose, "check", source])
                    run([goose, "--standalone", "-o", directory / "runtime.c", "--header", directory / "runtime.h", source])
                    mutant = directory / "attention.so"
                    run(["gcc-13", "-O0", "-g", "-fPIC", "-shared", "-Wl,-z,defs", directory / "runtime.c", out / "bindings.o",
                         support / "bin/libggml-base.so.0", "-Wl,-rpath," + str(support / "bin"), "-lm", "-o", mutant])
                    run([out / "gate", model], env={**os.environ, "GGML_CPU_DISABLE_FUSION": "0", "LD_PRELOAD": str(mutant)}, reject=True)
                    log.write("KILLED " + name + "\n")
                    print("KILLED", name, flush=True)
        log.write("RESULT PASS\n")
    print("RESULT PASS", args.step, flush=True)


if __name__ == "__main__":
    main()
