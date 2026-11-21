#!/usr/bin/env python3
"""Run every registered model reference against one independent Goose binary."""

import argparse
import hashlib
import importlib.util
import json
import os
import re
import statistics
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "docs/goose-migration/model-architectures.json"
SPEC = importlib.util.spec_from_file_location("apex_validate", ROOT / "tools/validate-apex-32k.py")
APEX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(APEX)
require = APEX.require
HEX = re.compile(r"[0-9a-f]{64}")
NAME = re.compile(r"[a-z0-9][a-z0-9-]*")
PROFILE_KEYS = {"n_ctx", "n_batch", "n_ubatch", "n_threads", "n_threads_batch",
                "cache_type_k", "cache_type_v", "expert_cache_mib"}
RATE_KEYS = ("prefill_tokens_per_second_from_ms", "decode_steps_per_second_from_ms")
MEMORY_KEYS = ("rss_peak_kib", "process_vram_peak_mib")
ORDER = ("A", "B", "B", "A")


def sha_file(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def validate_registry(data):
    require(data["schema"] == 1, "unsupported model registry schema")
    architectures = {}
    cases = set()
    pending = []
    for arch in data["architectures"]:
        name = arch["id"]
        require(NAME.fullmatch(name) and name not in architectures, f"invalid or duplicate architecture {name}")
        architectures[name] = arch
        status = arch["status"]
        require(status in {"qualified", "partial", "planned", "legacy-qualified"}, f"invalid status for {name}")
        gguf_arch = arch["gguf_architecture"]
        require((status == "planned" and gguf_arch is None) or
                (isinstance(gguf_arch, str) and re.fullmatch(r"[a-z0-9_-]+", gguf_arch)),
                f"invalid GGUF architecture for {name}")
        for field in ("compute", "quantization", "kv_cache", "cuda_kernels", "goose_changes",
                      "validation", "results", "limitations"):
            require(isinstance(arch[field], str) and arch[field].strip(), f"{name} lacks {field}")
        if status == "legacy-qualified" and not arch["references"]:
            pending.append(name)
        if status == "qualified":
            require(arch["references"], f"{name} is qualified without a regression reference")
        for case in arch["references"]:
            ident = case["id"]
            require(NAME.fullmatch(ident) and ident not in cases, f"invalid or duplicate reference {ident}")
            cases.add(ident)
            require(isinstance(case["gpu"], str) and case["gpu"].startswith("NVIDIA "), f"missing GPU for {ident}")
            for key in ("model", "request", "baseline"):
                asset = case[key]
                require(HEX.fullmatch(asset["sha256"]), f"{ident} has unpinned {key}")
                require(Path(asset["path"]).is_absolute(), f"{ident} has a relative {key} path")
                require(re.fullmatch(r"FG_[A-Z0-9_]+", asset["environment"]), f"{ident} has invalid {key} override")
            profile = case["profile"]
            require(set(profile) == PROFILE_KEYS, f"incomplete profile for {ident}")
            for key in PROFILE_KEYS - {"cache_type_k", "cache_type_v"}:
                require(type(profile[key]) is int and profile[key] > 0, f"invalid {key} for {ident}")
            for key in ("cache_type_k", "cache_type_v"):
                require(re.fullmatch(r"[a-z0-9_]+", profile[key]), f"invalid {key} for {ident}")
            workload = case["workload"]
            require(set(workload) == {"prompt_tokens", "output_tokens", "vocabulary", "alternatives"}
                    and all(type(n) is int and n > 0 for n in workload.values())
                    and workload["output_tokens"] >= 2 and workload["prompt_tokens"] <= profile["n_ctx"],
                    f"invalid workload for {ident}")
            require(all(re.fullmatch(r"(?:GOOSE_|GGML_CUDA_)[A-Z0-9_]+", k) and isinstance(v, str)
                        for k, v in case["goose_options"].items()), f"invalid runtime environment for {ident}")
            require(HEX.fullmatch(case["baseline_output"]), f"{ident} has unpinned baseline output")
            for approval in case["reviewed_outputs"]:
                evidence = (ROOT / approval["evidence"]).resolve()
                require(HEX.fullmatch(approval["fingerprint"]) and evidence.is_relative_to(ROOT) and evidence.is_file(),
                        f"invalid reviewed output for {ident}")
    apex = architectures.get("qwen35moe", {})
    require(apex.get("status") == "qualified" and any(case["id"] == "apex-32k512-rtx4070"
            for case in apex.get("references", [])), "the mandatory Apex reference is missing")
    return architectures, pending


def asset_path(asset):
    path = Path(os.environ.get(asset["environment"], asset["path"])).resolve(strict=True)
    require(sha_file(path) == asset["sha256"], f"asset SHA-256 mismatch: {path}")
    return path


def swap_out(path):
    line = next(line for line in path.read_text().splitlines() if line.startswith("pswpout "))
    return int(line.split()[1])


def check_capture(directory, case, request_sha):
    require(sha_file(directory / "request.json") == request_sha, "captured request changed")
    cmdline = (directory / "cmdline.txt").read_bytes().rstrip()
    require(cmdline == (directory / "argv.txt").read_bytes().rstrip(), "live command differs from requested command")
    maps = (directory / "maps-after.txt").read_text()
    require(not re.search(r"/(?:libggml|libllama|libstdc\+\+)[^/\s]*", maps), "unexpected native runtime dependency")

    profile = case["profile"]
    for stage in ("ready", "after"):
        props = json.loads((directory / f"props-{stage}.json").read_text())
        for key, value in profile.items():
            if key == "expert_cache_mib":
                require(props["cuda_expert_cache_limit_bytes"] == value * 1024 * 1024,
                        f"wrong cache budget {stage}")
            else:
                require(props[key] == value, f"wrong {key} {stage}")
        require(props["model_pages"] > 0 and props["model_resident_pages"] == props["model_pages"],
                f"incomplete model residency {stage}")
        require(props["n_vocab"] == case["workload"]["vocabulary"], f"wrong vocabulary size {stage}")

    _, result = APEX.response_evidence(directory / "r1.json", **case["workload"])
    APEX.verify_step_timings(result["timings"], case["workload"]["prompt_tokens"], case["workload"]["output_tokens"])
    raw = json.loads((directory / "r1.json").read_text())
    require(raw["stop_type"] == "limit", "fixed-length response ended unexpectedly")
    require(not any((ord(ch) < 32 and ch not in "\r\n\t") or ch == "\ufffd" for ch in raw["content"]),
            "damaged text in response")
    identity = [result[key] for key in ("content_sha256", "token_ids_sha256", "probability_rows_sha256")]
    result["output_fingerprint"] = APEX.digest(APEX.compact(identity + [raw["stop_type"]]))
    samples = [json.loads(line) for line in (directory / "memory.jsonl").read_text().splitlines()]
    vram = [s["process_vram_mib"] for s in samples if s["process_vram_mib"] is not None]
    require(len(vram) >= 2 and max(vram) > 0, "missing process VRAM samples")
    status = (directory / "status-after.txt").read_text()
    hwm = int(re.search(r"^VmHWM:\s+(\d+)", status, re.M)[1])
    result.update(rss_peak_kib=max(hwm, *(s["rss_hwm_kib"] for s in samples)),
                  process_vram_peak_mib=max(vram),
                  card_vram_peak_mib=max(d["used_mib"] for s in samples for d in s["devices"]),
                  process_swap_peak_kib=max(int(re.search(r"^VmSwap:\s+(\d+)", status, re.M)[1]),
                                            *(s["swap_kib"] for s in samples)),
                  system_swap_out_pages=swap_out(directory / "vmstat-after.txt") - swap_out(directory / "vmstat-before.txt"))
    return result


def compare(case, runs):
    a = [r for arm, r in zip(ORDER, runs, strict=True) if arm == "A"]
    b = [r for arm, r in zip(ORDER, runs, strict=True) if arm == "B"]
    failures = []
    measurements = {}
    if any(r["process_swap_peak_kib"] != 0 or r["system_swap_out_pages"] != 0 for r in runs):
        failures.append("swap activity makes the pair unsuitable for acceptance")
    if any(r["output_fingerprint"] != case["baseline_output"] for r in a):
        failures.append("pinned baseline output changed")
    reviewed = {entry["fingerprint"] for entry in case["reviewed_outputs"]}
    if any(r["output_fingerprint"] not in reviewed | {case["baseline_output"]} for r in b):
        failures.append("candidate output differs and needs recorded numerical and quality review")
    for key in RATE_KEYS:
        base, candidate = (statistics.median(run[key] for run in arm) for arm in (a, b))
        measurements[key] = {"baseline": base, "candidate": candidate, "minimum": 0.98 * base}
        if candidate < 0.98 * base:
            failures.append(f"{key} exceeds the existing 2% regression limit")
    for key in MEMORY_KEYS:
        base = [r[key] for r in a]
        candidate = max(r[key] for r in b)
        limit = max(base) + max(base) - min(base)
        measurements[key] = {"baseline": max(base), "candidate": candidate, "maximum": limit}
        if candidate > limit:
            failures.append(f"{key} exceeds the baseline peak plus observed baseline spread")
    return {"status": "fail" if failures else "pass", "failures": failures, "measurements": measurements}


def capture(case, candidate, candidate_sha, destination):
    baseline, model, request = (asset_path(case[k]) for k in ("baseline", "model", "request"))
    workload = case["workload"]
    workload_request = json.loads(request.read_bytes())
    prompt = workload_request["prompt"]
    require(len(prompt) == workload["prompt_tokens"] and all(APEX.token_id(i, workload["vocabulary"]) for i in prompt),
            "invalid registered prompt")
    require(workload_request["n_predict"] == workload["output_tokens"] and workload_request["cache_prompt"] is False,
            "invalid registered request settings")
    gpulist = subprocess.check_output(["nvidia-smi", "--query-gpu=name", "--format=csv,noheader"], text=True).splitlines()
    require(gpulist == [case["gpu"]], f"expected one {case['gpu']}, found {gpulist}")
    profile = case["profile"]
    env = {k: v for k, v in os.environ.items() if not k.startswith(("APEX_", "GOOSE_", "GGML_", "LD_", "LLAMA_"))}
    env.update(case["goose_options"], MODEL=str(model), APEX_REQUEST=str(request), APEX_REQUEST_COUNT="1",
               APEX_SAMPLE_MEMORY="1", APEX_CTX=str(profile["n_ctx"]), APEX_GOOSE_THREADS=str(profile["n_threads"]),
               APEX_GOOSE_THREADS_BATCH=str(profile["n_threads_batch"]), APEX_CACHE_TYPE_K=profile["cache_type_k"],
               APEX_CACHE_TYPE_V=profile["cache_type_v"])
    runs = []
    for ordinal, arm in enumerate(ORDER, start=1):
        binary = baseline if arm == "A" else candidate
        tag = f"{case['id']}-{ordinal}{arm.lower()}"
        root = destination / tag
        code = subprocess.run(["bash", str(ROOT / "tools/apex-test.sh"), tag, str(workload["output_tokens"]),
                               str(profile["n_batch"]), str(profile["n_ubatch"]), str(profile["expert_cache_mib"])],
                              env=dict(env, APEX_RUN_ROOT=str(root), APEX_GOOSE_BIN=str(binary))).returncode
        require(code == 0, f"{tag} capture returned {code}")
        directories = sorted(root.iterdir())
        require(len(directories) == 1, f"expected one capture for {tag}")
        require(Path((directories[0] / "exe.txt").read_text().strip()) == binary,
                f"{tag} ran an unexpected executable")
        sums = (directories[0] / "SHA256SUMS").read_text().splitlines()
        expected_sha = case["baseline"]["sha256"] if arm == "A" else candidate_sha
        require(sums[1].split(maxsplit=1)[0] == expected_sha, f"{tag} used a changed executable")
        result = check_capture(directories[0], case, case["request"]["sha256"])
        result["capture"] = str(directories[0].relative_to(destination))
        runs.append(result)
    return {"baseline_sha256": case["baseline"]["sha256"], "order": ORDER, "runs": runs, **compare(case, runs)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validate", action="store_true", help="check registry schema without model runs")
    parser.add_argument("--candidate", type=Path, help="independent frankenmoe-infer binary")
    parser.add_argument("--output", type=Path, help="new directory for captures and receipt")
    args = parser.parse_args()
    if not args.validate and (args.candidate is None or args.output is None):
        parser.error("pass --candidate and --output to run the acceptance gate")
    try:
        architectures, pending = validate_registry(json.loads(REGISTRY.read_bytes()))
        print(f"Registry valid: {len(architectures)} architectures")
        if args.validate:
            return 0
        candidate = args.candidate.resolve(strict=True)
        require(candidate.is_file() and os.access(candidate, os.X_OK), "candidate must be executable")
        destination = args.output.resolve()
        destination.mkdir(parents=True)
        receipt = {"registry_sha256": sha_file(REGISTRY), "candidate_sha256": sha_file(candidate),
                   "candidate": str(candidate), "missing_legacy_references": pending, "references": {}}
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(2, f"Invalid architecture gate input: {error}\n")

    for arch in architectures.values():
        for case in arch["references"]:
            try:
                result = capture(case, candidate, receipt["candidate_sha256"], destination)
            except (OSError, ValueError, TypeError, KeyError, IndexError, StopIteration, subprocess.SubprocessError) as error:
                result = {"status": "fail", "failures": [str(error)]}
            receipt["references"][case["id"]] = result
            print(f"{case['id']}: {result['status']}")
            for failure in result["failures"]:
                print(f"  {failure}")
    passed = not pending and all(r["status"] == "pass" for r in receipt["references"].values())
    receipt["status"] = "pass" if passed else "fail"
    (destination / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if pending:
        print("Missing reproducible legacy references: " + ", ".join(pending))
    print(f"Architecture regression gate: {receipt['status']}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
