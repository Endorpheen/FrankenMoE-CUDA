#!/usr/bin/env bash
# Unified entry for the permanent tests under tests/.
#
#   bash tests/run.sh list                       every registered test, its area and its tier
#   bash tests/run.sh fast                       every area that needs no model and no GPU,
#                                                then tools/check.sh fast
#   bash tests/run.sh <area>                     one area, e.g. grammar
#   bash tests/run.sh <area> <test>              one test, e.g. grammar reject-alternates
#   bash tests/run.sh integration                tools/check.sh integration, then every
#                                                integration-tier test
#   bash tests/run.sh full                       tools/check.sh full, then everything fast
#   bash tests/run.sh performance                performance tests only; never implied by full
#
# What this script does NOT do, on purpose:
#   - it does not reimplement the build, the stamps or the acceptance ladder: `fast`,
#     `integration` and `full` delegate to tools/check.sh, which stays the authority on what
#     those levels mean for the migration build;
#   - it does not run anything that needs the model or the Apex lock unless the caller asks for
#     that area explicitly, and those tests take the lock themselves through tools/apex-test.sh.
#
# Tier is declared by the test itself: a directory holds a file named `tier` with one of
# fast|integration|performance|model. A test without a `tier` file is treated as `model` and is
# never run implicitly - the safe default.
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
T=$ROOT/tests
FAILS=0
RAN=0
SKIPPED=0

tier_of() {
    local d=$1
    if [ -f "$d/tier" ]; then tr -d '[:space:]' < "$d/tier"; else echo model; fi
}

test_dirs() { # <area>
    local area=$1
    find "$T/$area" -mindepth 2 -maxdepth 2 -name run.sh -printf '%h\n' 2>/dev/null | sort
}

list_tests() {
    local area d
    printf '%-12s %-24s %-12s %s\n' AREA TEST TIER PATH
    for area in $(find "$T" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort); do
        for d in $(test_dirs "$area"); do
            printf '%-12s %-24s %-12s %s\n' "$area" "$(basename "$d")" "$(tier_of "$d")" "${d#$ROOT/}"
        done
    done
}

run_one() { # <dir>
    local d=$1 name rc
    name=${d#$ROOT/}
    printf '\n== %s (tier %s)\n' "$name" "$(tier_of "$d")"
    bash "$d/run.sh"
    rc=$?
    if [ "$rc" -eq 77 ]; then
        # 77 is the skip convention: a precondition (a built library, a GPU) is absent.
        # A skip is not evidence, so it is counted and printed, never folded into "ok".
        printf '  skip: %s (precondition absent)\n' "$name"
        SKIPPED=$((SKIPPED + 1))
        return 0
    fi
    RAN=$((RAN + 1))
    if [ "$rc" -eq 0 ]; then
        printf '  ok: %s\n' "$name"
    else
        printf '  FAIL: %s (exit %d)\n' "$name" "$rc"
        FAILS=$((FAILS + 1))
    fi
    return 0
}

# Run every test of one area whose tier is in <allowed...>
run_area() { # <area> <allowed-tiers...>
    local area=$1; shift
    local d t allowed
    for d in $(test_dirs "$area"); do
        t=$(tier_of "$d")
        allowed=0
        for want in "$@"; do [ "$t" = "$want" ] && allowed=1; done
        if [ "$allowed" -eq 0 ]; then
            printf 'skip: %s (tier %s)\n' "${d#$ROOT/}" "$t"
            continue
        fi
        run_one "$d"
    done
}

delegate() { # <level>
    printf '\n== tools/check.sh %s\n' "$1"
    if bash "$ROOT/tools/check.sh" "$1"; then
        printf '  ok: tools/check.sh %s\n' "$1"
    else
        printf '  FAIL: tools/check.sh %s\n' "$1"
        FAILS=$((FAILS + 1))
    fi
}

finish() {
    printf '\n== summary: %d test(s) run, %d failure(s), %d skipped\n' "$RAN" "$FAILS" "$SKIPPED"
    [ "$FAILS" -eq 0 ]
}

cmd=${1:-}
case "$cmd" in
    list)
        list_tests
        exit 0
        ;;
    fast)
        for a in cpu ggml kv tokenizer sampler grammar server models json memory regression; do
            run_area "$a" fast
        done
        delegate fast
        finish
        ;;
    integration)
        delegate integration
        run_area integration fast integration
        finish
        ;;
    full)
        delegate full
        for a in cpu ggml kv tokenizer sampler grammar server models json memory regression integration; do
            run_area "$a" fast integration
        done
        finish
        ;;
    performance)
        run_area performance performance
        finish
        ;;
    '')
        sed -n '2,26p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
        exit 2
        ;;
    *)
        area=$cmd
        if [ ! -d "$T/$area" ]; then
            printf 'no such area: %s\n' "$area" >&2
            exit 2
        fi
        if [ $# -ge 2 ]; then
            d=$T/$area/$2
            if [ ! -f "$d/run.sh" ]; then
                printf 'no such test: %s/%s\n' "$area" "$2" >&2
                exit 2
            fi
            run_one "$d"
        else
            run_area "$area" fast integration performance model
        fi
        finish
        ;;
esac
