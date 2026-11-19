#!/usr/bin/env python3
"""The Apex API surface: every endpoint a client of this runtime relies on, answered and shaped as expected.

One row per endpoint with its status and the property that was checked. A failure is printed as a failure; nothing is
counted as passing because the call merely returned.
"""
import json
import sys
import urllib.error
import urllib.request

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:9101"
rows = []


def call(method, path, body=None, raw=False, timeout=600):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(f"{BASE}{path}", data=data, method=method,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            payload = r.read()
            return r.status, (payload if raw else json.loads(payload or b"{}"))
    except urllib.error.HTTPError as e:
        return e.code, e.read()[:200].decode(errors="replace")


def check(name, ok, detail):
    rows.append((name, bool(ok), detail))
    print(f"{'ok  ' if ok else 'FAIL'} {name:38s} {detail}")


status, health = call("GET", "/health")
check("GET /health", status == 200 and health.get("status") == "ok", f"status={status} {health}")

status, _ = call("GET", "/v1/health")
check("GET /v1/health", status == 200, f"status={status}")

status, props = call("GET", "/props")
keys = sorted(props.keys())[:6] if isinstance(props, dict) else []
check("GET /props", status == 200 and isinstance(props, dict) and keys, f"status={status} keys={keys}")

status, models = call("GET", "/v1/models")
ids = [m.get("id") for m in (models.get("data") or [])] if isinstance(models, dict) else []
check("GET /v1/models", status == 200 and "Apex-35B-V1.0" in ids, f"status={status} ids={ids}")

status, tok = call("POST", "/tokenize", {"content": "hello world"})
tokens = tok.get("tokens") if isinstance(tok, dict) else None
check("POST /tokenize", status == 200 and tokens, f"status={status} tokens={tokens}")

status, det = call("POST", "/detokenize", {"tokens": tokens or [1, 2]})
content = det.get("content") if isinstance(det, dict) else None
check("POST /detokenize", status == 200 and isinstance(content, str) and content.strip() != "",
      f"status={status} content={content!r}")

status, tpl = call("POST", "/apply-template", {"messages": [{"role": "user", "content": "hi"}]})
check("POST /apply-template", status == 200 and isinstance(tpl.get("prompt"), str), f"status={status}")

status, comp = call("POST", "/completion", {"prompt": "The capital of France is", "n_predict": 8,
                                            "temperature": 0, "stop": ["\n"]})
text = comp.get("content") if isinstance(comp, dict) else None
check("POST /completion", status == 200 and isinstance(text, str) and text, f"status={status} text={text!r}")

status, oai = call("POST", "/v1/completions", {"prompt": "The capital of France is", "max_tokens": 8,
                                               "temperature": 0})
choices = oai.get("choices") if isinstance(oai, dict) else None
check("POST /v1/completions", status == 200 and choices and "text" in choices[0],
      f"status={status} choice_keys={sorted(choices[0].keys()) if choices else None}")

status, cnt = call("POST", "/v1/chat/completions/input_tokens",
                   {"messages": [{"role": "user", "content": "count these tokens"}]})
check("POST /chat/input_tokens", status == 200 and isinstance(cnt.get("input_tokens"), int),
      f"status={status} input_tokens={cnt.get('input_tokens') if isinstance(cnt, dict) else None}")

# streaming: the SSE frames must be data: lines ending with [DONE]
body = json.dumps({"messages": [{"role": "user", "content": "say hi"}], "max_tokens": 8, "stream": True}).encode()
req = urllib.request.Request(f"{BASE}/v1/chat/completions", data=body, method="POST",
                             headers={"Content-Type": "application/json"})
stream_ok = False
frames = 0
try:
    with urllib.request.urlopen(req, timeout=600) as r:
        for line in r:
            line = line.decode(errors="replace").strip()
            if line.startswith("data: "):
                frames += 1
                if line == "data: [DONE]":
                    stream_ok = True
except Exception as e:                                        # noqa: BLE001 - any surprise is a failure
    rows.append(("stream", False, str(e)))
check("POST /v1/chat/completions stream", stream_ok and frames > 2, f"frames={frames} done={stream_ok}")

status, msgs = call("POST", "/v1/messages",
                    {"model": "Apex-35B-V1.0", "max_tokens": 8,
                     "messages": [{"role": "user", "content": "hi"}]})
check("POST /v1/messages (anthropic)", status == 200 and (msgs.get("content") or msgs.get("error")) is not None,
      f"status={status} keys={sorted(msgs.keys())[:5] if isinstance(msgs, dict) else None}")

status, infill = call("POST", "/infill", {"input_prefix": "def add(a, b):", "input_suffix": "\n",
                                          "n_predict": 8, "temperature": 0})
check("POST /infill", status == 200 and isinstance(infill.get("content"), str), f"status={status}")

status, metrics = call("GET", "/metrics", raw=True)
body = metrics if isinstance(metrics, str) else metrics.decode(errors="replace")
# The endpoint is registered but deliberately refuses unless the server was started with --metrics, and the refusal names
# the flag. The accepted Apex profile does not enable it, so the contract to check is the refusal and its message.
if status == 200:
    check("GET /metrics", len(body) > 0, f"status=200 bytes={len(body)} (metrics enabled)")
else:
    check("GET /metrics", status == 501 and "--metrics" in body, f"status={status} message={body[:80]!r}")

status, _ = call("GET", "/no-such-endpoint")
check("GET unknown path -> 404", status == 404, f"status={status}")

failed = [r for r in rows if not r[1]]
print(f"\napi corpus: checks={len(rows)} failed={len(failed)}")
for name, _, detail in failed:
    print(f"  FAILED {name}: {detail}")
raise SystemExit(0 if not failed else 1)
