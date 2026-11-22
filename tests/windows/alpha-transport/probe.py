import json
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

def fetch(path, data=None):
    payload = None if data is None else json.dumps(data).encode()
    request = Request("http://127.0.0.1:8815" + path, payload,
                      {"Content-Type": "application/json"} if payload else {})
    try:
        with urlopen(request, timeout=4) as response:
            return response.status, response.headers.get_content_type(), response.read()
    except HTTPError as response:
        return response.code, response.headers.get_content_type(), response.read()

for _ in range(35):
    try:
        code, _, body = fetch("/health")
        assert code == 200 and json.loads(body)["status"] == "ok"
        break
    except URLError:
        time.sleep(0.3)
else:
    raise SystemExit("Windows test server did not listen on loopback")

code, mime, body = fetch("/")
assert code == 200 and mime == "text/html" and b"/immutable/assets/ember." in body
code, mime, body = fetch("/favicon.svg")
assert code == 200 and mime == "image/svg+xml" and b"<svg" in body
code, _, _ = fetch("/../model-path.txt")
assert code == 404

code, _, body = fetch("/v1/models")
assert code == 200 and json.loads(body)["data"][0]["id"] == "frankenmoe"
data = {"model": "frankenmoe", "messages": [{"role": "user", "content": "Unicode test \u0433\u0443\u0441\u044c"}],
        "temperature": 0, "max_tokens": 1}
code, _, body = fetch("/v1/chat/completions", data)
assert code == 200, body
assert json.loads(body)["choices"][0]["message"]["content"] == "fixture reply"
code, mime, body = fetch("/v1/chat/completions", {**data, "stream": True})
assert code == 200 and mime == "text/event-stream", body
assert body.endswith(b"data: [DONE]\n\n")
chunk = json.loads(body.splitlines()[0][6:])
assert chunk["choices"][0]["delta"]["content"] == "fixture reply"
print("Windows WinSock, Cyrillic file mapping, Ember assets, OpenAI chat and SSE passed with fixture model.")
