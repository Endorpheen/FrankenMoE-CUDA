#!/usr/bin/env python3
"""Check saved Apex 32K/512 responses against the frozen EXP-1476 workload."""

import argparse
import hashlib
import json
import math
from pathlib import Path


PROMPT_TOKENS = 32768
OUTPUT_TOKENS = 512
VOCABULARY = 248320
REQUEST_SHA = "484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f"
CONTENT_SHA = "c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f"
TOKENS_SHA = "fdf27fb67f12a5c0dcfa04ad59daf47100749df70cc096ed6cda8f9285e3d54c"
PROBABILITY_SHA = "20ac4af910d2e4c92dcbcaacdef1085b74241b2ee7447bb7825f9c18a99b5dd8"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def compact(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=True).encode("utf-8")


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def token_id(value):
    return type(value) is int and 0 <= value < VOCABULARY


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def verify_request(path):
    raw = path.read_bytes()
    require(digest(raw) == REQUEST_SHA, "request bytes differ from EXP-1476")
    request = json.loads(raw)
    prompt = request["prompt"]
    require(len(prompt) == PROMPT_TOKENS and all(map(token_id, prompt)), "invalid prompt IDs")
    require(request["n_predict"] == OUTPUT_TOKENS and request["cache_prompt"] is False, "invalid request settings")


def response_evidence(path):
    raw = path.read_bytes()
    response = json.loads(raw)
    content = response["content"]
    require(isinstance(content, str) and content.strip(), "response content is empty or whitespace")
    tokens = response["tokens"]
    require(isinstance(tokens, list) and len(tokens) == OUTPUT_TOKENS, "expected 512 output IDs")
    require(all(map(token_id, tokens)), "output ID outside the Apex vocabulary")
    require(response.get("ids", tokens) == tokens, "response token arrays disagree")
    require(response["tokens_evaluated"] == PROMPT_TOKENS, "prompt count differs")
    require(response["tokens_predicted"] == OUTPUT_TOKENS, "output count differs")
    timings = response["timings"]
    require(
        (timings["prompt_n"], timings["predicted_n"], timings["cache_n"]) == (PROMPT_TOKENS, OUTPUT_TOKENS, 0),
        "timing counts differ from the uncached 32K/512 workload",
    )
    for key in ("prompt_ms", "prompt_per_second", "predicted_ms", "predicted_per_second"):
        require(number(timings[key]) and timings[key] > 0, f"invalid {key}")

    rows = response["completion_probabilities"]
    require(len(rows) == OUTPUT_TOKENS, "expected a probability row per output ID")
    for index, (token, row) in enumerate(zip(tokens, rows, strict=True)):
        require(row["id"] == token and number(row["logprob"]), f"invalid probability row {index}")
        top = row["top_logprobs"]
        require(len(top) == 5, f"expected five alternatives at output index {index}")
        for alternative in top:
            require(
                token_id(alternative["id"]) and number(alternative["logprob"]),
                f"invalid probability alternative at output index {index}",
            )

    return tokens, {
        "response_sha256": digest(raw),
        "content_sha256": digest(content.encode("utf-8")),
        "token_ids_sha256": digest(compact(tokens)),
        "probability_rows_sha256": digest(compact(rows)),
        "content_characters": len(content),
        "distinct_output_tokens": len(set(tokens)),
        "timings": timings,
        "prefill_tokens_per_second_from_ms": 1000 * PROMPT_TOKENS / timings["prompt_ms"],
        "decode_steps": OUTPUT_TOKENS - 1,
        "decode_steps_per_second_from_ms": 1000 * (OUTPUT_TOKENS - 1) / timings["predicted_ms"],
    }


def verify_step_timings(timings):
    prefill_ms = timings["prompt_ms"]
    decode_ms = timings["predicted_ms"]
    for key, expected in (
        ("prompt_per_token_ms", prefill_ms / PROMPT_TOKENS),
        ("prompt_per_second", 1000 * PROMPT_TOKENS / prefill_ms),
        ("predicted_per_token_ms", decode_ms / (OUTPUT_TOKENS - 1)),
        ("predicted_per_second", 1000 * (OUTPUT_TOKENS - 1) / decode_ms),
    ):
        value = timings[key]
        require(number(value) and math.isclose(value, expected, rel_tol=1e-6),
                f"{key} does not match the measured interval and step count")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", action="store_true",
                        help="require valid step timings and report response differences without rejecting them")
    parser.add_argument("request", type=Path)
    parser.add_argument("reference", type=Path, help="one archived EXP-1476 response")
    parser.add_argument("responses", nargs="+", type=Path)
    args = parser.parse_args()
    try:
        verify_request(args.request)
        reference_tokens, reference = response_evidence(args.reference)
        require(
            (reference["content_sha256"], reference["token_ids_sha256"], reference["probability_rows_sha256"])
            == (CONTENT_SHA, TOKENS_SHA, PROBABILITY_SHA),
            "reference hashes differ from EXP-1476",
        )
        if args.capture:
            verify_step_timings(reference["timings"])
    except (OSError, ValueError, TypeError, KeyError, OverflowError) as error:
        parser.exit(1, f"Invalid reference input: {error}\n")

    valid = True
    for path in args.responses:
        try:
            tokens, evidence = response_evidence(path)
            if args.capture:
                verify_step_timings(evidence["timings"])
            evidence["first_different_output_index"] = next(
                (i for i, (expected, actual) in enumerate(zip(reference_tokens, tokens, strict=True)) if actual != expected),
                None,
            )
            evidence["matches_content_and_ids"] = (
                evidence["content_sha256"] == CONTENT_SHA and evidence["token_ids_sha256"] == TOKENS_SHA
            )
            evidence["matches_probability_rows"] = evidence["probability_rows_sha256"] == PROBABILITY_SHA
            if not args.capture:
                valid &= evidence["matches_content_and_ids"]
        except (OSError, ValueError, TypeError, KeyError, OverflowError) as error:
            evidence = {"error": str(error)}
            valid = False
        print(json.dumps({"path": str(path), "request_sha256": REQUEST_SHA, **evidence}))
    return 0 if valid else 1


if __name__ == "__main__":
    raise SystemExit(main())
