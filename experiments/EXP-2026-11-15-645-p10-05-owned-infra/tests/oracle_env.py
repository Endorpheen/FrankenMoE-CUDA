#!/usr/bin/env python3
"""Native oracle for P10-05 S2 (build-environment preflight).

The native text is executed, not re-typed:

    dir          build.sh:9 ${BUILD_DIR:-...} evaluated by bash
    jobs         build.sh:10 ${JOBS:-8} evaluated by bash
    args         build.sh:16-30 cmake block, run as `printf '%s\\n' <same words>`
    build_type   runtime/CMakeLists.txt:28-30 block, run by `cmake -P`
    subdirs      runtime/CMakeLists.txt:44-72 condition lines, run by `cmake -P`
    pin          prepare_upstreams.sh:27 [[ a == b ]], bash
    integration  prepare_upstreams.sh:63-70 if/elif chain, bash, callees shimmed
    link         prepare_upstreams.sh:76-79 guard on the real filesystem
    markers      prepare_upstreams.sh:42-61 integration_present(), bash, synthetic tree
    options      runtime/CMakeLists.txt:32-36 option() lines evaluated by `cmake -P`
    upstream     prepare_upstreams.sh:38-40 clone_at table, grep + awk

Usage: oracle_env.py CASES
Results: "<row>\\t<value>"; integers decimal, text hex of the raw bytes.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys

sys.dont_write_bytecode = True
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
NATIVE = HERE / "native"
BUILD = (NATIVE / "build.sh").read_text().splitlines()
PREP = (NATIVE / "prepare_upstreams.sh").read_text().splitlines()
CMAKE = (NATIVE / "runtime-CMakeLists.txt").read_text().splitlines()


def sh(script: str, env: dict[str, str] | None = None) -> str:
    e = dict(os.environ)
    for k in ("BUILD_DIR", "JOBS", "CC", "CXX", "CUDACXX", "CUDA_ARCH", "ROOT_DIR"):
        e.pop(k, None)
    if env:
        e.update(env)
    p = subprocess.run(["bash", "-c", script], capture_output=True, text=True, env=e, check=False)
    return p.stdout.strip()


def sh_raw(script: str, env: dict[str, str] | None = None) -> str:
    """Like sh() but keeps trailing whitespace, as a command substitution would."""
    e = dict(os.environ)
    for k in ("BUILD_DIR", "JOBS", "CC", "CXX", "CUDACXX", "CUDA_ARCH", "ROOT_DIR"):
        e.pop(k, None)
    if env:
        e.update(env)
    p = subprocess.run(["bash", "-c", script], capture_output=True, text=True, env=e, check=False)
    return p.stdout


def cmk(script: str, defines: dict[str, str]) -> tuple[str, str]:
    with tempfile.NamedTemporaryFile("w", suffix=".cmake", delete=False) as f:
        f.write(script)
        path = f.name
    args = ["cmake", "-P", path]
    for k, v in defines.items():
        args.insert(1, f"-D{k}={v}")
    try:
        p = subprocess.run(args, capture_output=True, text=True, check=False)
        return p.stdout + p.stderr, p.returncode
    finally:
        os.unlink(path)


def block_cmake_args() -> str:
    """build.sh:9-10 (the assignments the args depend on) plus the cmake call."""
    first = next(i for i, l in enumerate(BUILD) if l.startswith('BUILD_DIR='))
    start = next(i for i, l in enumerate(BUILD) if l.strip().startswith("cmake -S"))
    end = start
    while BUILD[end].rstrip().endswith("\\"):
        end += 1
    head = "\n".join(BUILD[first:start])
    words = " ".join(BUILD[i].rstrip().rstrip("\\").strip() for i in range(start, end + 1))
    return head + "\n" + words.replace("cmake ", "printf '%s\\n' ", 1)


def block_build_type() -> str:
    start = next(i for i, l in enumerate(CMAKE) if l.strip().startswith("if(NOT CMAKE_BUILD_TYPE"))
    return "\n".join(CMAKE[start:start + 3])


def block_integration() -> str:
    start = next(i for i, l in enumerate(PREP) if "apply --reverse --check" in l)
    start -= 1  # the `if` keyword line
    end = next(i for i in range(start, len(PREP)) if PREP[i].strip() == "fi")
    return "\n".join(PREP[start:end + 1])


def block_link() -> str:
    start = next(i for i, l in enumerate(PREP) if 'if [[ -e "${LINK}"' in l)
    end = next(i for i in range(start + 1, len(PREP)) if PREP[i].strip() == "fi")
    return "\n".join(PREP[start:end + 1])


def function_present() -> str:
    start = next(i for i, l in enumerate(PREP) if l.startswith("integration_present()"))
    end = next(i for i in range(start, len(PREP)) if PREP[i].strip() == "}")
    return "\n".join(PREP[start:end + 1])


def marker_table() -> list[tuple[str, str]]:
    body = function_present()
    pairs = re.findall(r'grep -Fq "([^"]*)" "([^"]*)"', body)
    return [(snip, path) for snip, path in pairs]


def subdir_conditions() -> list[tuple[str, str]]:
    """Each `if(BMOE_BUILD_...)` block and the subdirectory it guards."""
    out = []
    for i, l in enumerate(CMAKE):
        if l.startswith("if(BMOE_BUILD_"):
            for j in range(i + 1, len(CMAKE)):
                if CMAKE[j].strip() == "endif()":
                    break
                m = re.search(r"add_subdirectory\((\w+)\)", CMAKE[j])
                if m:
                    out.append((l.strip(), m.group(1)))
                    break
    return out


def decode(field: str) -> str:
    return field.encode().decode("unicode_escape") if "\\" in field else field


def main() -> int:
    cases = [l for l in Path(sys.argv[1]).read_text().splitlines() if l and not l.startswith("#")]
    cmake_args = block_cmake_args()
    bt_block = block_build_type()
    integ_block = block_integration()
    link_block = block_link()
    present_fn = function_present()
    markers = marker_table()
    conditions = subdir_conditions()

    option_lines = [l.strip() for l in CMAKE if l.strip().startswith("option(")]

    out: list[str] = []
    for idx, raw in enumerate(cases, 1):
        f = raw.split("\t")
        kind = f[0]
        if kind == "dir":
            set_, val, root = f[1], decode(f[2]), decode(f[3])
            env = {"ROOT_DIR": root}
            if set_ == "1":
                env["BUILD_DIR"] = val
            out.append(f"{idx}\t{sh_raw('v=\"${BUILD_DIR:-${ROOT_DIR}/build/franken-cuda}\"; printf %s \"$v\"', env).encode().hex()}")
        elif kind == "jobs":
            set_, val = f[1], decode(f[2])
            env = {"JOBS": val} if set_ == "1" else {}
            out.append(f"{idx}\t{sh_raw('v=\"${JOBS:-8}\"; printf %s \"$v\"', env).encode().hex()}")
        elif kind == "args":
            cc_set, cc, cxx_set, cxx, cu_set, cu, ar_set, ar, bd_set, bd, root = f[1:12]
            env = {"ROOT_DIR": decode(root)}
            for flag, name, value in ((cc_set, "CC", cc), (cxx_set, "CXX", cxx), (cu_set, "CUDACXX", cu),
                                      (ar_set, "CUDA_ARCH", ar), (bd_set, "BUILD_DIR", bd)):
                if flag == "1":
                    env[name] = decode(value)
            out.append(f"{idx}\t{sh_raw(cmake_args, env).encode().hex()}")
        elif kind == "build_type":
            set_, val, multi = f[1], decode(f[2]), f[3]
            defines = {}
            if set_ == "1":
                defines["CMAKE_BUILD_TYPE"] = val
            if multi == "1":
                defines["CMAKE_CONFIGURATION_TYPES"] = "Ninja"
            text, _rc = cmk(bt_block + '\nmessage("BT=[${CMAKE_BUILD_TYPE}]")\n', defines)
            m = re.search(r"BT=\[([^\]]*)\]", text)
            out.append(f"{idx}\t{(m.group(1) if m else '?').encode().hex()}")
        elif kind == "subdirs":
            llama, cli, tests, tools = f[1:5]
            script = [f"set(BMOE_HAVE_LLAMA {'ON' if llama == '1' else 'OFF'})"]
            script.append(f"set(BMOE_BUILD_CLI {'ON' if cli == '1' else 'OFF'})")
            script.append(f"set(BMOE_BUILD_TESTS {'ON' if tests == '1' else 'OFF'})")
            script.append(f"set(BMOE_BUILD_TOOLS {'ON' if tools == '1' else 'OFF'})")
            for cond, name in conditions:
                script.append(cond)
                script.append(f'message("S:{name}")')
                script.append("endif()")
            text, _rc = cmk("\n".join(script) + "\n", {})
            found = sorted(re.findall(r"S:(\w+)", text))
            # the unconditional core subdirectory is outside the if chain
            found = ["core"] + found
            out.append(f"{idx}\t{','.join(found).encode().hex()}")
        elif kind == "pin":
            a, b = decode(f[1]), decode(f[2])
            out.append(f"{idx}\t{run_pin(a, b)}")
        elif kind == "integration":
            reverse, mark = f[1], f[2]
            script = (f'REVERSE={reverse}\nMARKERS={mark}\n'
                      'git() {\n'
                      '    case "$*" in\n'
                      '        *--reverse*) if [ "$REVERSE" = 1 ]; then return 0; else return 1; fi;;\n'
                      '        *) return 0;;\n'
                      '    esac\n'
                      '}\n'
                      'integration_present() { if [ "$MARKERS" = 1 ]; then return 0; else return 1; fi; }\n'
                      + integ_block + "\n")
            text = sh(script)
            code = 1 if "already applied" in text else 2 if "preserved" in text else 3
            out.append(f"{idx}\t{code}")
        elif kind == "link":
            exists, symlink = f[1], f[2]
            with tempfile.TemporaryDirectory() as td:
                link = str(Path(td) / "llama.cpp")
                target = Path(td) / "target"
                target.mkdir()
                if symlink == "1":
                    os.symlink(target, link)
                elif exists == "1":
                    Path(link).write_text("regular\n")
                script = f'LINK="{link}"\n' + link_block + "\n"
                p = subprocess.run(["bash", "-c", script], capture_output=True, text=True, check=False)
                out.append(f"{idx}\t{1 if p.returncode == 1 else 0}")
        elif kind == "markers":
            flags = f[1]
            with tempfile.TemporaryDirectory() as td:
                work = Path(td) / "work"
                # Two markers share tools/server/server.cpp, so the file keeps
                # every snippet that is present, as the patched tree does.
                files: dict[str, str] = {}
                for i, (snippet, rel) in enumerate(markers):
                    body = files.setdefault(rel, "")
                    if i < len(flags) and flags[i] == "1":
                        files[rel] = body + snippet + "\n"
                for rel, body in files.items():
                    path = work / rel.replace("${WORK_DIR}/", "")
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text(body or "/* no */\n")
                script = f'WORK_DIR="{work}"\n' + present_fn + "\nintegration_present\n"
                p = subprocess.run(["bash", "-c", script], capture_output=True, text=True, check=False)
                out.append(f"{idx}\t{1 if p.returncode == 0 else 0}")
        elif kind == "options":
            script = ["\n".join(option_lines)]
            for line in option_lines:
                name = re.search(r"option\((\w+)", line).group(1)
                script.append(f'message("O:{name}=${{{name}}}")')
            text, _rc = cmk("\n".join(script) + "\n", {})
            rows = re.findall(r"O:(\w+)=(\S+)", text)
            body = "".join(f"{n}\t{v}\n" for n, v in rows)
            out.append(f"{idx}\t{body.encode().hex()}")
        elif kind == "upstream":
            text = sh("grep -E '^clone_at ' '" + str(NATIVE / 'prepare_upstreams.sh') +
                      "' | awk '{print $2\"\\t\"$3\"\\t\"$4\"\\t\"$5}'")
            out.append(f"{idx}\t{(text + chr(10)).encode().hex()}")
        else:
            raise SystemExit(f"unknown case kind: {kind}")
    print("\n".join(out))
    return 0


def run_pin(a: str, b: str) -> str:
    p = subprocess.run(["bash", "-c", '[[ "$1" == "$2" ]] && echo 1 || echo 0', "_", a, b],
                       capture_output=True, text=True, check=False)
    return p.stdout.strip()


if __name__ == "__main__":
    raise SystemExit(main())
