#!/usr/bin/env python3
"""Run nvcc's dry-run phase plan without /bin/sh (sandbox has no /bin)."""
import os, re, shlex, subprocess, sys

def main(argv):
    nvcc = argv[0]
    args = argv[1:]
    dry = subprocess.run([nvcc, "--dryrun", *args], capture_output=True, text=True)
    if dry.returncode != 0:
        sys.stderr.write(dry.stdout + dry.stderr)
        return dry.returncode
    variables = {}
    env = dict(os.environ)
    extra = os.path.dirname(nvcc) + ":/usr/lib/nvidia-cuda-toolkit/bin:/usr/local/cuda/bin:/usr/local/cuda/nvvm/bin"
    for raw in dry.stderr.splitlines():
        line = raw[3:] if raw.startswith("#$ ") else raw[2:] if raw.startswith("# ") else None
        if line is None:
            continue
        line = line.strip()
        if line.startswith("--error"):
            continue
        m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)=(.*)$", line)
        if m and not line.startswith('"'):
            variables[m.group(1)] = m.group(2).strip()
            if m.group(1) in ("PATH", "LD_LIBRARY_PATH"):
                env[m.group(1)] = m.group(2).strip()
            continue
        if not line or line.startswith("--"):
            continue
        try:
            parts = shlex.split(line, posix=True)
        except ValueError:
            continue
        out = []
        for p in parts:
            def repl(mo):
                n = mo.group(1) or mo.group(2)
                return variables.get(n, os.environ.get(n, ""))
            out.append(re.sub(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}|\$([A-Za-z_][A-Za-z0-9_]*)", repl, p))
        if not out:
            continue
        if out[0] == "rm":
            for t in out[1:]:
                try: os.remove(t)
                except FileNotFoundError: pass
            continue
        e = dict(env); e["PATH"] = e.get("PATH", "/usr/bin:/bin") + ":" + extra
        try:
            rc = subprocess.run(out, env=e).returncode
        except FileNotFoundError:
            sys.stderr.write("nvcc_exec: missing %s\n" % out[0]); return 127
        if rc != 0:
            sys.stderr.write("nvcc_exec: phase failed (%d): %s\n" % (rc, " ".join(out)))
            return rc
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
