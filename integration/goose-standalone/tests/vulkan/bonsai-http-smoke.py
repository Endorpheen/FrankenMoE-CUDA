"""Check the Bonsai Vulkan text HTTP path against the pinned CUDA oracle."""

import http.client
import json
from pathlib import Path
import sys


def main():
    port = int(sys.argv[1])
    path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).with_name("bonsai-http.json")
    fixture = json.loads(path.read_text(encoding="utf-8"))

    def request(method, route, body=None, stream=False):
        conn = http.client.HTTPConnection("127.0.0.1", port, timeout=30)
        try:
            conn.request(method, route, json.dumps(body).encode() if body is not None else None,
                         headers={"Content-Type": "application/json"} if body is not None else {})
            response = conn.getresponse()
            assert response.status == 200, (route, response.status, response.read())
            if not stream:
                return json.loads(response.read())
            assert response.getheader("Content-Type").startswith("text/event-stream")
            chunks = []
            while True:
                line = response.readline()
                assert line.startswith(b"data: ") and response.readline() == b"\n"
                if line == b"data: [DONE]\n":
                    return chunks
                chunks.append(json.loads(line[6:]))
        finally:
            conn.close()

    assert request("GET", "/health")["status"] == "ok"
    assert request("GET", "/v1/models")["data"][0]["id"] == "bonsai-2.5-27b"
    for case in fixture["tokenizers"]:
        assert request("POST", "/tokenize", {
            "content": case["text"], "add_special": False, "parse_special": case["special"]
        })["tokens"] == case["tokens"]
    prose = fixture["prose"]
    plain = {"prompt": prose["text"], "temperature": 0, "n_predict": 4, "return_tokens": True}
    first = request("POST", "/completion", plain)
    assert first["content"] == prose["content"] and first["tokens"] == prose["tokens"][:-1]
    assert prose["tokens"][-1] == 248046 and first["finish_reason"] == "stop"
    assert first["usage"]["prompt_tokens"] == len(prose["prompt"])
    eog = request("POST", "/completion", {"prompt": [1511, 17, 248046, 1511], "n_predict": 4})
    assert eog["content"] == "2" and eog["finish_reason"] == "stop"
    chat = fixture["chat"]
    response = request("POST", "/v1/chat/completions", {
        "model": "bonsai-2.5-27b", "messages": [{"role": "user", "content": chat["text"]}],
        "max_tokens": 2, "enable_thinking": False, "temperature": 0, "return_tokens": True,
    })
    assert response["model"] == "bonsai-2.5-27b"
    assert response["choices"][0]["message"]["content"] == chat["content"]
    assert response["choices"][0]["tokens"] == chat["tokens"]
    assert response["usage"]["prompt_tokens"] == len(chat["prompt"])
    chunks = request("POST", "/v1/completions", {
        "prompt": prose["prompt"], "temperature": 0, "max_tokens": 4,
        "stream": True, "stream_options": {"include_usage": True},
    }, stream=True)
    assert all(c["model"] == "bonsai-2.5-27b" for c in chunks)
    assert "".join(c["choices"][0]["text"] for c in chunks if c["choices"]) == prose["content"]
    assert chunks[-1]["usage"] == first["usage"]
    assert request("POST", "/completion", plain) == first
    print("Bonsai HTTP: model identity, tokenizer, completions, no-thinking chat, SSE and reset PASS")


if __name__ == "__main__":
    main()
