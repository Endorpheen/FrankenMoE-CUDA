"""Exercise the serial Vulkan HTTP contract against the pinned Qwen2 fixture."""

import http.client
import json
from pathlib import Path
import sys


PROMPT = "The capital of France is"
INPUT = [785, 6722, 315, 9625, 374]
OUTPUT = [12095, 11, 323, 432]
TEXT = " Paris, and it"
USAGE = {"prompt_tokens": 5, "completion_tokens": 4, "total_tokens": 9}
CHAT_PATH = "/v1/chat/completions"
CHAT = {"messages": [{"role": "user", "content": "What is the capital of France? Answer in one word."}],
        "temperature": 0.0, "max_tokens": 4}


def main():
    port = int(sys.argv[1])
    expected_chat = json.loads(Path(__file__).with_name("qwen2-chat.json").read_text(encoding="utf-8"))

    def request(method, path, body=None, status=200):
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=30)
        try:
            content = body if isinstance(body, bytes) else json.dumps(body).encode() if body is not None else None
            connection.request(method, path, body=content,
                               headers={"Content-Type": "application/json"} if content is not None else {})
            response = connection.getresponse()
            result = json.loads(response.read())
            if response.status != status:
                raise AssertionError((path, response.status, result))
            return result
        finally:
            connection.close()

    assert request("GET", "/health")["status"] == "ok"
    assert request("GET", "/v1/models")["data"][0]["id"] == "qwen2.5-0.5b-instruct"
    assert request("POST", "/tokenize", {"content": PROMPT, "add_special": False})["tokens"] == INPUT
    first = request("POST", "/completion", {"prompt": PROMPT, "n_predict": 4,
                                          "temperature": 0.0, "return_tokens": True})
    assert first["content"] == TEXT and first["tokens"] == OUTPUT and first["usage"] == USAGE
    assert request("POST", "/completion", {"prompt": [785], "n_predict": 1})["usage"]["prompt_tokens"] == 1
    second = request("POST", "/v1/completions", {"prompt": INPUT, "max_tokens": 4,
                                                "model": "qwen2.5-0.5b-instruct", "return_tokens": True})
    choice = second["choices"][0]
    assert choice["text"] == TEXT and choice["tokens"] == OUTPUT and second["usage"] == USAGE
    request("POST", "/completion", {"prompt": INPUT, "n_predict": 32768}, 400)
    request("POST", "/completion", {"prompt": PROMPT, "stream": True}, 400)
    request("POST", "/completion", b'{"prompt":', 400)
    request("POST", CHAT_PATH, {"messages": []}, 400)
    chat = request("POST", CHAT_PATH, CHAT)
    assert chat["object"] == "chat.completion"
    assert chat["choices"][0]["message"] == {"role": "assistant", "content": expected_chat["content"]}
    assert chat["usage"]["prompt_tokens"] == expected_chat["prompt_tokens"]
    assert chat["choices"][0]["finish_reason"] == expected_chat["finish_reason"]

    def stream(path, body, disconnect=False):
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=30)
        try:
            data = json.dumps({**body, "stream": True, "stream_options": {"include_usage": True}}).encode()
            connection.request("POST", path, body=data, headers={"Content-Type": "application/json"})
            response = connection.getresponse()
            assert response.status == 200 and response.getheader("Content-Type").startswith("text/event-stream")
            events = []
            while True:
                line = response.readline()
                assert line.startswith(b"data: ")
                assert response.readline() == b"\n"
                if line == b"data: [DONE]\n":
                    break
                events.append(json.loads(line[6:]))
                if disconnect:
                    return events
            assert len({event["id"] for event in events}) == 1
            return events
        finally:
            connection.close()

    chat_stream = stream(CHAT_PATH, CHAT)
    assert chat_stream[0]["choices"][0]["delta"] == {"role": "assistant"}
    assert all(event["object"] == "chat.completion.chunk" for event in chat_stream)
    streamed_chat = "".join(event["choices"][0]["delta"].get("content", "")
                            for event in chat_stream if event["choices"])
    assert streamed_chat == expected_chat["content"] and chat_stream[-1]["usage"] == chat["usage"]
    assert chat_stream[-2]["choices"][0]["finish_reason"] == chat["choices"][0]["finish_reason"]
    text_stream = stream("/v1/completions", {"prompt": INPUT, "max_tokens": 4})
    assert "".join(event["choices"][0]["text"] for event in text_stream if event["choices"]) == TEXT
    assert text_stream[-1]["usage"] == USAGE
    stream(CHAT_PATH, CHAT, disconnect=True)
    replay = request("POST", "/completion", {"prompt": INPUT, "n_predict": 4, "return_tokens": True})
    assert replay["content"] == TEXT and replay["tokens"] == OUTPUT and replay["usage"] == USAGE
    print("Vulkan HTTP: completions, ChatML, SSE, usage, reset, disconnect, and errors PASS")


if __name__ == "__main__":
    main()
