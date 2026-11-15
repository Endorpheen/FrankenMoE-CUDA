#!/usr/bin/env bash
# Live progress for a writer that runs in the background.
#
# Every build is started as
#
#     bash tools/build-arch-unit.sh <arch> > builds/.<name>.log 2>&1 &
#
# and this reads that log. It reports the stage, the elapsed time, the last lines and
# the verdict - and it never starts, restarts or duplicates the writer.
#
#   tools/progress.sh builds/.apex-build6.log [pidfile] [lines]
#
# The process is identified by its OWN pid when one is recorded; a `pgrep -f` on the
# command line matches the pgrep itself and reports a finished build as running, which
# is exactly the kind of lie this tool must not tell.
set -u

LOG=${1:?usage: progress.sh <log> [pidfile] [lines]}
PIDFILE=${2:-}
TAIL=${3:-6}

if [ ! -f "$LOG" ]; then
    echo "no log at $LOG"
    exit 2
fi

state=unknown
if [ -n "$PIDFILE" ] && [ -f "$PIDFILE" ]; then
    pid=$(cat "$PIDFILE" 2>/dev/null || true)
    if [ -n "${pid:-}" ] && kill -0 "$pid" 2>/dev/null; then
        state="running, elapsed $(ps -o etime= -p "$pid" 2>/dev/null | tr -d ' ')"
    else
        state="finished"
    fi
else
    # No pid recorded: fall back to the log's own verdict line.
    if grep -qE "unit ok|runtime ok|check ok|error|failed" "$LOG" 2>/dev/null; then
        state="finished (verdict in log)"
    else
        state="in progress or unknown"
    fi
fi

stage=$(grep -E "^\[[0-9]+/[0-9]+\]" "$LOG" | tail -1)
echo "log:    $LOG"
echo "state:  $state"
echo "stage:  ${stage:-none yet}"
echo "lines:  $(wc -l < "$LOG")"
echo "--- last $TAIL ---"
tail -n "$TAIL" "$LOG"

verdict=$(grep -E "unit ok|runtime ok|check ok|: ok \(|PASS|FAIL|error:|failed" "$LOG" | tail -1)
if [ -n "$verdict" ]; then
    echo "--- verdict ---"
    echo "$verdict"
fi
