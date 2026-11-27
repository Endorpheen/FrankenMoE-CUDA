"""Check the Qwen2 Vulkan CLI tokenizer without opening a Vulkan device."""

from pathlib import Path
import json
import subprocess
import sys
import tempfile


def main():
    binary, model = sys.argv[1:3]
    fixture = Path(__file__).with_name("qwen2-tokenizer.json")
    cases = json.loads(fixture.read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory() as directory:
        prompt = Path(directory) / "prompt.txt"
        for case in cases:
            prompt.write_bytes(case["text"].encode("utf-8"))
            argv = [binary, model, "--prompt-file", str(prompt), "--tokenize-only"]
            if case["parse_special"]:
                argv.append("--special")
            actual = [int(token) for token in subprocess.check_output(argv).split()]
            if actual != case["tokens"]:
                raise AssertionError(f"{case['case']}: {actual} != {case['tokens']}")
            print(f"{case['case']}: {len(actual)} token IDs PASS")


if __name__ == "__main__":
    main()
