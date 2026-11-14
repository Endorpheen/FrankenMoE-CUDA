#!/bin/bash
# Differential gate for the P7-11 S4/S5 slices.
#
#   tests/build.sh            gate at -O1
#   OPT=-O0 tests/build.sh    gate at -O0 (used by the mutation run)
#
# Two lines are printed and are the acceptance criterion:
#
#   FG_LOG cases=<n> failures=<n>
#   FG_CONSOLE cases=<n> failures=<n>
#
# The native side includes the real common/log.cpp, common/debug.cpp and
# common/console.cpp; the Goose side runs source/log-policy.goose and
# source/console-editing.goose. No terminal, socket or logging thread is used.
set -uo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
GATE_OPT=${GATE_OPT:--O1}
SKIP_NATIVE=${FG_SKIP_NATIVE:-0}
OUT=${OUT_DIR:-$S/build}
INC=(-I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -I"$L/vendor")

mkdir -p "$OUT"
python3 "$S/tests/gen_corpus.py" >/dev/null || exit 2

build() {
    "$CXX" "$OPT" -std=c++17 "${INC[@]}" "$@" || exit 2
}

if [ "$SKIP_NATIVE" = 1 ] && [ -f "$OUT/native_fglog.out" ] && [ -f "$OUT/native_console.out" ]; then
    echo "[gate] native oracle cached in $OUT"
else
    build "$S/build/probe_log.cpp" "$L/common/json.cpp" -o "$OUT/probe_log" || exit 2
    build "$S/build/probe_debug.cpp" -o "$OUT/probe_debug" || exit 2
    build "$S/build/probe_console.cpp" -o "$OUT/probe_console" || exit 2

    "$OUT/probe_log"     > "$OUT/native_log.out"     || { echo "probe_log failed"; exit 2; }
    "$OUT/probe_debug"   > "$OUT/native_debug.out"   || { echo "probe_debug failed"; exit 2; }
    ( cd "$OUT" && ./probe_console . > native_console.out ) || { echo "probe_console failed"; exit 2; }
    cat "$OUT/native_log.out" "$OUT/native_debug.out" > "$OUT/native_fglog.out"
fi

timeout 900 "$G" run "$GATE_OPT" "$S/build/gate_log.goose"     > "$OUT/goose_log.out" 2> "$OUT/goose_log.err"
rc_log=$?
timeout 900 "$G" run "$GATE_OPT" "$S/build/gate_console.goose" > "$OUT/goose_console.out" 2> "$OUT/goose_console.err"
rc_con=$?

python3 - "$OUT" "$rc_log" "$rc_con" <<'PY'
import sys
from pathlib import Path

out = Path(sys.argv[1])
rc_log = int(sys.argv[2])
rc_con = int(sys.argv[3])


def compare(native, goose, diff_path):
    a = (out / native).read_text(errors="replace").splitlines()
    b = (out / goose).read_text(errors="replace").splitlines()
    bad = sum(1 for x, y in zip(a, b) if x != y)
    bad += abs(len(a) - len(b))
    with open(out / diff_path, "w") as f:
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y:
                f.write("case %d\n  native: %s\n  goose : %s\n" % (i, x[:300], y[:300]))
        if len(a) != len(b):
            f.write("line count native=%d goose=%d\n" % (len(a), len(b)))
    return len(a), bad


cases_log, fail_log = compare("native_fglog.out", "goose_log.out", "diff_log.txt")
cases_con, fail_con = compare("native_console.out", "goose_console.out", "diff_console.txt")
if rc_log != 0:
    fail_log += 1
    print("goose log run exited %d" % rc_log)
if rc_con != 0:
    fail_con += 1
    print("goose console run exited %d" % rc_con)
print("FG_LOG cases=%d failures=%d" % (cases_log, fail_log))
print("FG_CONSOLE cases=%d failures=%d" % (cases_con, fail_con))
sys.exit(1 if (fail_log or fail_con) else 0)
PY
