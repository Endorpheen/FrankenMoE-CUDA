#!/usr/bin/env python3
"""Reproduce the constructor gate using one C unit and recorded support objects."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

E = Path(__file__).resolve().parents[1]
ROOT = E.parents[1]
ORIGINAL = ROOT / "experiments/EXP-2026-11-08-158-goose-graph-builders/source"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--support", type=Path, required=True)
    parser.add_argument("--step", choices=["prepare", "build", "corpus", "boundary", "numeric", "numeric-diagnostic", "numeric-materialized", "mutations", "main-check"], required=True)
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
    inputs = [E / "source/control.goose", E / "source/runtime.goose", E / "source/chain.goose",
              E / "tests/dispatch.c", E / "tests/dispatch.h", ROOT / "integration/goose-standalone/graph-bindings.c"]
    key = {str(p): sha(p) for p in inputs}
    key["compiler"] = pin["sha256"]
    keypath = out / "build-inputs.json"
    if keypath.exists() and json.loads(keypath.read_text()) != key:
        raise SystemExit("inputs changed; preserve this directory and use a new one")
    keypath.write_text(json.dumps(key, indent=2) + "\n")
    with (out / (args.step + ".log")).open("x") as log:
        def run(command, cwd=None, env=None, reject=False):
            command = list(map(str, command))
            label = "$ " + shlex.join(command)
            if cwd:
                label = f"cwd={cwd}\n" + label
            print(label, flush=True)
            log.write(label + "\n")
            log.flush()
            result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, timeout=180)
            print(result.stdout, end="", flush=True)
            log.write(result.stdout + f"\nexit_code={result.returncode}\n")
            log.flush()
            if reject and (result.returncode != 3 or "FAIL " not in result.stdout):
                raise RuntimeError("mutation must fail a semantic assertion")
            if not reject and result.returncode:
                raise RuntimeError(f"command failed: {result.returncode}")
            return result.stdout

        if args.step == "prepare":
            run([sys.executable, ROOT / "tools/compose-goose.py", out / "control.goose", E / "source/control.goose"])
            run([goose, "check", out / "control.goose"])
            if not (out / "candidate.goose").exists():
                run([sys.executable, ROOT / "tools/compose-goose.py", out / "candidate.goose", E / "source/runtime.goose"])
            run([sys.executable, ROOT / "tools/compose-goose.py", out / "standalone.goose",
                 E / "source/runtime.goose", E / "source/chain.goose"])
            run([goose, "check", out / "standalone.goose"])
        elif args.step == "main-check":
            text = (ROOT / "tools/build-main-runtime.sh").read_text()
            block = text.split("MODULES=(", 1)[1].split("\n)", 1)[0]
            modules = shlex.split(block, comments=True)
            paths = [E / "source/runtime.goose" if item == "goose/cpu/goose-graph-builders.goose" else ROOT / item for item in modules]
            run([sys.executable, ROOT / "tools/compose-goose.py", out / "main-candidate.goose", *paths])
            run([goose, "check", out / "main-candidate.goose"])
        elif args.step == "build":
            for filename in ("control.so", "candidate.so", "standalone.so", "constructor-corpus"):
                if (out / filename).exists():
                    raise RuntimeError("refusing to overwrite a binary")
            recipes = json.loads((ORIGINAL / "recipes.json").read_text())
            native_source = ROOT / "vendor/flint/ggml/src/ggml.c"
            text = native_source.read_text()
            for name, row in recipes.items():
                pattern = r"(?m)^[A-Za-z_][\w ]*?\s+(?:\*\s*)?" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{"
                matches = list(re.finditer(pattern, text))
                if len(matches) != 1:
                    raise RuntimeError(f"native signature drift: {name}: {len(matches)} matches")
                fields = [str(row["mode"]), row["c"], row["a"], row["b"], row["d"], row["op"], row["ip"]]
                fields += ["FG158_ARG(" + value + ")" for value in row["extra"]]
                if len(fields) > 16:
                    raise RuntimeError("packet exceeds sixteen fields")
                call = "FG158_CALL(" + ",".join(fields) + ")"
                kind = row["kind"]
                code = call + ";return;" if kind == "void" else "return " + ("(void *)(intptr_t)" if kind == "ptr" else "(" + kind + ")") + call + ";"
                end = matches[0].end()
                text = text[:end] + "\n    if (ggt_test_enabled()) { " + code + " }\n" + text[end:]
            redirected = out / "ggml-test-redirect.c"
            redirected.write_text('#include "dispatch.h"\n' + text)

            database = json.loads((support / "compile_commands.json").read_text())
            record = next(row for row in database if Path(row["file"]).resolve() == native_source)
            command = shlex.split(record["command"])
            native_object = (Path(record["directory"]) / command[command.index("-o") + 1]).resolve()
            command[command.index("-c") + 1] = str(redirected)
            command[command.index("-o") + 1] = str(out / "ggml-test.o")
            command.insert(1, "-I" + str(E / "tests"))
            run(command, cwd=record["directory"])
            raw = run(["ninja", "-C", support, "-t", "commands", "bin/libggml-base.so.0.27.0"])
            last = raw.splitlines()[-1]
            link = shlex.split(last)
            if link[:2] != [":", "&&"] or link[-2:] != ["&&", ":"]:
                raise RuntimeError("unrecognized Ninja link syntax")
            link = link[2:-2]
            provenance = []
            for i, word in enumerate(link):
                if word.endswith(".o"):
                    obj = (support / word).resolve()
                    provenance.append({"path": str(obj), "sha256": sha(obj)})
                    link[i] = str(out / "ggml-test.o") if obj == native_object else str(obj)
                elif word.startswith("-Wl,--dependency-file="):
                    link[i] = "-Wl,--dependency-file=" + str(out / "link.d")
            if str(out / "ggml-test.o") not in link:
                raise RuntimeError("original GGML C object was not replaced")
            (out / "support-inputs.json").write_text(json.dumps(provenance, indent=2) + "\n")
            include = ROOT / "vendor/flint/ggml/include"
            run(["gcc-13", "-O1", "-g", "-fPIC", "-std=c11", "-I", include,
                 "-c", ROOT / "integration/goose-standalone/graph-bindings.c", "-o", out / "bindings.o"])
            for name, typed in (("control", 0), ("candidate", 1)):
                directory = out / name
                directory.mkdir(exist_ok=False)
                run([goose, "--standalone", "-o", directory / "runtime.c", "--header", directory / "runtime.h", out / (name + ".goose")])
                run(["gcc-13", "-O1", "-g", "-fPIC", "-std=c11", "-c", directory / "runtime.c", "-o", directory / "runtime.o"])
                run(["gcc-13", "-O1", "-g", "-fPIC", "-std=c11", "-I", directory,
                     "-DGGT_HAS_TYPED=" + str(typed), "-c", E / "tests/dispatch.c", "-o", directory / "dispatch.o"])
                current = link.copy()
                current[current.index("-o") + 1] = str(out / (name + ".so"))
                current += [str(directory / "runtime.o"), str(directory / "dispatch.o"), str(out / "bindings.o"), "-Wl,-z,defs", "-lm"]
                run(current, cwd=support)
            run(["g++-13", "-std=c++17", "-O1", "-I", include, E / "tests/constructor_corpus.cpp", "-ldl", "-o", out / "constructor-corpus"])
            directory = out / "standalone"
            directory.mkdir(exist_ok=False)
            run([goose, "--standalone", "-o", directory / "runtime.c", "--header", directory / "runtime.h", out / "standalone.goose"])
            run(["gcc-13", "-O1", "-g", "-fPIC", "-shared", "-Wl,-z,defs", directory / "runtime.c", out / "bindings.o",
                 support / "bin/libggml-base.so.0", "-Wl,-rpath," + str(support / "bin"), "-lm", "-o", out / "standalone.so"])
        elif args.step == "corpus":
            env = {**os.environ, "GGML_NO_BACKTRACE": "1", "GGML_GOOSE_GRAPH_BUILDERS": "1"}
            native = support / "bin/libggml-base.so.0"
            for library, route in (("control.so", 0), ("candidate.so", 0), ("candidate.so", 1)):
                run([out / "constructor-corpus", native, out / library, route], env=env)
            run([out / "constructor-corpus", native, out / "candidate.so", 1],
                env={**env, "GGML_GOOSE_GRAPH_BUILDERS": "0"})
        elif args.step == "boundary":
            binary = out / "boundary"
            if binary.exists():
                raise RuntimeError("refusing to overwrite a binary")
            run(["g++-13", "-std=c++17", "-O1", "-I", ROOT / "vendor/flint/ggml/include",
                 "-I", out / "candidate", E / "tests/boundary.cpp", "-ldl", "-o", binary])
            run([binary, out / "control.so", 0])
            run([binary, out / "candidate.so", 1])
        elif args.step in ("numeric", "numeric-diagnostic", "numeric-materialized"):
            binary = out / args.step
            if binary.exists():
                raise RuntimeError("refusing to overwrite a binary")
            run(["g++-13", "-std=c++17", "-O1", "-I", ROOT / "vendor/flint/ggml/include", "-I", out / "standalone",
                 E / "tests/numeric.cpp", out / "standalone.so", support / "bin/libggml-cpu.so.0", support / "bin/libggml-base.so.0",
                 "-Wl,--export-dynamic", "-Wl,-rpath," + str(support / "bin"), "-lm", "-o", binary])
            run([binary], env={**os.environ, "GGML_CPU_DISABLE_FUSION": "0"})
        elif args.step == "mutations":
            text = (out / "candidate.goose").read_text()
            lines = [line for line in (out / "build.log").read_text().splitlines()
                     if line.startswith("$ ") and "-o " + str(out / "candidate.so") in line]
            if len(lines) != 1:
                raise RuntimeError("candidate link record is ambiguous")
            original_link = shlex.split(lines[0][2:])
            for name, old, new in [("short-slice", "if values.len < 16", "if values.len < 15"),
                                   ("argument-seven", "return values[i];", "if i == 7 { return values[8]; }\n    return values[i];")]:
                if text.count(old) != 1:
                    raise RuntimeError("mutation anchor mismatch: " + name)
                directory = out / name
                directory.mkdir(exist_ok=False)
                source = directory / "runtime.goose"
                source.write_text(text.replace(old, new))
                run([goose, "check", source])
                run([goose, "--standalone", "-o", directory / "runtime.c", "--header", directory / "runtime.h", source])
                run(["gcc-13", "-O0", "-g", "-fPIC", "-std=c11", "-c", directory / "runtime.c", "-o", directory / "runtime.o"])
                link = [str(directory / "runtime.o") if word == str(out / "candidate/runtime.o") else word for word in original_link]
                link[link.index("-o") + 1] = str(directory / "mutant.so")
                link = ["-Wl,--dependency-file=" + str(directory / "link.d") if word.startswith("-Wl,--dependency-file=") else word for word in link]
                run(link, cwd=support)
                if name == "short-slice":
                    run([out / "boundary", directory / "mutant.so", 1], reject=True)
                else:
                    run([out / "constructor-corpus", support / "bin/libggml-base.so.0", directory / "mutant.so", 1],
                        env={**os.environ, "GGML_NO_BACKTRACE": "1", "GGML_GOOSE_GRAPH_BUILDERS": "1"}, reject=True)
                log.write("KILLED " + name + "\n")
                print("KILLED", name, flush=True)
        log.write("RESULT PASS\n")
    print("RESULT PASS", args.step, flush=True)


if __name__ == "__main__":
    main()
