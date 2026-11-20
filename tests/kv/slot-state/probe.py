#!/usr/bin/env python3
"""Check the existing slot state format and exact continuation on one Apex server."""
import hashlib
import json
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path


def replay_signature(response):
    timings = response["timings"]
    if timings["cache_n"] <= 0 or timings["prompt_n"] <= 0:
        raise RuntimeError("replay must reuse the saved prefix and evaluate its suffix")
    if not response["tokens"] or len(response["completion_probabilities"]) != len(response["tokens"]):
        raise RuntimeError("replay must return tokens and probabilities")
    return tuple(response[k] for k in ("content", "tokens", "completion_probabilities")) + (
        timings["cache_n"], timings["prompt_n"],
    )


def main():
    port, output_dir, slot_dir, save_name, restore_name = sys.argv[1:]
    output = Path(output_dir)
    state = Path(slot_dir)
    for name in (save_name, restore_name):
        if name and not re.fullmatch(r"[A-Za-z0-9_-]+\.bin", name):
            raise SystemExit("slot state filenames must be simple .bin names")
    if save_name and ((state / save_name).exists() or (state / save_name).is_symlink()):
        raise SystemExit("slot output already exists")
    if restore_name and not (state / restore_name).is_file():
        raise SystemExit("slot input is missing")

    request = json.loads((output / "request.json").read_text())
    if not isinstance(request["prompt"], str) or not request["prompt"].strip():
        raise SystemExit("slot check needs a text prompt")
    request.update(id_slot=0, return_tokens=True, n_probs=5, stream=False)
    continuation = dict(request, prompt=request["prompt"] + "\n\nSummarize the passage in one paragraph:", cache_prompt=True)

    def post(path, payload, name):
        call = urllib.request.Request(
            f"http://127.0.0.1:{int(port)}/{path}", data=json.dumps(payload).encode(),
            headers={"Content-Type": "application/json"},
        )
        try:
            with urllib.request.urlopen(call, timeout=120) as response:
                raw = response.read()
        except urllib.error.HTTPError as error:
            (output / f"{name}.error.json").write_bytes(error.read())
            raise
        (output / f"{name}.json").write_bytes(raw)
        return json.loads(raw)

    def slot(action, filename, label):
        payload = {"filename": filename} if filename else {}
        result = post(f"slots/0?action={action}", payload, label)
        if result["id_slot"] != 0:
            raise RuntimeError("slot result refers to another slot")
        return result

    def restore(filename, label):
        restored = slot("restore", filename, label)
        if restored["n_restored"] <= 0 or restored["n_read"] != (state / filename).stat().st_size:
            raise RuntimeError("slot restore did not consume the saved state")
        return restored

    def replay(label):
        result = post("completion", continuation, label)
        sig = replay_signature(result)
        digest = hashlib.sha256(json.dumps(sig, sort_keys=True).encode()).hexdigest()
        print(f"{label}: cached={sig[-2]} evaluated={sig[-1]} tokens={len(sig[1])} replay_sha256={digest}")
        return sig

    local = None
    if save_name:
        priming = post("completion", dict(request, n_predict=1, cache_prompt=False), "prime")
        saved = slot("save", save_name, "save")
        if saved["n_saved"] < priming["timings"]["prompt_n"] or saved["n_written"] != (state / save_name).stat().st_size:
            raise RuntimeError("slot save did not write the full prefix state")
        local = replay("warm")
        if slot("erase", "", "erase-local")["n_erased"] < saved["n_saved"]:
            raise RuntimeError("slot erase did not clear the warm state")
        restored = restore(save_name, "restore-local")
        if restored["n_restored"] != saved["n_saved"] or local != replay("replayed-local"):
            raise RuntimeError("local slot state round trip changed the continuation")

    if restore_name:
        slot("erase", "", "erase-import")
        restore(restore_name, "restore-import")
        imported = replay("replayed-import")
        if local is not None and imported != local:
            raise RuntimeError("imported slot state changed the continuation")
    print("slot state replay passed")


def compare(paths):
    if len(paths) < 2:
        raise SystemExit("--compare requires two or more saved replay responses")
    expected = replay_signature(json.loads(Path(paths[0]).read_text()))
    for path in paths[1:]:
        if replay_signature(json.loads(Path(path).read_text())) != expected:
            raise SystemExit(f"slot replay differs from {paths[0]}: {path}")
    print(f"{len(paths)} replays match, with {expected[-2]} cached and {expected[-1]} evaluated input tokens")


if __name__ == "__main__":
    if sys.argv[1:2] == ["--compare"]:
        compare(sys.argv[2:])
    else:
        main()
