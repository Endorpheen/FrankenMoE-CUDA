#!/usr/bin/env python3
"""Hash-bound replay for the copied EXP107 context sources.

This tool is inert: it never builds, runs, or imports llama/Goose, never
touches weights or a model, and never uses the network or subprocesses. It
binds the local trace fixture to the exact bytes of the copied
inputs/active-llama-context.cpp/.h and validates/emits the expected call
sequence for a parent harness to compare against a recorded run.

Usage (parent runs this):
  python3 replay.py --pin       # write context-trace.lock.json (once)
  python3 replay.py --check     # verify hashes + fixture invariants
  python3 replay.py --replay [--recorded FILE]

Status: UNVERIFIED PROPOSAL for worker-4. Hashes are computed at runtime and
are never hardcoded here.
"""
import argparse
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
TRACE = HERE / "context-trace-fixture.json"
LOCK = HERE / "context-trace.lock.json"
SCHEMA = "fg107.context-trace/1"
LOCK_SCHEMA = "fg107.context-trace.lock/1"

# Actual implementation artifacts that the trace must be bound to. This is not
# a friend/include check: it covers the Goose module, its call frame, the
# services/bridge and the executable driver.
EXTRA_BOUND = [
    "worker-4/context-execution.goose",
    "worker-4/context-execution-frame.hpp",
    "worker-4/context-services.h",
    "worker-4/context-services.cpp",
    "worker-4/context-trace-driver.cpp",
    "worker-4/context-resource.goose",
    "worker-4/context-resource-driver.cpp",
    "worker-4/context-policy.goose",
    "worker-4/context-policy-driver.cpp",
    "worker-4/bridge.h",
    "worker-4/unit-exec.c",
    "worker-4/unit-resource.c",
    "worker-4/unit-policy.c",
    "worker-4/fg107-route.h",
    "worker-4/fg107-compat.cpp",
    "worker-4/make_patched.py",
    "worker-4/patched/llama-context.cpp",
    "worker-4/patch-manifest.json",
    "worker-4/local-check.json",
]

REQUIRED_DRIVER_SYMBOLS = [
    "fg107_goose_decode",
    "fg107_goose_output_reorder",
    "fg107_goose_sched_copy_experts",
    "fg107_goose_resource_init",
    "fg107_goose_sched_reserve",
    "fg107_goose_state_get_data",
    "fg107_goose_cparams_policy",
    "fg107_goose_resolve_fused",
    "fg107_goose_graph_cb_policy",
    "fg107_goose_realloc_policy",
]
REQUIRED_SERVICE_WIRING = [
    "fg107_sched_set_copy_callback",
    "fg107_goose_sched_copy_experts",
    "FG107_NATIVE_ROLLBACK",
]
REQUIRED_UNIT_WIRING = [
    "gg_function_1",
    "gg_function_38",
    "gg_function_26",
]


def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_json(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        fail(f"missing file: {path}")
    except json.JSONDecodeError as e:
        fail(f"invalid JSON in {path}: {e}")


def fail(msg, code=2):
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(code)


def bound_files(trace):
    names = list(trace.get("source_bindings", []))
    names += EXTRA_BOUND
    if not names:
        fail("trace has no source_bindings")
    out = {}
    for name in names:
        p = (ROOT / name).resolve()
        if not p.is_file():
            fail(f"bound source missing: {p}")
        out[name] = p
    return out


def check_patch_binding():
    man_path = HERE / "patch-manifest.json"
    if not man_path.is_file():
        fail("patch-manifest.json missing; run make_patched.py", code=4)
    man = json.loads(man_path.read_text(encoding="utf-8"))
    base = ROOT / man["base_cpp"]
    base_h = ROOT / man["base_h"]
    patched = ROOT / man["patched"]
    if sha256(base) != man["base_cpp_sha256"]:
        fail("patched source was generated from a different context snapshot", code=3)
    if sha256(base_h) != man["base_h_sha256"]:
        fail("context header changed since the patch was generated", code=3)
    if sha256(patched) != man["patched_sha256"]:
        fail("patched context source changed without regenerating the manifest", code=3)
    text = patched.read_text(encoding="utf-8")
    required = [
        "fg107_goose_decode",
        "fg107_goose_sched_reserve",
        "fg107_goose_sched_new",
        "fg107_goose_resource_init",
        "fg107_goose_resource_release",
        "fg107_goose_cparams_policy",
        "fg107_goose_resolve_fused",
        "fg107_goose_graph_cb_policy",
        "fg107_goose_realloc_policy",
        "FG107_NATIVE_ROLLBACK",
    ]
    for sym in required:
        if sym not in text:
            fail(f"patched context is missing default-on routing marker {sym}")


def check_actual_wiring():
    check_patch_binding()
    drv = (ROOT / "worker-4/context-trace-driver.cpp").read_text(encoding="utf-8")
    rdrv = (ROOT / "worker-4/context-resource-driver.cpp").read_text(encoding="utf-8")
    pdrv = (ROOT / "worker-4/context-policy-driver.cpp").read_text(encoding="utf-8")
    svc = (ROOT / "worker-4/context-services.cpp").read_text(encoding="utf-8")
    unit = (ROOT / "worker-4/unit-exec.c").read_text(encoding="utf-8")
    runit = (ROOT / "worker-4/unit-resource.c").read_text(encoding="utf-8")
    punit = (ROOT / "worker-4/unit-policy.c").read_text(encoding="utf-8")
    for sym in REQUIRED_DRIVER_SYMBOLS:
        if sym not in drv and sym not in rdrv and sym not in pdrv:
            fail(f"drivers do not reference actual export {sym}")
    for sym in REQUIRED_SERVICE_WIRING:
        if sym not in svc:
            fail(f"services do not implement wiring {sym}")
    for sym in REQUIRED_UNIT_WIRING:
        if sym not in unit and sym not in runit and sym not in punit:
            fail(f"unit wrappers do not call {sym}")
    if "__attribute__((weak))" in svc:
        fail("services must not use a weak callback fallback")
    res = HERE / "local-check-results" / "result.json"
    if not res.is_file():
        fail("local-check-results/result.json missing; run the local check first", code=4)
    data = json.loads(res.read_text(encoding="utf-8"))
    if data.get("status") == "FAILED":
        fail("local checks failed")
    for st in data.get("steps", []):
        if st.get("exit_code") != 0:
            fail(f"local step {st.get('step')} failed: {st.get('argv')}")


def validate(trace):
    if trace.get("schema") != SCHEMA:
        fail(f"unexpected trace schema: {trace.get('schema')!r}")
    scenarios = trace.get("scenarios")
    if not isinstance(scenarios, list) or not scenarios:
        fail("trace has no scenarios")
    seen = set()
    for sc in scenarios:
        sid = sc.get("id")
        if not sid or sid in seen:
            fail(f"missing or duplicate scenario id: {sid!r}")
        seen.add(sid)
        ctx = sc.get("ctx", {})
        exp = sc.get("expect", {})
        batch = sc.get("batch")
        if batch is not None and "decode_return" not in exp:
            fail(f"{sid}: expect.decode_return missing")
        if batch is not None:
            n = len(batch.get("tokens", []))
            for key in ("output", "pos", "seq_ids"):
                if len(batch.get(key, [])) != n:
                    fail(f"{sid}: batch.{key} length != tokens length")
            n_out = sum(1 for v in batch["output"] if v)
            if exp.get("decode_return") == 0 and exp.get("n_outputs") != n_out:
                fail(f"{sid}: expect.n_outputs != number of output tokens")
            ub = int(ctx.get("n_ubatch", 0))
            if ub <= 0:
                fail(f"{sid}: n_ubatch must be > 0")
            ubatches = math.ceil(n / ub)
            calls = {c["fn"]: c for c in exp.get("calls", [])}
            abort = "compute_status_by_ubatch" in sc.get("inject", {})
            want = 1 if abort else ubatches
            if calls.get("fg107_process_ubatch", {}).get("count") != want:
                fail(f"{sid}: process_ubatch count != {want}")
            if exp["decode_return"] == 0:
                if calls.get("fg107_sched_graph_compute_async", {}).get("count") != ubatches:
                    fail(f"{sid}: compute count != ubatches")
            for pair in exp.get("output_ids", []):
                if len(pair) != 2:
                    fail(f"{sid}: output_ids entry must be [batch_idx,row]")
                b, row = pair
                if not (0 <= b < n) or not batch["output"][b]:
                    fail(f"{sid}: output_ids batch index {b} is not an output token")
                if not (0 <= row < n_out):
                    fail(f"{sid}: output_ids row {row} out of range")
        if "swaps" in sc.get("state", {}):
            order = sc["state"].get("embd_batch_idxs", [])
            if sorted(order) != list(range(len(order))):
                fail(f"{sid}: embd_batch_idxs is not a permutation")
    return scenarios


def cmd_pin():
    trace = load_json(TRACE)
    validate(trace)
    files = bound_files(trace)
    lock = {
        "schema": LOCK_SCHEMA,
        "pinned_by": "worker-4",
        "trace": TRACE.name,
        "trace_sha256": sha256(TRACE),
        "files": {name: sha256(p) for name, p in files.items()},
    }
    LOCK.write_text(json.dumps(lock, indent=2) + "\n", encoding="utf-8")
    print(f"pinned {len(files)} source file(s) -> {LOCK.name}")
    for name, digest in lock["files"].items():
        print(f"  {digest}  {name}")
    return 0


def cmd_check():
    trace = load_json(TRACE)
    validate(trace)
    check_actual_wiring()
    if not LOCK.is_file():
        fail(f"{LOCK.name} missing; run --pin once", code=4)
    lock = load_json(LOCK)
    if lock.get("schema") != LOCK_SCHEMA:
        fail("unexpected lock schema")
    if lock.get("trace_sha256") != sha256(TRACE):
        fail("trace fixture changed since pinning", code=3)
    for name, p in bound_files(trace).items():
        want = lock.get("files", {}).get(name)
        got = sha256(p)
        if want != got:
            fail(f"hash mismatch for {name}: pinned {want}, now {got}", code=3)
    print(f"OK: {len(lock['files'])} bound source(s), {len(trace['scenarios'])} scenario(s)")
    return 0


def cmd_replay(recorded):
    trace = load_json(TRACE)
    scenarios = validate(trace)
    for sc in scenarios:
        for call in sc["expect"].get("calls", []):
            row = {"scenario": sc["id"], "expect": call,
                   "decode_return": sc["expect"].get("decode_return")}
            print(json.dumps(row, sort_keys=True))
    if not recorded:
        return 0
    rec_path = Path(recorded)
    observed = []
    try:
        for line in rec_path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line:
                observed.append(json.loads(line))
    except (FileNotFoundError, json.JSONDecodeError) as e:
        fail(f"cannot read recorded trace: {e}")
    have = {}
    for r in observed:
        have.setdefault((r.get("scenario"), r.get("fn")), 0)
        have[(r.get("scenario"), r.get("fn"))] += 1
    problems = []
    for sc in scenarios:
        for call in sc["expect"].get("calls", []):
            key = (sc["id"], call["fn"])
            want = call.get("count", call.get("min", 0))
            got = have.get(key, 0)
            if call.get("min") is not None:
                ok = got >= call["min"]
            else:
                ok = got == want
            if not ok:
                problems.append(f"{sc['id']}: {call['fn']} expected {want}, observed {got}")
    if problems:
        for p in problems:
            print(f"MISMATCH: {p}", file=sys.stderr)
        return 2
    print(f"OK: recorded trace matches {len(scenarios)} scenario(s)")
    return 0


def cmd_driver(binary, name):
    trace = load_json(TRACE)
    validate(trace)
    check_actual_wiring()
    try:
        r = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError) as e:
        fail(f"cannot run driver: {e}")
    sys.stdout.write(r.stdout)
    sys.stderr.write(r.stderr)
    if r.returncode != 0:
        fail(f"driver exit code {r.returncode}")
    if '"status":"PASS"' not in r.stdout or name not in r.stdout:
        fail(f"driver did not report PASS for {name}")
    print(f"OK: {name} exercised actual Goose exports on {len(trace['scenarios'])} scenario(s)")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--pin", action="store_true")
    g.add_argument("--check", action="store_true")
    g.add_argument("--replay", action="store_true")
    g.add_argument("--driver", help="path to the compiled context-trace-driver")
    g.add_argument("--resource-driver", help="path to the compiled context-resource-driver")
    g.add_argument("--policy-driver", help="path to the compiled context-policy-driver")
    ap.add_argument("--recorded", help="JSONL of observed calls for comparison")
    a = ap.parse_args()
    if a.pin:
        return cmd_pin()
    if a.check:
        return cmd_check()
    if a.driver:
        return cmd_driver(a.driver, "fg107-native-goose-trace")
    if a.resource_driver:
        return cmd_driver(a.resource_driver, "fg107-resource-trace")
    if a.policy_driver:
        return cmd_driver(a.policy_driver, "fg107-policy-trace")
    return cmd_replay(a.recorded)


if __name__ == "__main__":
    sys.exit(main())
