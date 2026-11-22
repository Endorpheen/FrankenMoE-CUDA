import json
import os
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

LEGACY = os.environ.get("FRANKENMOE_HTTP_LEGACY") == "1"
CHAT = "/v1/chat/completions"
data = {
    "model": "Apex-35B-V1.0-Q4_K_M",
    "messages": [{"role": "user", "content": "PRIVATE-FIXTURE-PROMPT гусь"}],
    "temperature": 0,
    "max_tokens": 2,
}
tool = {"type": "function", "function": {"name": "get_datetime", "parameters": {"type": "object"}}}
parts = [{"type": "text", "text": "Hello, "}, {"type": "text", "text": "world"}]
ui = {
    **data, "tools": [tool, {**tool, "function": {**tool["function"], "name": "get_info"}}],
    "return_progress": True, "sse_ping_interval": 1, "reasoning_format": "auto",
    "reasoning_control": True, "backend_sampling": False, "timings_per_token": True,
}
native = {"prompt": "native fixture", "n_predict": 2, "temperature": 0}
cases = [
    ("server tool catalog", "/tools", None, False, 404, 200, "empty-list"),
    ("resumable streams", "/v1/streams/lookup", {"conversation_ids": ["fixture"]}, False, 404, 200, "empty-list"),
    ("server tool execution unavailable", "/tools", {"tool": "fixture"}, False, 404, 404, "unavailable"),
    ("plain chat", CHAT, data, False, 200, 200, "chat"),
    ("OpenCode output budget", CHAT, {**data, "max_tokens": 65536}, False, 400, 200, "chat"),
    ("OpenAI content parts", CHAT, {**data, "messages": [{"role": "user", "content": parts}]}, False, 400, 200, "chat"),
    ("tools", CHAT, {**data, "tools": [tool]}, False, 400, 200, "chat"),
    ("empty tools", CHAT, {**data, "tools": []}, False, 400, 200, "chat"),
    ("tool_choice alone", CHAT, {**data, "tool_choice": "auto"}, False, 400, 200, "chat"),
    ("response_format", CHAT, {**data, "response_format": {"type": "json_object"}}, False, 400, 200, "chat"),
    ("full UI JSON", CHAT, ui, False, 400, 200, "chat"),
    ("full UI SSE", CHAT, {**ui, "stream": True}, False, 400, 200, "chat-sse"),
    ("full UI SSE without tools", CHAT, {**{k: v for k, v in ui.items() if k != "tools"}, "stream": True}, False, 200, 200, "chat-sse"),
    ("native completion", "/completion", native, False, 200, 200, "native"),
    ("chat body on native route", "/completion", data, False, 400, 400, "route-error"),
    ("native completion SSE", "/completion", {**native, "stream": True}, False, 400, 200, "native-sse"),
    ("OpenAI text SSE", "/v1/completions", {"prompt": native["prompt"], "max_tokens": 2, "temperature": 0, "stream": True}, False, 404, 200, "text-sse"),
    ("native alias", "/v1/completion", native, False, 404, 200, "native"),
    ("trailing slash and UTF-8 BOM", CHAT + "/", data, True, 404, 200, "chat"),
    ("unsupported image part", CHAT, {**data, "messages": [{"role":"user", "content":[{"type":"image_url","image_url":{"url":"https://invalid.example"}}]}]}, False, 400, 400, "text-only-error"),
]

if sys.argv[1:] == ["--count"]:
    print(6 + len(cases))
    raise SystemExit

def fetch(path, data=None, bom=False):
    payload = None if data is None else json.dumps(data).encode("utf-8")
    if bom:
        payload = b"\xef\xbb\xbf" + payload
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
assert fetch("/../model-path.txt")[0] == 404
code, _, body = fetch("/v1/models")
assert code == 200 and json.loads(body)["data"][0]["id"] == "frankenmoe"
assert fetch("/props?PRIVATE-FIXTURE-QUERY")[0] == 200

for name, path, request, bom, before, after, kind in cases:
    status, mime, body = fetch(path, request, bom)
    assert status == (before if LEGACY else after), (name, status, body)
    print(f"{name}: {status}")
    if status >= 400:
        if not LEGACY:
            detail = json.loads(body)["error"]
            assert "unsupported chat feature" not in detail
            if kind == "route-error":
                assert "/v1/chat/completions" in detail
            if kind == "text-only-error":
                assert "text parts" in detail
        continue
    if kind.endswith("-sse"):
        assert mime == "text/event-stream" and body.endswith(b"data: [DONE]\n\n")
        events = [json.loads(line[6:]) for line in body.splitlines()
                  if line.startswith(b"data: ") and line != b"data: [DONE]"]
        response = events[-1]
    else:
        response = json.loads(body)
    if kind == "empty-list":
        assert mime == "application/json" and response == [], (name, response)
        continue
    if kind == "chat":
        answer = response["choices"][0]["message"]["content"]
    elif kind == "chat-sse":
        answer = "".join(event["choices"][0]["delta"].get("content") or "" for event in events)
    elif kind == "text-sse":
        answer = response["choices"][0]["text"]
    else:
        answer = response["text"]
    assert answer == "fixture reply", (name, answer)
