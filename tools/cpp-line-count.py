#!/usr/bin/env python3
"""Count physical source lines in the current main checkout."""

import argparse
import hashlib
import json
import os
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


def git(*args):
    return subprocess.check_output(["git", *args])


def category(path):
    suffix = PurePosixPath(path).suffix
    if suffix in {".C", ".H"} or suffix.lower() in CPP_SUFFIXES:
        return LABELS[0]
    if suffix == ".h":
        return LABELS[1]
    return None


def raise_scan_error(error):
    raise error


def scan_disk(root):
    files = []
    totals = dict.fromkeys(LABELS, 0)
    for directory, subdirectories, names in os.walk(root, onerror=raise_scan_error):
        parent = Path(directory)
        subdirectories[:] = sorted(
            name for name in subdirectories
            if name != ".git" and not os.path.lexists(parent / name / ".git")
        )
        for name in sorted(names):
            label = category(name)
            if not label:
                continue
            path = parent / name
            info = path.lstat()
            if not stat.S_ISREG(info.st_mode):
                continue
            lines = 0
            digest = hashlib.sha256()
            last = b""
            with path.open("rb") as source:
                for chunk in iter(lambda: source.read(1 << 20), b""):
                    lines += chunk.count(b"\n")
                    digest.update(chunk)
                    last = chunk[-1:]
                read_info = os.fstat(source.fileno())
            identity = (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns)
            if identity != (read_info.st_dev, read_info.st_ino, read_info.st_size, read_info.st_mtime_ns):
                raise RuntimeError(f"File changed during the scan: {path}")
            lines += bool(last and last != b"\n")
            totals[label] += lines
            files.append({"path": path.relative_to(root).as_posix(), "lines": lines, "sha256": digest.hexdigest()})
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
        raise SystemExit("both revisions must be on the first-parent history of main")
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
    branch = subprocess.run(["git", "symbolic-ref", "--quiet", "--short", "HEAD"], capture_output=True)
    if branch.returncode or branch.stdout.strip() != b"main":
        parser.error("run this counter in the main checkout")
    if args.git:
        compare_commits(*args.git)
        return

    started = time.monotonic()
    root = Path(os.fsdecode(git("rev-parse", "--show-toplevel").rstrip(b"\n"))).resolve()
    head = git("rev-parse", "HEAD").strip().decode()
    files, totals = scan_disk(root)
    snapshot = {"version": SNAPSHOT_VERSION, "root": str(root), "head": head, "totals": totals, "files": files}
    print(f"main {head} | {len(files)} source files | {time.monotonic() - started:.2f} s")
    if args.against:
        baseline = json.loads(args.against.read_text())
        if baseline["version"] != SNAPSHOT_VERSION or baseline["root"] != str(root):
            parser.error("snapshot version or repository root differs")
        print("Scope\tBefore\tDecrease in scope\tAfter")
        for label in LABELS:
            before = baseline["totals"][label]
            print(f"{label}\t{before}\t{before - totals[label]}\t{totals[label]}")
    else:
        for label in LABELS:
            print(f"{label}: {totals[label]}")
    if args.save:
        with args.save.open("x", encoding="utf-8") as target:
            json.dump(snapshot, target, ensure_ascii=True, indent=2)
            target.write("\n")


if __name__ == "__main__":
    main()
