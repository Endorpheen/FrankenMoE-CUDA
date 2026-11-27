"""Check the Bonsai Vulkan raw-token CLI against two frozen CUDA responses."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    binary, model, *flags = sys.argv[1:]
    with Path(model).open("rb") as file:
        sha = hashlib.file_digest(file, "sha256").hexdigest()
    if sha != "ededc9fc0a8a6a8dd30914d107b2d2ce7674d789f1d05a1a481f240609aa6c23":
        raise ValueError("Bonsai GGUF does not match the reference")
    cases = json.loads(Path(__file__).with_name("bonsai-model.json").read_text())
    for case in cases:
        args = [binary, model, "--bonsai", *flags, "--n-predict", str(case["max_tokens"]),
                "--", *map(str, case["prompt"])]
        ids = list(map(int, subprocess.check_output(args, timeout=60).split()))
        if ids != case["tokens"]:
            raise AssertionError(f"{case['name']}: {ids} != {case['tokens']}")
        print(f"{case['name']}: {len(ids)} reference IDs PASS")


if __name__ == "__main__":
    main()
