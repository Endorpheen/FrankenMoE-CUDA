#!/usr/bin/env bash
# Self-contained synthetic boundary suite for the monitor guard in scripts/run_qwen38_server.sh.
# Usage: run_synthetic_boundary_suite.sh <path-to-launcher-script> [--fixed]
#   --fixed applies "s/sys_rate > 2097152/sys_rate > 2048/" to a scratch copy before extraction.
# Runs in its own scratch dir with a fake nvidia-smi. The monitor prologue+loop is extracted
# verbatim into run_guard(); only three substitutions:
#   - the vmstat path -> "$FAKE_VMSTAT" (vector-fed)
#   - sleep 1         -> sleep "$GUARD_INTERVAL" (speed only)
#   - one extra echo increments ITER_FILE after the real sleep (harness synchronization)
# Dependencies before run_guard(): MONITOR_PATH (temp csv), RUN_PID (sleep child),
# stop_children (trip flag stub), nvidia-smi (fakebin). Each step waits for the guard iteration
# counter before writing the next vmstat value, then waits until the value is echoed to the CSV.
# No real swap, no real server. Exit 0 iff all expectations hold.
set -u
SCRIPT="$1"
VARIANT="${2:-orig}"
ROOT=$(mktemp -d)
trap 'rm -rf "$ROOT"' EXIT

mkdir -p "$ROOT/fakebin"
cat > "$ROOT/fakebin/nvidia-smi" <<'EOF'
#!/usr/bin/env bash
echo "123, 45"
EOF
chmod +x "$ROOT/fakebin/nvidia-smi"

if [[ $VARIANT == --fixed ]]; then
    sed 's/sys_rate > 2097152/sys_rate > 2048/' "$SCRIPT" > "$ROOT/script.fixed.sh"
    SCRIPT="$ROOT/script.fixed.sh"
    hard2049=trip      # fixed hard rule 2048 trips immediately at 2049
else
    hard2049=no-trip   # bug: hard rule 2097152 unreachable in MiB/s units, single burst too short
fi

export FAKE_VMSTAT=$ROOT/fake-vmstat
export GUARD_INTERVAL=0.3
export MONITOR_PATH=$ROOT/monitor.csv
export TRIP_FLAG=$ROOT/trip-flag
export ITER_FILE=$ROOT/iter-count
export PATH=$ROOT/fakebin:$PATH

extract_guard() {
    awk '
        /^    last_in=/ { keep=1; print "run_guard() {"; }
        keep {
            gsub(/\/proc\/vmstat/, "\"$FAKE_VMSTAT\"")
            gsub(/sleep 1$/, "sleep \"$GUARD_INTERVAL\"; echo $(( $(cat \"$ITER_FILE\" 2>/dev/null || echo 0) + 1 )) > \"$ITER_FILE\"")
            print
        }
        keep && /^    done$/ { print "}"; exit }
    ' "$SCRIPT" > "$ROOT/guard.sh"
}

stop_children() { echo TRIP > "$TRIP_FLAG"; }

fail=0; pass=0
run_case() { # name expect rate...
    local name="$1" expect="$2"; shift 2
    rm -f "$TRIP_FLAG" "$ITER_FILE"
    : > "$MONITOR_PATH"
    local base=1000000
    local prev=$base
    printf 'pswpin %d\npswpout 1000\n' "$prev" > "$FAKE_VMSTAT"   # prologue read
    sleep 300 & local RUN_PID=$!
    export RUN_PID
    run_guard & local gpid=$!
    local r v i
    for r in "$@"; do
        for i in $(seq 1 10); do [[ $(cat "$ITER_FILE" 2>/dev/null || echo 0) -ge $i ]] && break; sleep 0.05; done
        v=$(( prev + r * 256 ))
        printf 'pswpin %d\npswpout 1000\n' "$v" > "$FAKE_VMSTAT"
        prev=$v
        for i in $(seq 1 200); do grep -q ",$v," "$MONITOR_PATH" && break; sleep 0.05; done
    done
    sleep 1
    local got=no-trip
    [[ -f "$TRIP_FLAG" ]] && got=trip
    kill "$gpid" 2>/dev/null; kill "$RUN_PID" 2>/dev/null
    wait "$gpid" 2>/dev/null; wait "$RUN_PID" 2>/dev/null
    if [[ $got == "$expect" ]]; then pass=$((pass+1)); local res=OK; else fail=$((fail+1)); local res=MISMATCH; fi
    printf '  %-44s expect=%-7s got=%-7s %s\n' "$name" "$expect" "$got" "$res"
}

echo "== variant: $VARIANT ($SCRIPT)"
grep -m1 "sys_rate >" "$SCRIPT" | sed 's/^ *//'
extract_guard
# shellcheck disable=SC1090
source "$ROOT/guard.sh"
run_case "hard-rate 2049 single sample"        "$hard2049" 2049
run_case "hard-rate 2048 single sample"        no-trip     2048
run_case "hard-rate 2047 single sample"        no-trip     2047
run_case "burst 5 consecutive >512"            trip        600 600 600 600 600
run_case "burst 4 consecutive >512"            no-trip     600 600 600 600
run_case "burst 4x600 reset normal 4x600"      no-trip     600 600 600 600 100 600 600 600 600
run_case "rate exactly 512, 8 samples"         no-trip     512 512 512 512 512 512 512 512

echo "== proc VmSwap burst constants (expression parity, strict >)"
internal_limit=$(( 256 * 1024 ))     # server-swapwatchdog.cpp proc_swap_burst_kib default
external_limit=262144                # run_qwen38_server.sh literal
for delta_mib in 255 256 257; do
    delta_kib=$(( delta_mib * 1024 ))
    ri=no-trip; (( delta_kib > internal_limit )) && ri=trip
    re=no-trip; (( delta_kib > external_limit )) && re=trip
    printf '  %s MiB -> internal(>262144KiB)=%-7s external(>262144KiB)=%-7s %s\n' \
        "$delta_mib" "$ri" "$re" "$( [[ $ri == "$re" ]] && echo MATCH || echo MISMATCH )"
done

echo "pass=$pass fail=$fail"
[[ $fail -eq 0 ]]
