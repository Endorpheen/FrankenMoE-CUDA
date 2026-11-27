#!/bin/bash
# Three-level verification for the GooseInfer migration.
#
#   bash tools/check.sh fast          changed code only: compose + typecheck the
#                                     unit, compile it, run the gates whose
#                                     modules changed
#   bash tools/check.sh integration   incremental build of the staged M1 tree
#                                     plus the ten accepted gates in both
#                                     configurations
#   bash tools/check.sh full          clean configure and build, determinism,
#                                     failure recovery, tree and ledger checks
#
# Levels are cumulative: `full` implies `integration` implies `fast`.
#
# Every level records an input stamp. If the inputs did not change since the
# last successful run of that level, the level is skipped and its previous
# result is reported instead of being recomputed. `--force` ignores the stamp,
# `--why` prints why a level ran or was skipped.
#
# Build directory policy: the staged tree lives under $STAGE_ROOT, which
# defaults to <repo>/builds/stage-m1 and is gitignored. Nothing is written to
# /tmp; /tmp on this host is a 31 GB tmpfs that several concurrent agents share.
#
# Usage: bash tools/check.sh <fast|integration|full> [--force] [--why]
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
LEVEL=${1:?usage: check.sh <fast|integration|full> [--force] [--why]}
FORCE=0
WHY=0
for a in "${@:2}"; do
    case "$a" in
        --force) FORCE=1 ;;
        --why)   WHY=1 ;;
        *) echo "check.sh: unknown option $a" >&2; exit 2 ;;
    esac
done

STAGE_ROOT=${STAGE_ROOT:-$ROOT/builds/stage-m1}
STAMP_DIR=$ROOT/builds/.check-stamps
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
JOBS=${JOBS:-16}
mkdir -p "$STAMP_DIR" "$ROOT/builds"

FAILS=0
T0=$(date +%s)

# Rule 14: the log of a step is written from its first line, and the same bytes
# are shown. `logrun <file> <command...>` does both; the file is the evidence and
# is reused instead of re-running while the step's inputs are unchanged.
say() { printf '%s\n' "$*"; }
step() { printf '\n== %s\n' "$*"; }
ok() { say "  ok: $*"; }
bad() { say "  FAIL: $*"; FAILS=$((FAILS + 1)); }

# A cheap fingerprint of one input set: the SHA-256 of every tracked file plus
# its path, in a stable order. Cheap enough to run on every invocation (the
# module set is ~1000 files, a few milliseconds) and it never misses an edit.
fingerprint() { # <paths...>
    ( cd "$ROOT" && git ls-files -s -- "$@" 2>/dev/null | sha256sum | cut -d' ' -f1
      git diff --stat -- "$@" 2>/dev/null | sha256sum | cut -d' ' -f1 ) \
        | sha256sum | cut -d' ' -f1
}

stamp_path() { printf '%s/%s.stamp' "$STAMP_DIR" "$1"; }

# Run a step, write its log from the first line and show the same output.
# Usage: logrun <logfile> <command...>   (exit code is the command's)
logrun() {
    local log=$1; shift
    local rc
    mkdir -p "$(dirname "$log")"
    {
        printf '$ %s\n' "$*"
        "$@" 2>&1
        rc=$?
        printf '[exit %d]\n' "$rc"
        exit "$rc"
    } | tee "$log"
    return "${PIPESTATUS[0]}"
}

# should_run <level> <fingerprint> -> 0 when the level must run
should_run() {
    local level=$1 fp=$2 path
    path=$(stamp_path "$level")
    if [ "$FORCE" -eq 1 ]; then
        [ "$WHY" -eq 1 ] && say "  why: --force"
        return 0
    fi
    if [ ! -f "$path" ]; then
        [ "$WHY" -eq 1 ] && say "  why: no previous $level result"
        return 0
    fi
    if [ "$(cat "$path")" = "$fp" ]; then
        [ "$WHY" -eq 1 ] && say "  why: inputs unchanged since the last $level run"
        return 1
    fi
    [ "$WHY" -eq 1 ] && say "  why: inputs changed since the last $level run"
    return 0
}

mark_done() { printf '%s' "$2" > "$(stamp_path "$1")"; }

# --- FAST -------------------------------------------------------------------
# The composed unit is the single artifact every stage shares, so a typecheck
# plus a compile of it is the cheapest check that catches a broken module, a
# broken import or a duplicate global. The gates for the touched stages run only
# when that stage's modules are among the changed paths.
level_fast() {
    step "FAST: compose, typecheck and compile the unit"
    local fp
    fp=$(fingerprint goose tools/compose-goose.py tools/compose-alpha-runtime.py tools/build-main-runtime.sh)
    if should_run fast "$fp"; then
        local out=$ROOT/build/goose-main
        if bash "$ROOT/tools/build-main-runtime.sh" "$out" >"$STAMP_DIR/fast.log" 2>&1; then
            tail -1 "$STAMP_DIR/fast.log" | sed 's/^/  /'
            ok "unit composes, typechecks and has no duplicate global symbol"
            mark_done fast "$fp"
        else
            bad "unit build failed; see $STAMP_DIR/fast.log"
            tail -n 45 "$STAMP_DIR/fast.log" | sed 's/^/    /'
        fi
    else
        ok "unit unchanged, previous result reused ($(cat "$(stamp_path fast)" | cut -c1-12))"
    fi
}

# --- INTEGRATION ------------------------------------------------------------
# One staged tree, reused in place. Ninja rebuilds only what changed; the ten
# gates then run against the binaries that were actually produced.
level_integration() {
    step "INTEGRATION: staged M1 tree at $STAGE_ROOT"
    local fp
    fp=$(fingerprint integration/m1 upstream/llama.cpp/CMakeLists.txt goose tools/stage-m1-tree.sh)
    if [ ! -d "$STAGE_ROOT/build-on" ]; then
        say "  first run: assembling the staged tree"
        rm -rf "$STAGE_ROOT"
        if ! bash "$ROOT/tools/stage-m1-tree.sh" "$STAGE_ROOT" --with-unit 2>&1 | tail -3 | sed 's/^/  /'; then
            bad "staging failed"
            return
        fi
    else
        # Re-apply only the integration layer: upstream and the patches are
        # cheap to copy, a full re-stage is not.
        cp -r "$ROOT/integration/m1/common/." "$STAGE_ROOT/common/"
        cp -r "$ROOT/integration/m1/host/." "$STAGE_ROOT/host/"
        cp "$ROOT/integration/m1/CMakeLists.txt" "$STAGE_ROOT/CMakeLists.txt"
        ok "integration layer refreshed in place"
    fi

    local cfg
    for cfg in off on; do
        # The project defaults every stage ON (rule 10), so the isolation build
        # has to name the ten flags and the product build names none.
        local flags="-DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_GGUF=OFF -DFG_GOOSE_QUANT=OFF \
                   -DFG_GOOSE_METRICS=OFF -DFG_GOOSE_TOKENIZER=OFF -DFG_GOOSE_STREAM=OFF \
                   -DFG_GOOSE_CONFIG=OFF -DFG_GOOSE_SPEC=OFF -DFG_GOOSE_MODELS=OFF \
                   -DFG_GOOSE_SESSION=OFF"
        [ "$cfg" = on ] && flags=""
        # A CMake cache is bound to the absolute source path it was created with,
        # so a tree moved between directories must be reconfigured, not reused.
        local cache="$STAGE_ROOT/build-$cfg/CMakeCache.txt"
        local need_cfg=0
        if [ ! -f "$cache" ]; then
            need_cfg=1
        elif ! grep -q "^CMAKE_HOME_DIRECTORY:INTERNAL=$STAGE_ROOT$" "$cache"; then
            ok "build-$cfg: stale cache from another path, reconfiguring"
            rm -rf "$STAGE_ROOT/build-$cfg"
            need_cfg=1
        fi
        if [ "$need_cfg" -eq 1 ]; then
            # shellcheck disable=SC2086
            cmake -S "$STAGE_ROOT" -B "$STAGE_ROOT/build-$cfg" -G Ninja \
                -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-13 \
                -DCMAKE_CXX_COMPILER=g++-13 $flags >"$STAMP_DIR/cfg-$cfg.log" 2>&1 \
                || { bad "configure $cfg failed; see $STAMP_DIR/cfg-$cfg.log"; continue; }
        fi
        if cmake --build "$STAGE_ROOT/build-$cfg" -j "$JOBS" >"$STAMP_DIR/build-$cfg.log" 2>&1; then
            local n
            n=$(grep -c "^ninja: no work to do" "$STAMP_DIR/build-$cfg.log" 2>/dev/null || true)
            if [ "${n:-0}" -gt 0 ]; then
                ok "build-$cfg: nothing to rebuild (incremental reuse)"
            else
                ok "build-$cfg: incremental build"
            fi
        else
            bad "build-$cfg failed; see $STAMP_DIR/build-$cfg.log"
        fi
    done

    step "INTEGRATION: the ten accepted gates"
    local gates="fg-goose-smoke fg-gguf-smoke fg-goose-quant-smoke fg-goose-metrics-smoke
                 fg-goose-tokenizer-smoke fg-goose-stream-gate fg-config-smoke
                 fg-spec-smoke fg-model-smoke fg-session-smoke"
    local g fail_g=0
    for g in $gates; do
        local off_out on_out
        off_out=$(timeout 180 "$STAGE_ROOT/build-off/$g" 2>&1 | tail -1)
        on_out=$(timeout 180 "$STAGE_ROOT/build-on/$g" 2>&1 | tail -1)
        case "$on_out" in
            *failures=0*|*"checks passed"*|*"emitted="*) ;;
            *) bad "$g: unexpected ON result: $on_out"; fail_g=1 ;;
        esac
        case "$off_out" in
            *failures=0*|*"checks passed"*|*"emitted="*) ;;
            *) bad "$g: unexpected OFF result: $off_out"; fail_g=1 ;;
        esac
    done
    [ "$fail_g" -eq 0 ] && ok "10 gates green in both configurations"
    timeout 60 "$STAGE_ROOT/build-on/fg-host-smoke" 2>&1 | tail -1 | sed 's/^/  /'
    timeout 120 "$STAGE_ROOT/build-on/fg-iq-seam" 2>&1 | grep FG_IQ_SEAM | sed 's/^/  /'

    if [ "$FAILS" -eq 0 ]; then
        mark_done integration "$fp"
    fi
}

# --- FULL -------------------------------------------------------------------
level_full() {
    step "FULL: clean staged build from tracked inputs"
    local fp
    fp=$(fingerprint integration/m1 upstream/llama.cpp goose tools)
    if ! should_run full "$fp" && [ "$FORCE" -eq 0 ]; then
        ok "inputs unchanged, previous FULL result reused"
        return
    fi

    local clean=$ROOT/builds/.full-clean
    rm -rf "$clean"
    if bash "$ROOT/tools/stage-m1-tree.sh" "$clean" --with-unit >"$STAMP_DIR/full-stage.log" 2>&1; then
        ok "tree staged from upstream + integration/m1 + goose"
    else
        bad "staging failed; see $STAMP_DIR/full-stage.log"
        return
    fi

    local cfg
    for cfg in off on; do
        # The project defaults every stage ON (rule 10), so the isolation build
        # has to name the ten flags and the product build names none.
        local flags="-DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_GGUF=OFF -DFG_GOOSE_QUANT=OFF \
                   -DFG_GOOSE_METRICS=OFF -DFG_GOOSE_TOKENIZER=OFF -DFG_GOOSE_STREAM=OFF \
                   -DFG_GOOSE_CONFIG=OFF -DFG_GOOSE_SPEC=OFF -DFG_GOOSE_MODELS=OFF \
                   -DFG_GOOSE_SESSION=OFF"
        [ "$cfg" = on ] && flags=""
        # shellcheck disable=SC2086
        if cmake -S "$clean" -B "$clean/build-$cfg" -G Ninja -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 $flags \
                >"$STAMP_DIR/full-cfg-$cfg.log" 2>&1 \
           && cmake --build "$clean/build-$cfg" -j "$JOBS" >"$STAMP_DIR/full-build-$cfg.log" 2>&1; then
            ok "clean build-$cfg: $(stat -c%s "$clean/build-$cfg/bin/llama-server") B"
        else
            bad "clean build-$cfg failed; see $STAMP_DIR/full-build-$cfg.log"
        fi
    done

    if [ "$FAILS" -eq 0 ]; then
        local off_on off_n on_n
        off_on=$(nm "$clean/build-off/bin/llama-server" | grep -c ' T goose_init')
        on_n=$(nm "$clean/build-on/bin/llama-server" | grep -c ' T goose_init')
        if [ "$off_on" -eq 0 ] && [ "$on_n" -eq 1 ]; then
            ok "goose_init: 0 in OFF, 1 in ON"
        else
            bad "goose_init counts are $off_on / $on_n, expected 0 / 1"
        fi
        step "FULL: determinism"
        if bash "$ROOT/tools/build-main-runtime.sh" --repro 2>&1 | tail -1 | grep -q "repro: ok"; then
            ok "the unit reproduces byte-identically"
        else
            bad "determinism gate failed"
        fi
        step "FULL: failure recovery"
        if bash "$ROOT/tools/check-failure-recovery.sh" "$clean" 2>&1 | tail -8 | sed 's/^/  /'; then
            ok "failure recovery matrix"
        else
            bad "failure recovery matrix"
        fi
        step "FULL: tree, ledger and generated-file rules"
        python3 "$ROOT/tools/check-goose-tree.py" >"$STAMP_DIR/full-tree.log" 2>&1 \
            && ok "goose tree hashes" || bad "goose tree check; see $STAMP_DIR/full-tree.log"
        python3 "$ROOT/tools/check-generated-files.py" >"$STAMP_DIR/full-gen.log" 2>&1 \
            && ok "generated-file rule" || bad "generated-file rule; see $STAMP_DIR/full-gen.log"
        [ -f "$ROOT/docs/goose-migration/tasks.json" ] \
            && python3 -c "import json,sys;json.load(open('$ROOT/docs/goose-migration/tasks.json'))" 2>/dev/null \
            && ok "ledger parses" || bad "ledger does not parse"
    fi

    if [ "$FAILS" -eq 0 ]; then
        mark_done full "$fp"
    fi
}

case "$LEVEL" in
    fast)        level_fast ;;
    integration) level_fast; level_integration ;;
    full)        level_fast; level_integration; level_full ;;
    *) echo "check.sh: unknown level $LEVEL (fast|integration|full)" >&2; exit 2 ;;
esac

T=$(( $(date +%s) - T0 ))
say ""
if [ "$FAILS" -ne 0 ]; then
    say "check $LEVEL: $FAILS check(s) failed, ${T}s"
    exit 1
fi
say "check $LEVEL: ok, ${T}s"
