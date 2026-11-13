#!/usr/bin/env bash
# Self-contained lifecycle suite for scripts/run_qwen38_server.sh.
# Usage: run_lifecycle_suite.sh <path-to-launcher-script>
# Creates its own scratch dir (mktemp -d), fake llama-server/nvidia-smi, and a Python driver
# that resets inherited ignored SIGINT/SIGQUIT before exec, waits via Popen.wait() (never via
# kill(pid,0) on own child), rejects stale pidfiles, and waits for post-trap launcher readiness.
# Cases: server exit 0, server exit 7 (code preserved), SIGINT and SIGTERM to the launcher only
# (launcher must exit on its own; the fake server must be left not running; no pkill is used).
# Exit 0 iff all checks pass; prints PASS/FAIL and a final pass=/fail= counter.
set -u
LAUNCHER="$1"
SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

mkdir -p "$SCRATCH/fakebin" "$SCRATCH/fakebuild/bin" "$SCRATCH/work"
cat > "$SCRATCH/fakebin/llama-server" <<'FAKEEOF'
#!/usr/bin/env bash
echo $$ > "${SERVER_PIDFILE:?}"
echo "fake llama-server started, args: $*"
echo "fake server listening"
if [[ ${SERVER_MODE:-exit0} == exit0 ]]; then sleep "${SERVER_SLEEP:-2}"; exit 0; fi
if [[ ${SERVER_MODE:-exit0} == exit7 ]]; then sleep "${SERVER_SLEEP:-2}"; exit 7; fi
if [[ ${SERVER_MODE:-exit0} == long ]]; then while true; do sleep 0.2; done; fi
exit 9
FAKEEOF
cat > "$SCRATCH/fakebin/nvidia-smi" <<'SMIEOF'
#!/usr/bin/env bash
echo "123, 45, 67, 89"
SMIEOF
chmod +x "$SCRATCH/fakebin/llama-server" "$SCRATCH/fakebin/nvidia-smi"
cp "$SCRATCH/fakebin/llama-server" "$SCRATCH/fakebuild/bin/llama-server"
for part in 00001 00002 00003; do echo model > "$SCRATCH/work/model-$part-of-00003.gguf"; done

cat > "$SCRATCH/driver.py" <<'PYEOF'
import os, signal, subprocess, sys, time

launcher, mode, sig_name = sys.argv[1], sys.argv[2], sys.argv[3]
scratch = os.environ["SCRATCH"]
sig = getattr(signal, "SIG" + sig_name, None) if sig_name != "none" else None

env = {
    "PATH": f"{scratch}/fakebin:/usr/bin:/bin",
    "HOME": scratch,
    "SCRATCH": scratch,
    "BUILD_DIR": f"{scratch}/fakebuild",
    "MODEL_PATH": f"{scratch}/work/model-00001-of-00003.gguf",
    "SERVER_MODE": mode,
    "SERVER_PIDFILE": f"{scratch}/server.pid",
    "SERVER_SLEEP": "2",
    "SWAP_WATCHDOG": os.environ.get("SWAP_WATCHDOG", "0"),
    "LOG_PATH": f"{scratch}/server.log",
    "MONITOR_PATH": f"{scratch}/monitor.csv",
}

def reset_signals():
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    signal.signal(signal.SIGQUIT, signal.SIG_DFL)

def state(pid):
    try:
        with open(f"/proc/{pid}/stat") as f:
            return f.read().split()[2]
    except OSError:
        return None

def live_group_members(pgid):
    members = []
    for name in os.listdir("/proc"):
        if not name.isdigit():
            continue
        pid = int(name)
        try:
            if os.getpgid(pid) == pgid and state(pid) not in (None, "Z"):
                members.append(pid)
        except OSError:
            pass
    return members

if os.path.exists(env["SERVER_PIDFILE"]):
    os.remove(env["SERVER_PIDFILE"])   # never trust a stale pidfile across cases
p = subprocess.Popen(["/usr/bin/bash", launcher], env=env,
                     stdin=subprocess.DEVNULL,
                     stdout=open(f"{scratch}/launcher.out", "w"),
                     stderr=subprocess.STDOUT,
                     start_new_session=True, preexec_fn=reset_signals)
deadline = time.time() + 10
while time.time() < deadline and not os.path.exists(env["SERVER_PIDFILE"]):
    time.sleep(0.1)
if not os.path.exists(env["SERVER_PIDFILE"]):
    print("FAIL: fake server never started"); p.kill(); sys.exit(2)
server = int(open(env["SERVER_PIDFILE"]).read().strip())

if sig is not None:
    launcher_log = f"{scratch}/launcher.out"
    deadline = time.time() + 10
    while time.time() < deadline:
        try:
            if "llama-server pid" in open(launcher_log).read():
                break
        except OSError:
            pass
        time.sleep(0.1)
    else:
        print("FAIL: launcher never reached post-trap readiness")
        p.kill()
        sys.exit(2)
    p.send_signal(sig)

try:
    rc = p.wait(timeout=25)
    print(f"launcher exited rc={rc}")
except subprocess.TimeoutExpired:
    print("TIMEOUT: launcher has not exited")
    p.kill(); rc = None

time.sleep(0.6)
sst = state(server)
print(f"server state after: {sst if sst else 'gone'}")
members = live_group_members(p.pid)
print(f"live process-group members after: {members}")

if mode == "exit0":
    ok = (rc == 0) and sst not in ("R", "S", "D", "Z") and not members
elif mode == "exit7":
    ok = (rc == 7) and sst not in ("R", "S", "D", "Z") and not members
elif sig_name == "INT":
    ok = (rc == 130) and sst not in ("R", "S", "D", "Z") and not members
elif sig_name == "TERM":
    ok = (rc == 143) and sst not in ("R", "S", "D", "Z") and not members
else:
    ok = False
print("RESULT", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
PYEOF

export SCRATCH
pass=0 fail=0
run_case() { # label mode sig expected
    local label="$1" mode="$2" sig="$3"
    if python3 "$SCRATCH/driver.py" "$LAUNCHER" "$mode" "$sig"; then
        echo "PASS  $label"; pass=$((pass+1))
    else
        echo "FAIL  $label"; fail=$((fail+1))
    fi
}

run_case "server exit 0 -> launcher exit 0, no leftovers" exit0 none
run_case "server exit 7 -> launcher exit 7" exit7 none
run_case "SIGINT to launcher -> exits 130, server not running" long INT
run_case "SIGTERM to launcher -> exits 143, server not running" long TERM

if [[ ${EXPECT_WATCHDOG_OPT_IN:-0} == 1 ]]; then
    if SWAP_WATCHDOG=1 python3 "$SCRATCH/driver.py" "$LAUNCHER" exit0 none >/dev/null &&
            grep -q -- '--swap-watchdog' "$SCRATCH/server.log"; then
        echo "PASS  SWAP_WATCHDOG=1 adds --swap-watchdog"
        pass=$((pass+1))
    else
        echo "FAIL  SWAP_WATCHDOG=1 adds --swap-watchdog"
        fail=$((fail+1))
    fi
    if SWAP_WATCHDOG=0 python3 "$SCRATCH/driver.py" "$LAUNCHER" exit0 none >/dev/null &&
            ! grep -q -- '--swap-watchdog' "$SCRATCH/server.log"; then
        echo "PASS  default profile omits --swap-watchdog"
        pass=$((pass+1))
    else
        echo "FAIL  default profile omits --swap-watchdog"
        fail=$((fail+1))
    fi
fi

echo "pass=$pass fail=$fail"
[[ $fail -eq 0 ]]
