"""Check the common HTTP routes with one resident Bonsai model."""
import json
from pathlib import Path
import sys
from urllib.request import Request, urlopen

root = Path(__file__).resolve().parents[3]
port = int(sys.argv[1])
out = Path(sys.argv[2])
reference = root / "experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/tokenizer-results.json"
prompt = root / "experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json"
cli = root / "experiments/EXP-2026-11-25-2002-bonsai-session/data/session.out"


def request(path, data=None):
    payload = json.dumps(data).encode() if data is not None else None
    req = Request(f"http://127.0.0.1:{port}{path}", data=payload,
                  headers={"Content-Type": "application/json", "Connection": "close"})
    with urlopen(req, timeout=30) as response:
        return json.load(response)


def save(name, result):
    (out / f"{name}.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n")


assert request("/health") == {"status": "ok"}
a = json.loads(prompt.read_text())
a["prompt"] = a["prompt"][:273]
a["n_predict"] = 8
b = dict(a, prompt=[1511, 17, 248046, 1511])
expected = [json.loads(line)["token"] for line in cli.read_text().splitlines()[:16]]
for name, req, tokens in (("a1", a, expected[:8]), ("b", b, expected[8:]), ("a2", a, expected[:8])):
    result = request("/completion", req)
    save(name, result)
    assert result["tokens"] == tokens, result
for i, fixture in enumerate(json.loads(reference.read_text())["fixtures"]):
    common = dict(a, n_predict=1)
    text = request("/completion", dict(common, prompt=fixture["text"]))
    ids = request("/completion", dict(common, prompt=fixture["ids"]))
    save(f"text-{i}", text)
    save(f"ids-{i}", ids)
    assert text["tokens"] == ids["tokens"], (text, ids)
    assert text["tokens_evaluated"] == ids["tokens_evaluated"] == len(fixture["ids"]), (text, ids)
print("A–B–A and five text versus ID /completion pairs matched")
