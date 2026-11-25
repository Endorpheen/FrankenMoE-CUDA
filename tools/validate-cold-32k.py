#!/usr/bin/env python3
"""Check an uncached 32K/512 request without launching inference."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re


PROMPT = 32768
DECODE = 512
RESET = "[bench-cold] reset position=0"
PREFILL = "[bench-cold] prefill position=32768"
PRISM_SLOT = re.compile(r"\bslot\s+get_availabl:\s+id\s+(\d+)\s+\|.*?\bn_tokens\s*=\s*(\d+)")
PRISM_EVAL = re.compile(r"\bslot\s+print_timing:\s+id\s+(\d+)\s+\|.*?prompt eval time\s*=\s*[0-9.]+ ms /\s*(\d+) tokens")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def verify_request(path, expected_sha):
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == expected_sha, "request SHA-256 differs from reference")
    request = json.loads(raw)
    prompt = request["prompt"]
    require(isinstance(prompt, list) and len(prompt) == PROMPT and
            all(type(token) is int and token >= 0 for token in prompt), "expected 32768 raw token IDs")
    require(request["cache_prompt"] is False, "cache_prompt must be false")
    require(request.get("n_cache_reuse", 0) == 0, "n_cache_reuse must be zero")
    require(request["n_predict"] == DECODE and request.get("stream", False) is False,
            "expected one non-streaming 512-token completion")


def verify_response(response):
    timings = response["timings"]
    for key, expected in (("cache_n", 0), ("prompt_n", PROMPT), ("predicted_n", DECODE)):
        actual = timings[key]
        require(type(actual) is int and actual == expected,
                f"timings.{key}: expected {expected}, observed {actual}")
    require(response["tokens_evaluated"] == PROMPT and response["tokens_predicted"] == DECODE,
            "response counts disagree with the workload")
    require(isinstance(response["tokens"], list) and len(response["tokens"]) == DECODE,
            "expected 512 returned token IDs")


def verify_log(engine, log):
    if engine == "prism":
        selected = PRISM_SLOT.findall(log)
        evaluated = PRISM_EVAL.findall(log)
        require(len(selected) == len(evaluated) == 1, "expected exactly one PrismML slot and prefill record")
        slot, initial = selected[0]
        eval_slot, computed = evaluated[0]
        require(log.find("selected slot") < log.find("prompt eval time"),
                "PrismML slot selection must precede prefill")
        require(slot == eval_slot and int(initial) == 0 and int(computed) == PROMPT,
                f"PrismML slot {slot}: initial={initial}, evaluated={computed}, eval_slot={eval_slot}")
    else:
        evidence = [line for line in log.splitlines() if line.startswith("[bench-cold]")]
        require(evidence == [RESET, PREFILL], f"expected one successful reset and full prefill, observed {evidence}")


def identity(pid, binary):
    process = Path("/proc") / str(pid)
    require(pid > 0 and (process / "exe").resolve(strict=True) == binary.resolve(strict=True),
            "server PID does not match the benchmark executable")
    stat = (process / "stat").read_text().rsplit(")", 1)[1].split()
    boot_id = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    return [boot_id, stat[19]]


def before(args):
    verify_request(args.request, args.request_sha256)
    instance = identity(args.pid, args.binary)
    with args.log.open("rb") as log:
        stat = os.fstat(log.fileno())
        startup = log.read().decode("utf-8", errors="replace")
        offset = log.tell()
    require(not PRISM_SLOT.search(startup) and "[bench-cold]" not in startup,
            "server log already contains request activity")
    receipt = {
        "pid": args.pid, "process_identity": instance, "binary": str(args.binary.resolve()),
        "log": str(args.log.resolve()), "log_offset": offset,
        "log_identity": [stat.st_dev, stat.st_ino],
        "request": str(args.request.resolve()), "request_sha256": args.request_sha256,
        "engine": args.engine,
    }
    with args.receipt.open("x") as file:
        json.dump(receipt, file, indent=2)
        file.write("\n")


def after(args):
    receipt = json.loads(args.receipt.read_text())
    require(identity(receipt["pid"], Path(receipt["binary"])) == receipt["process_identity"],
            "server changed after preflight")
    verify_request(Path(receipt["request"]), receipt["request_sha256"])
    with Path(receipt["log"]).open("rb") as log:
        stat = os.fstat(log.fileno())
        require([stat.st_dev, stat.st_ino] == receipt["log_identity"] and stat.st_size >= receipt["log_offset"],
                "server log was replaced or truncated")
        log.seek(receipt["log_offset"])
        evidence = log.read().decode("utf-8", errors="replace")
    raw = args.response.read_bytes()
    verify_response(json.loads(raw))
    verify_log(receipt["engine"], evidence)
    print(json.dumps({
        "cold_prefill": "PASS", "computed_prompt_tokens": PROMPT, "cached_prompt_tokens": 0,
        "engine": receipt["engine"], "process_identity": receipt["process_identity"],
        "request_sha256": receipt["request_sha256"], "response_sha256": hashlib.sha256(raw).hexdigest(),
    }))


def instrument_bonsai(args):
    require(args.source.resolve() != args.output.resolve(), "use a separate generated output file")
    host = args.source.read_text()
    reset = """    return finish_session_call(session, status);
}

int bonsai_session_step(struct bonsai_session *session,"""
    step = """    return finish_session_call(session, status);
}

int bonsai_session_is_eog("""
    require(host.count(reset) == host.count(step) == 1, "Bonsai session anchors changed")
    host = host.replace(reset, f"""    status = finish_session_call(session, status);
    if (!status && m->position == 0) fputs("{RESET}\\n", stderr);
    return status;
}}

int bonsai_session_step(struct bonsai_session *session,""")
    host = host.replace(step, f"""    status = finish_session_call(session, status);
    if (!status && logits && m->position == {PROMPT}) fputs("{PREFILL}\\n", stderr);
    return status;
}}

int bonsai_session_is_eog(""")
    args.output.write_text(host)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)

    preflight = sub.add_parser("before", help="snapshot a dedicated server and validate the frozen request")
    preflight.add_argument("--engine", choices=("goose", "prism"), required=True)
    preflight.add_argument("--pid", type=int, required=True)
    preflight.add_argument("--binary", type=Path, required=True)
    preflight.add_argument("--log", type=Path, required=True)
    preflight.add_argument("--request", type=Path, required=True)
    preflight.add_argument("--request-sha256", required=True)
    preflight.add_argument("--receipt", type=Path, required=True)
    preflight.set_defaults(run=before)

    check = sub.add_parser("after", help="admit or exclude the completed arm")
    check.add_argument("--receipt", type=Path, required=True)
    check.add_argument("--response", type=Path, required=True)
    check.set_defaults(run=after)

    instrument = sub.add_parser("instrument-bonsai", help="add benchmark-only counter logs to generated host source")
    instrument.add_argument("source", type=Path)
    instrument.add_argument("output", type=Path)
    instrument.set_defaults(run=instrument_bonsai)

    args = parser.parse_args()
    try:
        args.run(args)
    except (OSError, ValueError, TypeError, KeyError, IndexError) as error:
        parser.exit(1, f"Cold prefill check failed: {error}\n")


if __name__ == "__main__":
    main()
