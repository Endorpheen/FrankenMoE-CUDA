#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify a fresh experimental package directory}
candidate=${2:?Specify the checkpoint artifact directory}
manifest="$root/experiments/bonsai-checkpoints.json"

python3 - "$manifest" "$candidate" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

manifest, artifacts = map(Path, sys.argv[1:])
checkpoint = json.loads(manifest.read_text())["experimental"]
if checkpoint["status"] != "experimental":
    raise SystemExit("Expected an experimental checkpoint")
for name in ("frankenmoe-infer", "pipeline.cubin"):
    with (artifacts / name).open("rb") as file:
        actual = hashlib.file_digest(file, "sha256").hexdigest()
    if actual != checkpoint["artifact_sha256"][name]:
        raise SystemExit(f"Experimental checkpoint checksum mismatch: {name}")
PY

FRANKENMOE_BONSAI_HTTP_BUILD="$candidate" bash "$src/stage-linux.sh" "$out"

python3 - "$manifest" "$out" <<'PY'
import hashlib
import json
from pathlib import Path
import re
import sys

manifest, package = map(Path, sys.argv[1:])
receipt = json.loads(manifest.read_text())
for name, expected in receipt["shared_sha256"].items():
    with (package / name).open("rb") as file:
        actual = hashlib.file_digest(file, "sha256").hexdigest()
    if actual != expected:
        raise SystemExit(f"Experimental checkpoint shared checksum mismatch: {name}")

profile = package / "profiles/bonsai.env"
existing = dict(line.split("=", 1) for line in profile.read_text().splitlines())
for key, value in receipt["experimental"]["profile_overrides"].items():
    if not re.fullmatch(r"BONSAI_[A-Z0-9_]+", key) or not re.fullmatch(r"[A-Za-z0-9_-]+", value):
        raise SystemExit(f"Invalid Bonsai checkpoint setting: {key}")
    existing[key] = value
profile.write_text("".join(f"{key}={value}\n" for key, value in existing.items()))
(package / "BONSAI-CHECKPOINT.json").write_bytes(manifest.read_bytes())
readme = package / "README.md"
original = readme.read_text()
name = receipt["experimental"]["id"]
banner = (f"# Experimental Bonsai checkpoint {name}\n\n"
          "For local research. See BONSAI-CHECKPOINT.json for provenance, "
          "measured speeds and open quality checks.\n\n")
readme.write_text(banner + original)
PY

(
    cd "$out"
    find . -type f ! -path ./CONTENTS.sha256 -print0 | sort -z | xargs -0 sha256sum > CONTENTS.sha256
)
