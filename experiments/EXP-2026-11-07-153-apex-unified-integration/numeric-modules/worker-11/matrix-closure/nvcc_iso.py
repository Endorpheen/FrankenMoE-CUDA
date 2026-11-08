#!/usr/bin/env python3
"""Run the CUDA 12.6 driver's own compile stages without /bin/sh.

The isolated local-check image has no /bin, while the nvcc driver spawns its
tools through system(3).  This wrapper asks nvcc for --dryrun and executes the
printed commands directly.  It is still the real nvcc/cudafe++/cicc/ptxas
pipeline; no source transformation and no GPU execution.
"""
import os, shlex, subprocess, sys
from pathlib import Path

CUDA = "/usr/local/cuda-12.6"
NVCC = CUDA + "/bin/nvcc"
ENV = dict(os.environ)
ENV["PATH"] = CUDA + "/bin:" + CUDA + "/nvvm/bin:/usr/bin:/bin"
ENV["CICC_PATH"] = CUDA + "/nvvm/bin"

def main(argv):
    dry = subprocess.run([NVCC, "--dryrun", *argv], capture_output=True, text=True, env=ENV)
    if dry.returncode != 0:
        sys.stderr.write(dry.stdout + dry.stderr)
        return dry.returncode
    work = Path(__file__).resolve().parent / ".nvcc_tmp"
    work.mkdir(parents=True, exist_ok=True)
    for line in (dry.stdout + dry.stderr).splitlines():
        if not line.startswith("#$ "):
            continue
        cmd = line[3:].strip().replace("$CICC_PATH", ENV["CICC_PATH"]).replace("${CICC_PATH}", ENV["CICC_PATH"])
        parts = shlex.split(cmd)
        if not parts or "=" in parts[0] or parts[0] == "rm":
            continue
        print("+", " ".join(parts))
        p = subprocess.run(parts, cwd=str(work), env=ENV, capture_output=True, text=True)
        if p.returncode != 0:
            sys.stderr.write(p.stdout + p.stderr)
            return p.returncode
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
