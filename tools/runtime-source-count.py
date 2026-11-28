#!/usr/bin/env python3
"""Inventory native source lines and universal build inputs without building."""

import argparse
from collections import defaultdict
import json
from pathlib import Path
import re
import subprocess


root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--json", action="store_true", help="include the full per-file inventory")
args = parser.parse_args()


def git(*arguments):
    return subprocess.check_output(["git", "-C", str(root), *arguments]).decode()


source_extensions = set(
    "c h inc cl cpp cc cxx hh hpp hxx cu cuh inl ipp tcc c++ h++ "
    "i ii cp comp glsl ccm cppm cxxm ixx tpp txx".split()
)
files = set()
for name in git("ls-files", "--cached", "--others", "--exclude-standard", "-z").split("\0"):
    if not name:
        continue
    path = Path(name)
    suffix = path.suffix.lower()
    if suffix == ".in":
        suffix = Path(path.stem).suffix.lower()
    if (suffix.lstrip(".") in source_extensions or name.endswith(".cl.patch")) and (root / path).is_file():
        files.add(path)

s=Path('integration/goose-standalone')
u=s/'universal'
g=Path('vendor/flint/ggml')
d=u/'cuda'
marks={}
def mark(base,names,ext,status,reason):
    for name in names.split():
        f=base/(name+ext)
        if f not in files:
            raise SystemExit(f"Universal input missing: {f}. Check the inventory against the build recipe.")
        marks[f]=(status,reason)
both='Host Linux + Windows'
mark(u/'inkling','graph-execute inkling-backend','.c',both,'Graph ABI, execution and Inkling backend')
mark(u/'bonsai','backend-cold model-host','.c',both,'Bonsai session and model host')
mark(u/'common','probability-pipeline','.c',both,'Probability platform adapter')
mark(s/'vulkan','bonsai-model apex-model inkling-model','.c',both,'Vulkan backend: corresponding loader, compute, model or HTTP component')
mark(Path('integration/m1/host'),'goose_host','.c',both,'Goose ABI, platform and quantization helpers')
mark(g/'src','ggml','.c',both,'GGML tensor and graph core')
images=d/'images'
names=' '.join(line.split()[0].removesuffix('.h') for line in (root/images/'inkling-images.txt').read_text().splitlines() if line.strip())
gpu='GPU image input'
mark(images,names,'.cl',gpu,'Inkling CUDA kernels or device ABI')
mark(d/'apex-routing','route softmax mailbox prediction','.cl',gpu,'Apex routing and softmax device code')
frozen=u/'bonsai-frozen-cuda'
mark(frozen,'device-prelude','.cl',gpu,'Device compilation prelude')
mark(s,'cuda-device','.cl',gpu,'Shared device source for GigaChat4, Apex and Bonsai')
mark(s / "gigachat4", "cuda-dsa-certified", ".cl", gpu, "Experimental GigaChat4 DSA image adapter")
mark(d/'apex-extra','epilogue','.cl',gpu,'Apex MoE device epilogue')
for f in sorted(files):
    if f.name.endswith('.cl.patch') and f.is_relative_to(d/'apex-extra'):
        marks[f]=('GPU source overlay','Patch for copied CUDA device source')
mark(s / "vulkan/shaders", "reference", ".comp", gpu, "Vulkan GLSL shader source")
direct=set(marks)
mark(frozen / "glue", "cuda-flash-vec cuda-mmvq cuda-rms-reference", ".cl",
     "Source include (Bonsai/Apex staging)", "Platform-specific CUDA device ABI variant")
todo = sorted(marks)
visited = set()
missing = {}
include_dirs=[s,u,d,d/'images',u/'inkling',u/'inkling/host',s/'gigachat4',frozen/'glue',g/'include',g/'src',g/'src/ggml-cpu',Path('integration/m1/host')]
while todo:
    f=todo.pop()
    if f in visited:
        continue
    visited.add(f)
    for name in re.findall(r'^\s*#\s*include\s+"([^"]+)"',(root/f).read_text(errors='replace'),re.M):
        if (f == g / "src/ggml.c" and name in {"ggml-quants.h", "ggml-backend.h", "ggml-cpu.h", "ggml-quantization-reference.inc", "ggml-graph-reference.inc", "ggml-operator-reference.inc", "ggml-geometry-reference.inc", "ggml-op-names-reference.inc", "ggml-hash-size-reference.inc", "ggml-runtime-reference.inc", "ggml-arena-reference.inc", "ggml-graph-storage-reference.inc"}) or (
            f == s / "ggml-system.c" and name in {"ggml-backend-impl.h", "ggml-system-reference.inc"}
        ) or (f == g / "src/ggml-impl.h" and name in {"gguf.h", "ggml-impl-reference.inc", "ggml-hash-reference.inc"}):
            continue
        candidates=[]
        if name not in {'main.h','cpu.h','goose-unit.h','qwen2_unit.h'} and (name != 'graph.h' or f.is_relative_to(s/'vulkan')):
            for folder in [f.parent]+include_dirs:
                candidate=(root/folder/name).resolve()
                if candidate.is_relative_to(root) and candidate.relative_to(root) in files:
                    candidates.append(candidate.relative_to(root))
        if not candidates:
            missing.setdefault(name, set()).add(str(f))
        elif candidates[0] not in marks:
            child=candidates[0]
            marks[child]=('Source include (conditional possible)','Included by '+str(f))
            todo.append(child)

groups = defaultdict(list)
for path in sorted(files):
    name = path.as_posix()
    lines = len((root / path).read_bytes().splitlines())
    if path in direct:
        status, purpose = marks[path]
        bucket = "runtime_host" if status.startswith("Host") else "runtime_gpu"
    elif path in marks:
        status, purpose = marks[path]
        bucket = "runtime_include"
    elif path.parts[0] in {"experiments", "results"} or "/_archive/" in name or name.startswith("runtime/checkpoints/"):
        bucket = "archive"
        status, purpose = "No resolved runtime edge", "Archived experiment or reference"
    elif "tests" in path.parts or "gates" in path.parts or path.parts[0] == "benchmarks":
        bucket = "tests"
        status, purpose = "Separate targets", "Test or benchmark"
    else:
        bucket = "outside_runtime_closure"
        status, purpose = "No resolved runtime edge", "Alternate component, vendor code, or tool"
    if "generated" in path.parts:
        bucket = "recorded_generated_output"
    groups[bucket].append(dict(path=name, lines=lines, evidence=status, purpose=purpose))

summary = {
    key: dict(files=len(rows), lines=sum(row["lines"] for row in rows))
    for key, rows in sorted(groups.items())
}
if args.json:
    print(json.dumps(dict(revision=git("rev-parse", "HEAD").strip(), summary=summary,
                          files=dict(sorted(groups.items())),
                          unresolved_includes={k: sorted(v) for k, v in sorted(missing.items())}),
                     indent=2))
else:
    for key, total in summary.items():
        print(f"{key:28} {total['files']:5} files {total['lines']:9} lines")
    print("Runtime rows count source files, including conditional and discarded code.")
    print(f"{len(missing)} unresolved include names (generated, external, or missing); use --json for details.")
