from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="goose-graph-check-") as directory:
    build = Path(directory)
    cc = shlex.split(os.environ.get("CC", "cc"))
    cxx = shlex.split(os.environ.get("CXX", "c++"))
    subprocess.run(cc + ["-O3", "-march=native", "-fno-strict-aliasing", "-fPIC", "-c",
        str(root / "source/graph-plan/unit.c"), "-o", str(build / "graph.o")], check=True)
    subprocess.run(cxx + ["-O3", "-march=native", "-fno-strict-aliasing", "-pthread",
        str(root / "source/graph-plan/fixture.cpp"), str(build / "graph.o"),
        "-o", str(build / "graph-check"), "-lm"], check=True)
    subprocess.run([str(build / "graph-check")], cwd=build, check=True)
