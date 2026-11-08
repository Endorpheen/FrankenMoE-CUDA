#!/usr/bin/env python3
# Execute an nvcc compile by expanding its -dryrun command list.
# Needed because the isolated sandbox has no /bin/sh, which nvcc uses for
# its internal command runner. Each printed command is executed directly.
import os, re, shlex, subprocess, sys
CUDA = "/usr/local/cuda-12.6"
def main():
    if len(sys.argv) < 3:
        raise SystemExit("usage: nvcc_dry.py <out.o> <source.cu> [arch]")
    out, src = sys.argv[1], sys.argv[2]
    arch = sys.argv[3] if len(sys.argv) > 3 else "sm_70"
    env = dict(os.environ)
    env["PATH"] = CUDA + "/bin:" + CUDA + "/nvvm/bin:" + env.get("PATH", "/usr/bin:/bin")
    env["CICC_PATH"] = CUDA + "/nvvm/bin"
    env["LD_LIBRARY_PATH"] = CUDA + "/lib:" + CUDA + "/nvvm/lib64:" + env.get("LD_LIBRARY_PATH", "")
    cmd = [CUDA + "/bin/nvcc", "-dryrun", "-ccbin", "/usr/bin/g++-13",
           "-allow-unsupported-compiler", "-std=c++17", "-c", src, "-o", out,
           "-arch=" + arch, "-Xcompiler", "-U_GNU_SOURCE", "-I/work"]
    p = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if p.returncode != 0:
        sys.stderr.write(p.stdout + p.stderr)
        raise SystemExit(p.returncode)
    commands = []
    for line in p.stderr.splitlines():
        if not line.startswith("#$ "):
            continue
        text = line[3:].strip()
        first = text.split()[0] if text.split() else ""
        if not text or "=" in first:
            continue
        text = text.replace("$CICC_PATH", env["CICC_PATH"])
        commands.append(text)
    if not commands:
        raise SystemExit("nvcc dryrun produced no commands")
    for text in commands:
        argv = shlex.split(text)
        if not argv:
            continue
        print("+", " ".join(argv), flush=True)
        q = subprocess.run(argv, cwd="/tmp", env=env)
        if q.returncode != 0 and argv[0] != "rm":
            raise SystemExit("command failed rc=%d: %s" % (q.returncode, argv[0]))
if __name__ == "__main__":
    main()
