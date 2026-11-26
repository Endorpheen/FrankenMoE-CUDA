"""Exercise the serial Vulkan HTTP contract against the pinned Qwen2 fixture."""

import http.client
import json
import sys


PROMPT = "The capital of France is"
INPUT = [785, 6722, 315, 9625, 374]
OUTPUT = [12095, 11, 323, 432]
TEXT = " Paris, and it"
USAGE = {"prompt_tokens": 5, "completion_tokens": 4, "total_tokens": 9}


def main():
    port = int(sys.argv[1])

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
    request("POST", "/v1/chat/completions", {"messages": []}, 404)
    print("Vulkan HTTP: health, model listing, tokenization, both completions, reset, and errors PASS")


if __name__ == "__main__":
    main()
