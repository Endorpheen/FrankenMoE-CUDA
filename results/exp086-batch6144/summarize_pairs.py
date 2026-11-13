"""Summarize control vs candidate SSE completions from a manifest.

Reads SSE streams of the chat completions endpoint (one token per event, the
final stop=true event carries "timings"), compares the two arms token by
token and by probabilities, and writes the per-pair comparison. All numbers
come straight out of the SSE payloads: total_ms is the server-side
prompt_ms + predicted_ms, not a client TTFT.
"""

import argparse
import json
import math
import sys


class SSEError(Exception):
    """Input data is damaged or does not follow the expected format."""


def probability(logprob):
    """Convert a logprob to a probability.

    -inf is allowed and reads as probability zero; NaN and +inf are invalid.
    """
    if math.isnan(logprob) or (logprob > 0 and math.isinf(logprob)):
        return None
    if logprob == float("-inf"):
        return 0.0
    return math.exp(logprob)


def parse_sse(path):
    """Parse an SSE file into a mapping of the validated events.

    Every event that carries tokens also carries exactly one probability
    record per token. The last stop=true event carries "timings" and must
    not be truncated.
    """
    token_ids = []
    chosen_id = []
    chosen_prob = []
    top5_id = []
    top5_prob = []
    final = None
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            if not line.startswith("data:"):
                continue
            event = json.loads(line[len("data:"):].strip())
            if event.get("stop"):
                final = event
            tokens = event.get("tokens") or []
            token_ids.extend(tokens)
            records = event.get("completion_probabilities") or []
            if len(tokens) != len(records):
                raise SSEError(f"{path}: {len(tokens)} token ids but "
                               f"{len(records)} probability records")
            for index, record in enumerate(records):
                if record["id"] != tokens[index]:
                    raise SSEError(f"{path}: probability record id "
                                   f"{record['id']} differs from token id "
                                   f"{tokens[index]}")
                chosen_id.append(record["id"])
                if len(record["top_logprobs"]) != 5:
                    raise SSEError(f"{path}: expected 5 top logprobs, "
                                   f"got {len(record['top_logprobs'])}")
                top5_id.append([entry["id"] for entry in record["top_logprobs"][:5]])
                logprob = record["logprob"]
                chosen_prob.append(probability(logprob))
                if chosen_prob[-1] is None:
                    raise SSEError(f"{path}: invalid chosen logprob "
                                   f"{logprob}")
                top5_prob.append([])
                for entry in record["top_logprobs"][:5]:
                    entry_prob = probability(entry["logprob"])
                    if entry_prob is None:
                        raise SSEError(f"{path}: invalid logprob {entry['logprob']}")
                    top5_prob[-1].append(entry_prob)
    if final is None:
        raise SSEError(f"{path}: no stop event")
    if "timings" not in final:
        raise SSEError(f"{path}: final event has no timings")
    timings = final["timings"]
    if final.get("truncated"):
        raise SSEError(f"{path}: output was truncated")
    cache_n = timings.get("cache_n")
    prompt_n = timings.get("prompt_n")
    prompt_ms = timings.get("prompt_ms")
    predicted_ms = timings.get("predicted_ms")
    predicted_per_second = timings.get("predicted_per_second")
    if None in (cache_n, prompt_n, prompt_ms, predicted_ms, predicted_per_second):
        raise SSEError(f"{path}: timings is incomplete")
    if not (prompt_ms > 0 and predicted_ms > 0 and predicted_per_second > 0):
        raise SSEError(f"{path}: invalid non-positive timings")
    predicted_n = timings.get("predicted_n", final.get("tokens_predicted"))
    if predicted_n is None:
        raise SSEError(f"{path}: cannot resolve predicted_n")
    if cache_n != 0 or prompt_n != 32768 or predicted_n != 128:
        raise SSEError(f"{path}: expected cache_n=0, prompt_n=32768, "
                       f"predicted_n=128, got cache_n={cache_n}, "
                       f"prompt_n={prompt_n}, predicted_n={predicted_n}")
    if len(token_ids) != predicted_n:
        raise SSEError(f"{path}: collected {len(token_ids)} token ids, "
                       f"timings predict {predicted_n}")
    if any(len(values) != predicted_n
           for values in (chosen_id, chosen_prob, top5_id, top5_prob)):
        raise SSEError(f"{path}: probability arrays ({chosen_id.__len__()}"
                       f" chosen, {top5_id.__len__()} top5) do not match "
                       f"predicted_n={predicted_n}")
    return {
        "token_ids": token_ids,
        "chosen_id": chosen_id,
        "chosen_prob": chosen_prob,
        "top5_id": top5_id,
        "top5_prob": top5_prob,
        "prompt_ms": prompt_ms,
        "predicted_ms": predicted_ms,
        "predicted_per_second": predicted_per_second,
    }


def compare_streams(control, candidate):
    """Return (correct, max_delta, description) for one arm pair.

    Every position must select the same token id and the same ordered top-5
    ids, and the probability deltas must be within tolerance. A delta is only
    defined where the ids agree, so a mismatch fails correctness without
    fabricating a correspondence.
    """
    length = min(len(control["token_ids"]), len(candidate["token_ids"]))
    same_length = length == len(control["token_ids"]) == len(candidate["token_ids"])
    max_delta = 0.0
    first_mismatch = None
    first_big_delta = None
    for index in range(length):
        if (control["token_ids"][index] != candidate["token_ids"][index] or
                control["chosen_id"][index] != candidate["chosen_id"][index] or
                control["top5_id"][index] != candidate["top5_id"][index]):
            if first_mismatch is None:
                first_mismatch = index
            continue
        delta = abs(control["chosen_prob"][index] - candidate["chosen_prob"][index])
        max_delta = max(max_delta, delta)
        if delta > 0.001 and first_big_delta is None:
            first_big_delta = index
        for a, b in zip(control["top5_prob"][index], candidate["top5_prob"][index]):
            delta = abs(a - b)
            max_delta = max(max_delta, delta)
            if delta > 0.001 and first_big_delta is None:
                first_big_delta = index
    same_length = length == len(control["token_ids"]) == len(candidate["token_ids"])
    correct = first_mismatch is None and same_length and max_delta <= 0.001
    if first_mismatch is not None:
        index = first_mismatch
        description = (f"token id mismatch at position {index}: control id "
                       f"{control['chosen_id'][index]} vs candidate id "
                       f"{candidate['chosen_id'][index]}")
    elif first_big_delta is not None:
        index = first_big_delta
        description = (f"probabilities differ at position {index}: control id "
                       f"{control['chosen_id'][index]}, candidate id "
                       f"{candidate['chosen_id'][index]}, max delta "
                       f"{max_delta:.4g} exceeds 0.001")
    else:
        description = f"identical ids; max probability delta {max_delta:.4g}"
    return correct, max_delta, description


def fail(message):
    sys.exit(f"error: {message}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True,
                        help="path to the SSE pair manifest JSON")
    parser.add_argument("--output", required=True,
                        help="path of the JSON summary to write")
    args = parser.parse_args(argv)

    try:
        with open(args.manifest, encoding="utf-8") as handle:
            manifest = json.load(handle)
    except OSError as error:
        fail(f"cannot read manifest: {error}")
    if not isinstance(manifest, dict) or not isinstance(manifest.get("pairs"), list):
        fail("manifest must hold a 'pairs' array")
    if not manifest["pairs"]:
        fail("manifest has no pairs")

    pairs = []
    for entry in manifest["pairs"]:
        if "name" not in entry:
            fail("manifest pair has no name")
        control = parse_sse(entry["control"])
        candidate = parse_sse(entry["candidate"])
        prefill_reduction = 100.0 * (1.0 - candidate["prompt_ms"] / control["prompt_ms"])
        decode_change = 100.0 * (candidate["predicted_per_second"] / control["predicted_per_second"] - 1.0)
        correct, max_delta, description = compare_streams(control, candidate)
        pairs.append({
            "name": entry["name"],
            "control": {"prompt_ms": control["prompt_ms"],
                        "decode_tok_s": control["predicted_per_second"],
                        "total_ms": control["prompt_ms"] + control["predicted_ms"]},
            "candidate": {"prompt_ms": candidate["prompt_ms"],
                          "decode_tok_s": candidate["predicted_per_second"],
                          "total_ms": candidate["prompt_ms"] + candidate["predicted_ms"]},
            "prefill_time_reduction_pct": prefill_reduction,
            "decode_change_pct": decode_change,
            "correctness": correct,
            "max_delta": max_delta,
            "difference_summary": description,
        })

    with open(args.output, "w", encoding="utf-8") as handle:
        json.dump({"pairs": pairs}, handle, indent=2)
        handle.write("\n")
    print(f"{len(pairs)} pairs summarized")


if __name__ == "__main__":
    main()
