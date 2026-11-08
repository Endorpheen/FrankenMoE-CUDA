#!/usr/bin/env python3
"""Run the CUDA 12.6 driver's own compile stages without /bin/sh.

The isolated local-check bubblewrap image has no /bin (only /usr), while the
nvcc driver spawns its sub-tools through system(3), which needs /bin/sh.  This
wrapper asks nvcc for --dryrun, then executes exactly the commands nvcc printed
with subprocess (absolute paths, PATH includes the CUDA bin dir).  No source
transformation happens here; it is still the real nvcc/cudafe++/cicc/ptxas
pipeline.  GPU execution is never attempted.
"""
import os, shlex, subprocess, sys, tempfile
from pathlib import Path

CUDA = "/usr/local/cuda-12.6"
NVCC = CUDA + "/bin/nvcc"
ENV = dict(os.environ)
ENV["PATH"] = CUDA + "/bin:" + CUDA + "/nvvm/bin:/usr/bin:/bin"
ENV["CICC_PATH"] = CUDA + "/nvvm/bin"

def expand(cmd: str) -> str:
    return cmd.replace("$CICC_PATH", ENV["CICC_PATH"]).replace("${CICC_PATH}", ENV["CICC_PATH"])

def main(argv):
    dry = subprocess.run([NVCC, "--dryrun", *argv], capture_output=True, text=True, env=ENV)
    if dry.returncode != 0:
        sys.stderr.write(dry.stdout + dry.stderr)
        return dry.returncode
    work = Path("/work/.nvcc_iso_tmp")
    work.mkdir(parents=True, exist_ok=True)
    for line in (dry.stdout + dry.stderr).splitlines():
        if not line.startswith("#$ "):
            continue
        cmd = expand(line[3:].strip())
        if not cmd:
            continue
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
