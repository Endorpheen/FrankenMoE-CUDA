"""Check the real Inkling chat template and streaming reasoning frames."""

import http.client
import json
from pathlib import Path
import sys


def main():
    port = int(sys.argv[1])
    expected = json.loads(Path(__file__).with_name("inkling-chat.json").read_text())
    base = {"model": "inkling-2-mini", "messages": [{"role": "user", "content": "Paris?"}],
            "max_tokens": 5, "temperature": 0, "reasoning_effort": "none"}

    def post(path, body, stream=False):
        conn = http.client.HTTPConnection("127.0.0.1", port, timeout=75)
        try:
            conn.request("POST", path, json.dumps(body).encode(), {"Content-Type": "application/json"})
            result = conn.getresponse()
            assert result.status == 200, (result.status, result.read())
            if not stream:
                return json.loads(result.read())
            assert result.getheader("Content-Type").startswith("text/event-stream")
            chunks = []
            while True:
                line = result.readline()
                assert line.startswith(b"data: ") and result.readline() == b"\n"
                if line == b"data: [DONE]\n":
                    return chunks
                chunks.append(json.loads(line[6:]))
        finally:
            conn.close()

    assert post("/tokenize", {"content": expected["prompt_text"], "parse_special": True})["tokens"] == expected["prompt_ids"]
    r = post("/v1/chat/completions", {**base, "return_tokens": True})
    choice = r["choices"][0]
    assert choice["tokens"] == expected["output_ids"]
    assert choice["message"] == {"role": "assistant", "content": expected["content"],
                                 "reasoning_content": expected["reasoning"]}
    assert r["usage"]["prompt_tokens"] == len(expected["prompt_ids"])
    chunks = post("/v1/chat/completions", {**base, "stream": True,
                  "stream_options": {"include_usage": True}}, stream=True)
    deltas = [c["choices"][0]["delta"] for c in chunks if c["choices"]]
    assert "".join(d.get("reasoning_content", "") for d in deltas) == expected["reasoning"]
    assert "".join(d.get("content", "") for d in deltas) == expected["content"]
    assert chunks[-1]["usage"] == r["usage"]
    print("Inkling chat: model template, exact token IDs, reasoning and SSE PASS")


if __name__ == "__main__":
    main()
