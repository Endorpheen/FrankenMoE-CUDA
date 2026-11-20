#!/usr/bin/env python3
"""Count physical source lines in main or the isolated deletion checkout."""

import argparse
import hashlib
import json
import os
import re
from pathlib import Path, PurePosixPath
import stat
import subprocess
import time


CPP_SUFFIXES = {
    ".c++", ".cc", ".ccm", ".cp", ".cpp", ".cppm", ".cu", ".cuh", ".cxx", ".cxxm",
    ".h++", ".hh", ".hpp", ".hxx", ".ii", ".inl", ".ipp", ".ixx", ".mm", ".tpp", ".txx",
}
LABELS = ("C++ and CUDA", "Shared .h headers")
SNAPSHOT_VERSION = 1

HEADER_CPP = "C++-bearing .h headers"
MIXED_CPP = "C++ in other source extensions"
EMBEDDED_CPP = "C++-bearing generators and patches"
DISK_LABELS = (*LABELS, HEADER_CPP, MIXED_CPP, EMBEDDED_CPP)
MIXED_SUFFIXES = {".c", ".inc", ".def", ".m"}
EMBEDDED_SUFFIXES = {".py", ".sh", ".patch", ".diff", ".cmake"}
LINKAGE_GUARD = re.compile(
    rb'^[^\S\n]*#\s*(?:ifdef\s+__cplusplus|if\s+defined\s*\(\s*__cplusplus\s*\))[^\S\n]*\n'
    rb'[^\S\n]*(?:extern\s+"C"\s*\{|\})[^\S\n]*\n'
    rb'[^\S\n]*#\s*endif[^\n]*(?:\n|$)',
    re.MULTILINE,
)
NON_CODE = re.compile(rb"""//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'""", re.DOTALL)
CPP_MARKERS = re.compile(
    rb'\b(?:__cplusplus|__CUDACC__|__CUDA_ARCH__|__GNUG__|__global__|__device__|constexpr|consteval|constinit|decltype|nullptr|'
    rb'noexcept|dynamic_cast|reinterpret_cast|static_cast|const_cast|thread_local|typeid|override)\b'
    rb'|\bnamespace\s*(?:[A-Za-z_]\w*|\{)|\btemplate\s*<|\bclass\s+[A-Za-z_]\w*\s*[:{;]'
    rb'|\b(?:public|protected|private)\s*:|\b[A-Za-z_]\w*\s*::\s*[A-Za-z_~]'
    rb'|^[^\S\n]*#\s*include\s*<(?:algorithm|array|atomic|bit|chrono|concepts|cstdint|cstdio|cstdlib|'
    rb'cstring|exception|filesystem|functional|iostream|limits|map|memory|mutex|numeric|optional|'
    rb'set|span|sstream|string|string_view|thread|tuple|type_traits|unordered_map|unordered_set|'
    rb'utility|variant|vector)>',
    re.MULTILINE,
)


def has_cpp_content(data):
    # C headers may bracket a plain C ABI with a C++ linkage guard.
    # Treat other C++ syntax, language-specific branches and CUDA as C++ content.
    code = NON_CODE.sub(lambda item: item[0] if item[0] == b'"C"' else b' ', LINKAGE_GUARD.sub(b'\n', data))
    return bool(CPP_MARKERS.search(code) or re.search(rb'\bextern\s+"C"', code))


def git(*args):
    return subprocess.check_output(["git", *args])


def category(path):
    name = PurePosixPath(path)
    suffix = PurePosixPath(name.stem).suffix if name.suffix == ".in" else name.suffix
    if suffix in {".C", ".H"} or suffix.lower() in CPP_SUFFIXES:
        return LABELS[0]
    if suffix == ".h":
        return LABELS[1]
    return None


def embedded_cpp(data, suffix):
    if suffix in {".patch", ".diff"}:
        for line in data.splitlines():
            if line.startswith((b"+++ ", b"--- ")) and category(os.fsdecode(bytes(line[4:].split(b"\t", 1)[0]))) == LABELS[0]:
                return True
        return False
    return bool(re.search(
        rb'^\s*(?:#\s*include\s*<(?:vector|string|memory|iostream|type_traits|cstdlib|cstdint|cstdio)>'
        rb'|namespace\s+[A-Za-z_]\w*\s*\{|template\s*<|__global__\s+)',
        data, re.MULTILINE,
    ))


def raise_scan_error(error):
    raise error


def scan_disk(root, include_nested=False):
    files = []
    totals = dict.fromkeys(DISK_LABELS, 0)
    for directory, subdirectories, names in os.walk(root, onerror=raise_scan_error):
        parent = Path(directory)
        subdirectories[:] = sorted(
            name for name in subdirectories
            if name != ".git" and (include_nested or not os.path.lexists(parent / name / ".git"))
        )
        for name in sorted(names):
            label = category(name)
            suffix = Path(name).suffix
            embedded = suffix in EMBEDDED_SUFFIXES and name != "cpp-line-count.py"
            check_content = label == LABELS[1] or suffix in MIXED_SUFFIXES or embedded
            if not label and not check_content:
                continue
            path = parent / name
            info = path.lstat()
            if not stat.S_ISREG(info.st_mode):
                continue
            lines = 0
            digest = hashlib.sha256()
            last = b""
            content = bytearray() if check_content else None
            with path.open("rb") as source:
                for chunk in iter(lambda: source.read(1 << 20), b""):
                    lines += chunk.count(b"\n")
                    digest.update(chunk)
                    if content is not None:
                        content.extend(chunk)
                    last = chunk[-1:]
                read_info = os.fstat(source.fileno())
            identity = (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns)
            if identity != (read_info.st_dev, read_info.st_ino, read_info.st_size, read_info.st_mtime_ns):
                raise RuntimeError(f"File changed during the scan: {path}")
            lines += bool(last and last != b"\n")
            cpp_content = content is not None and (embedded_cpp(content, suffix) if embedded else has_cpp_content(content))
            if label is None and not cpp_content:
                continue
            label = label or (EMBEDDED_CPP if embedded else MIXED_CPP)
            totals[label] += lines
            if label == LABELS[1] and cpp_content:
                totals[HEADER_CPP] += lines
            files.append({"path": path.relative_to(root).as_posix(), "lines": lines,
                          "sha256": digest.hexdigest(), "scope": label, "cpp_content": cpp_content})
    return files, totals


def count_blob(batch, oid):
    batch.stdin.write(oid + b"\n")
    batch.stdin.flush()
    name, kind, size = batch.stdout.readline().split()
    if name != oid or kind != b"blob":
        raise RuntimeError(f"Unexpected Git object for {oid.decode()}")
    remaining = int(size)
    lines = 0
    data = b""
    while remaining:
        data = batch.stdout.read(min(remaining, 1 << 20))
        if not data:
            raise RuntimeError(f"Truncated Git object {oid.decode()}")
        remaining -= len(data)
        lines += data.count(b"\n")
    if batch.stdout.read(1) != b"\n":
        raise RuntimeError(f"Missing Git object delimiter for {oid.decode()}")
    return lines + bool(data and not data.endswith(b"\n"))


def count_tree(revision, batch, cached):
    result = dict.fromkeys(LABELS, 0)
    for entry in git("ls-tree", "-rz", "--full-tree", revision).split(b"\0"):
        if not entry:
            continue
        metadata, path = entry.split(b"\t", 1)
        mode, _, oid = metadata.split()
        label = category(path.decode(errors="surrogateescape"))
        if label and mode in {b"100644", b"100755"}:
            if oid not in cached:
                cached[oid] = count_blob(batch, oid)
            result[label] += cached[oid]
    return result


def renamed_lines(before, after, cached):
    moved = dict.fromkeys(LABELS, 0)
    changes = iter(git("diff", "--raw", "-z", "--no-abbrev", "-M100%", before, after).split(b"\0"))
    for meta in changes:
        if not meta:
            continue
        old = category(next(changes).decode(errors="surrogateescape"))
        if meta.endswith(b" R100"):
            new = category(next(changes).decode(errors="surrogateescape"))
            if old != new:
                lines = cached.get(meta.split()[2], 0)
                if old:
                    moved[old] += lines
                if new:
                    moved[new] -= lines
    return moved


def compare_commits(before_ref, after_ref):
    head = git("rev-parse", "--verify", after_ref + "^{commit}").strip().decode()
    base = git("rev-parse", "--verify", before_ref + "^{commit}").strip().decode()
    first_parent = set(git("rev-list", "--first-parent", "HEAD").decode().splitlines())
    if base not in first_parent or head not in first_parent:
        raise SystemExit("both revisions must be on the first-parent history of the current branch")
    cached = {}
    with subprocess.Popen(["git", "cat-file", "--batch"], stdin=subprocess.PIPE, stdout=subprocess.PIPE) as batch:
        after = count_tree(head, batch, cached)
        before = count_tree(base, batch, cached)
        batch.stdin.close()
    if batch.returncode:
        raise subprocess.CalledProcessError(batch.returncode, batch.args)
    print(f"{base} -> {head}")
    moves = renamed_lines(base, head, cached)
    print("Scope\tBefore\tNet removed\tAfter")
    for label in LABELS:
        removed = before[label] - after[label] - moves[label]
        print(f"{label}\t{before[label]}\t{removed}\t{after[label]}")
        if moves[label]:
            print(f"{label}: {moves[label]:+d} lines left the scope through exact renames")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--save", type=Path, help="write a new on-disk snapshot with per-file hashes")
    parser.add_argument("--against", type=Path, help="compare the on-disk tree to a saved snapshot")
    parser.add_argument("--git", nargs=2, metavar=("BEFORE", "AFTER"), help="compare committed trees")
    args = parser.parse_args()
    if args.git and (args.save or args.against):
        parser.error("--git is a separate committed-tree comparison")
    ref = subprocess.run(["git", "symbolic-ref", "--quiet", "--short", "HEAD"], capture_output=True)
    branch = ref.stdout.decode().strip()
    if ref.returncode or branch not in {"main", "deletecplusplus"}:
        parser.error("run this counter in main or deletecplusplus")
    if args.git:
        compare_commits(*args.git)
        return
    if branch == "deletecplusplus" and any(path.startswith(b"S ") for path in git("ls-files", "-t", "-z").split(b"\0")):
        parser.error("materialize every tracked file before counting deletecplusplus")

    started = time.monotonic()
    root = Path(os.fsdecode(git("rev-parse", "--show-toplevel").rstrip(b"\n"))).resolve()
    head = git("rev-parse", "HEAD").strip().decode()
    files, totals = scan_disk(root, include_nested=branch == "deletecplusplus")
    snapshot = {"version": SNAPSHOT_VERSION, "root": str(root), "head": head, "totals": totals, "files": files}
    print(f"{branch} {head} | {len(files)} source files | {time.monotonic() - started:.2f} s")
    if args.against:
        baseline = json.loads(args.against.read_text())
        if baseline["version"] != SNAPSHOT_VERSION or baseline["root"] != str(root):
            parser.error("snapshot version or repository root differs")
        print("Scope\tBefore\tDecrease in scope\tAfter")
        for label in DISK_LABELS:
            if label not in baseline["totals"]:
                print(f"{label}\t-\t-\t{totals[label]}")
                continue
            before = baseline["totals"][label]
            print(f"{label}\t{before}\t{before - totals[label]}\t{totals[label]}")
    else:
        for label in DISK_LABELS:
            print(f"{label}: {totals[label]}")
    if args.save:
        with args.save.open("x", encoding="utf-8") as target:
            json.dump(snapshot, target, ensure_ascii=True, indent=2)
            target.write("\n")


if __name__ == "__main__":
    main()
