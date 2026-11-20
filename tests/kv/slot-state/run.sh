#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
if [ -z "${APEX_SLOT_STATE_DIR:-}" ]; then
    echo 'skip: slot state directory and save or restore filename are required'
    exit 77
fi
exec bash "$ROOT/tools/apex-test.sh" "${APEX_SLOT_TAG:-slot-state}" 24 4096 4096 4992
