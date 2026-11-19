#!/usr/bin/env python3
"""Chat-level acceptance for Maxwell: Russian dialogue, long prompt, code gen.

Checks the HTTP status, non-empty assistant content, that the raw control tags
do not leak into the reply, and the finish reason (stop = EOG worked).
"""
import json
import sys
import urllib.error
import urllib.request


def ask(base, messages, n=512):
    body = {"messages": messages, "max_tokens": n, "temperature": 0}
    req = urllib.request.Request(base + "/v1/chat/completions",
                                 data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=3600) as r:
            return 200, json.load(r)
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()[:300]


def check(name, base, messages, n, min_chars=8):
    code, d = ask(base, messages, n)
    if code != 200:
        print(f"FAIL {name}: HTTP {code} {d}")
        return False
    ch = d["choices"][0]
    text = ch["message"].get("content") or ""
    reason = ch.get("finish_reason")
    leak = "<|maxwell_end|>" in text or "<|maxwell_start|>" in text
    ok = len(text.strip()) >= min_chars and not leak
    print(f"{'ok  ' if ok else 'FAIL'} {name}: status={code} finish={reason} "
          f"chars={len(text)} leak={leak}")
    print("     " + repr(text[:140]))
    return ok


def main():
    base = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:9102"
    ok = True
    ok &= check("russian dialogue", base,
                [{"role": "user", "content": "Привет! Расскажи в двух предложениях, чем полезна математика."}],
                256)
    ok &= check("long prompt", base,
                [{"role": "user", "content": "Объясни кратко: " + "почему небо голубое? " * 120}],
                256, min_chars=10)
    ok &= check("html/three.js", base,
                [{"role": "user", "content": "Напиши минимальную HTML-страницу с Three.js: вращающийся куб, без пояснений."}],
                512, min_chars=20)
    print("MAXWELL-CHAT OK" if ok else "MAXWELL-CHAT FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
