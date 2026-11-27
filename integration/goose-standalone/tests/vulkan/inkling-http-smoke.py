"""Check the complete Vulkan Inkling model against the CUDA token fixture."""

import json
from pathlib import Path
import sys
import urllib.request


def main():
    port = int(sys.argv[1])
    path = Path(__file__).with_name("inkling-http.json")
    reference = json.loads(path.read_text(encoding="utf-8"))

    def request(route, data=None):
        req = urllib.request.Request(f"http://127.0.0.1:{port}{route}",
                                     json.dumps(data).encode() if data is not None else None,
                                     {"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=75) as response:
            return json.load(response)

    assert request("/v1/models")["data"][0]["id"] == "inkling-2-mini"
    assert request("/tokenize", {"content": reference[0]["text"]})["tokens"] == reference[0]["prompt"]
    results = []
    for case in [*reference, reference[0]]:
        response = request("/completion", {
            "prompt": case["text"], "n_predict": case["n_predict"],
            "temperature": 0, "return_tokens": True,
        })
        ids = case["tokens"][:-1] if case["finish_reason"] == "stop" else case["tokens"]
        assert response["tokens"] == ids and response["content"] == case["content"]
        assert response["finish_reason"] == case["finish_reason"]
        assert response["usage"]["prompt_tokens"] == len(case["prompt"])
        results.append(response)
    assert results[-1] == results[0]
    print("Inkling HTTP model identity, tokenizer, CUDA token parity and reset PASS")


if __name__ == "__main__":
    main()
