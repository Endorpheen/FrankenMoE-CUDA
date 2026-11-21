#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
exec python3 "$ROOT/tools/check-model-regressions.py" \
    --candidate "${FG_MODEL_CANDIDATE:?set the independent candidate executable}" \
    --output "${FG_MODEL_REPORT:?set a new directory for the acceptance receipt}"
