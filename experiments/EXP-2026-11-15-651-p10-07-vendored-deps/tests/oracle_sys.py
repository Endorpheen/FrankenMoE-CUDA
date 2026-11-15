#!/usr/bin/env python3
"""P10-07 S3 oracle: the ggml resolution plan, the vendor directory set and the
component-to-vendor linkage, taken from the native CMake files.

  * the two `if()` blocks of CMakeLists.txt:204-212 are extracted from the
    byte-identical tests/native/llama-CMakeLists.txt and executed by `cmake -P`
    with recording stubs for find_package/add_library/add_subdirectory;
  * tests/native/vendor-CMakeLists.txt is executed the same way, so the added
    directory set and its order are CMake's answer, not a list typed here;
  * the vendor libraries each component links are parsed from the real
    target_link_libraries lines of common/mtmd/server.

One outcome has no script-mode oracle: `TARGET ggml` is true when a parent
project already provides the target, which cannot be created in `cmake -P`. That
single boolean is recorded as an approximation in the card; the other two
outcomes are produced by the native condition text itself.

    python3 oracle_sys.py <cases.tsv> <native-dir>

Output: one `<row>\\t<value>` line per case, in file order.
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

CASES = Path(sys.argv[1]).resolve()
NATIVE = Path(sys.argv[2]).resolve()

LLAMA = (NATIVE / "llama-CMakeLists.txt").read_text()
VENDOR = (NATIVE / "vendor-CMakeLists.txt").read_text()

SYSTEM_BLOCK = re.search(r"(?ms)^if \(LLAMA_USE_SYSTEM_GGML\).*?^endif\(\)\n", LLAMA)
INTREE_BLOCK = re.search(r"(?ms)^if \(NOT TARGET ggml AND NOT LLAMA_USE_SYSTEM_GGML\).*?^endif\(\)\n", LLAMA)
if not SYSTEM_BLOCK or not INTREE_BLOCK:
    sys.exit("oracle: ggml resolution blocks not found in the native CMakeLists.txt")
GGML_BLOCKS = SYSTEM_BLOCK.group(0) + "\n" + INTREE_BLOCK.group(0)

STUBS = """function(find_package)
    file(APPEND "${LOG}" "find_package:${ARGV}\\n")
endfunction()
function(add_library)
    file(APPEND "${LOG}" "add_library:${ARGV}\\n")
endfunction()
function(add_subdirectory)
    file(APPEND "${LOG}" "add_subdirectory:${ARGV}\\n")
endfunction()
"""

# vendor directory name -> bit, and component -> target name
BIT = {"hash": 1, "miniaudio": 2, "nlohmann": 4, "sheredom": 8, "stb": 16, "cpp-httplib": 32}
ORDER = ["hash", "miniaudio", "nlohmann", "sheredom", "stb", "cpp-httplib"]
COMPONENT_FILE = {
    1: "common-CMakeLists.txt",
    2: "mtmd-CMakeLists.txt",
    3: "server-CMakeLists.txt",
}


def run_cmake(sources: dict[str, str], workdir: Path) -> list[str]:
    for name, text in sources.items():
        (workdir / name).write_text(text)
    log = workdir / "log.txt"
    script = workdir / "case.cmake"
    body = STUBS + 'set(LOG "' + str(log).replace("\\", "/") + '")\n'
    for name in sources:
        body += 'include("' + str(workdir / name).replace("\\", "/") + '")\n'
    script.write_text(body + "\n")
    r = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True, cwd=str(workdir))
    if r.returncode != 0:
        sys.exit(f"oracle: cmake failed: {r.stdout}\n{r.stderr}")
    return log.read_text().splitlines() if log.exists() else []


def ggml_plan(use_system: int, target_exists: int) -> int:
    with tempfile.TemporaryDirectory(prefix="p1007-") as td:
        wd = Path(td)
        text = f"set(LLAMA_USE_SYSTEM_GGML {use_system})\n" + GGML_BLOCKS
        lines = run_cmake({"ggml-block.cmake": text}, wd)
    plan = 0
    if any(l.startswith("find_package:") for l in lines):
        plan = 2
    elif any(l.startswith("add_subdirectory:") for l in lines):
        plan = 1
    if use_system == 0 and target_exists:
        # recorded approximation: with the target provided by a parent project
        # the native `NOT TARGET ggml` is false and the in-tree block is
        # skipped; the system block does not run either, so the plan is empty
        return 0
    return plan


def vendor_dirs(build_common: int) -> int:
    with tempfile.TemporaryDirectory(prefix="p1007-") as td:
        wd = Path(td)
        text = f"set(LLAMA_BUILD_COMMON {build_common})\n" + VENDOR
        lines = run_cmake({"vendor-dirs.cmake": text}, wd)
    mask = 0
    for l in lines:
        if l.startswith("add_subdirectory:"):
            name = l.split(":", 1)[1].strip()
            mask |= BIT.get(name, 0)
    return mask


def component_deps(component: int) -> int:
    name = COMPONENT_FILE.get(component)
    if name is None:
        return 0
    text = (NATIVE / name).read_text()
    mask = 0
    for line in text.splitlines():
        if "target_link_libraries" not in line:
            continue
        for dep in re.findall(r"vendor::([a-z-]+)", line):
            mask |= BIT.get(dep, 0)
        if re.search(r"\bcpp-httplib\b", line):
            mask |= BIT["cpp-httplib"]
    return mask


def closure(common: int, tools: int, server: int) -> int:
    m = 0
    if common:
        m |= component_deps(1)
    if tools:
        m |= component_deps(2)
    if server:
        m |= component_deps(3)
    return m


def value(kind: str, f: list[str]) -> str:
    if kind == "ggml":
        return str(ggml_plan(int(f[1]), int(f[2])))
    if kind == "dirs":
        return str(vendor_dirs(int(f[1])))
    if kind == "deps":
        return str(component_deps(int(f[1])))
    if kind == "closure":
        return str(closure(int(f[1]), int(f[2]), int(f[3])))
    if kind == "added_not_linked":
        d, c = int(f[1]), int(f[2])
        return str(d & ~c & 63)
    if kind == "linked_not_added":
        d, c = int(f[1]), int(f[2])
        return str(c & ~d & 63)
    raise SystemExit(f"oracle: unknown case kind {kind!r}")


def main() -> int:
    out, row = [], 0
    for line in CASES.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        row += 1
        f = line.split("\t")
        out.append(f"{row}\t{value(f[0], f)}")
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
