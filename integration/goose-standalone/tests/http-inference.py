"""Check framed localhost HTTP inference against the synthetic CPU-graph GGUF."""
import http.client
import json
import math
import select
import socket
import subprocess
import sys


def exchange(port, head, body=b"", *, fragment=False, eof=False):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as conn:
        if fragment:
            split = len(body) // 2
            conn.sendall(head + body[:split])
            ready, _, _ = select.select([conn], [], [], 0.1)
            assert not ready, "server answered before the body was complete"
            conn.sendall(body[split:])
        else:
            conn.sendall(head + body)
        if eof:
            conn.shutdown(socket.SHUT_WR)
        response = http.client.HTTPResponse(conn)
        response.begin()
        assert response.getheader("Connection") == "close"
        return response.status, json.loads(response.read())


def get(path):
    return f"GET {path} HTTP/1.1\r\nHost: localhost\r\n\r\n".encode()


def post(length, content_type="application/json", extra=""):
    return (
        "POST /completion HTTP/1.1\r\n"
        "Host: localhost\r\n"
        f"Content-Type: {content_type}\r\n"
        f"Content-Length: {length}\r\n{extra}\r\n"
    ).encode()


def complete(port, options):
    body = json.dumps(options).encode()
    return exchange(port, post(len(body)), body)


def check(binary, fixture):
    with socket.socket() as candidate:
        candidate.bind(("127.0.0.1", 0))
        port = candidate.getsockname()[1]

    limit = 23
    with subprocess.Popen(
        [binary, "--ctx-size", "262144", fixture, "--serve", str(port), str(limit)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    ) as server:
        try:
            ready, _, _ = select.select([server.stderr], [], [], 5)
            assert ready, "listener did not start"
            line = server.stderr.readline()
            assert line == f"listening on 127.0.0.1:{port}\n", line

            assert exchange(port, get("/health")) == (200, {"status": "ok"})
            status, props = exchange(port, get("/props"))
            assert status == 200 and props["n_vocab"] == 4 and props["n_layer"] == 2
            assert props["n_ctx"] == 262144
            assert exchange(port, post(2, "text/plain"), b"ab") == (
                200, {"ids": [2], "text": "ab", "count": 1},
            )

            body = br'{"prompt":"\u0061b","n_predict":2,"temperature":0.0,"stream":false}'
            assert exchange(port, post(len(body), "application/json; charset=utf-8"), body, fragment=True) == (
                200, {"ids": [2, 2], "text": "abab", "count": 2},
            )
            body = json.dumps({"prompt": [2] * 129, "n_predict": 2, "ignore_eos": True}).encode()
            assert exchange(port, post(len(body)), body) == (
                200, {"ids": [2, 2], "text": "abab", "count": 2},
            )
            for bad_prompt in ([4], [2, [1]], [2, True]):
                body = json.dumps({"prompt": bad_prompt}).encode()
                assert exchange(port, post(len(body)), body)[0] == 400
            profile = {
                "prompt": [2], "n_predict": 2, "temperature": 1.0, "seed": 123,
                "cache_prompt": False, "ignore_eos": True, "n_probs": 5,
                "return_tokens": True, "top_p": 0.95, "top_k": 20, "min_p": 0,
            }
            status, result = complete(port, profile)
            assert status == 200
            assert result["tokens"] == result["ids"] == [2, 2]
            assert result["content"] == "abab" and result["stop_type"] == "limit"
            assert result["timings"]["prompt_n"] == 1 and result["timings"]["predicted_n"] == 2
            rows = result["completion_probabilities"]
            assert len(rows) == 2
            logit = 1 / math.sqrt(1 / 32 + 1e-5)
            logp = -math.log1p(3 * math.exp(-logit))
            for row in rows:
                assert row["id"] == 2 and row["token"] == "ab" and row["bytes"] == [97, 98]
                assert math.isclose(row["logprob"], logp, abs_tol=1e-6)
                choices = row["top_logprobs"]
                assert choices[0]["id"] == 2 and {item["id"] for item in choices} == {0, 1, 2, 3}
                assert math.isclose(sum(math.exp(item["logprob"]) for item in choices), 1.0, abs_tol=1e-6)
            long_prompt = dict(profile, prompt=[2] * 32767 + [4], n_predict=512)
            assert complete(port, long_prompt) == (400, {"error": "invalid prompt token"})

            # The first MT19937 canonical draw for seed 1 exceeds 0.99.
            # Seed 17 falls in the first candidate's interval for these logits.
            sampled = {"prompt": [2], "temperature": 20.0, "seed": 1, "ignore_eos": True}
            for options, expected in (
                (sampled, 2),
                (sampled, 2),
                (dict(sampled, seed=17), 0),
                (dict(sampled, ignore_eos=False), 3),
            ):
                status, result = complete(port, options)
                assert status == 200 and result["ids"] == [expected], result
            for invalid in ({"top_p": 1.01}, {"seed": 4294967296}, {"cache_prompt": True}):
                assert complete(port, dict(profile, **invalid))[0] == 400
            assert exchange(port, get("/unknown"))[0] == 404
            assert exchange(port, post(2, extra="Content-Length: 2\r\n"), b"ab")[0] == 400
            assert exchange(port, post(9223372036854775808))[0] == 400
            assert exchange(port, post(262144))[0] == 413
            assert exchange(port, post(4, "text/plain"), b"ab", eof=True)[0] == 400
            nested = b'{"prompt":{"value":"ab"}}'
            assert exchange(port, post(len(nested)), nested)[0] == 400
            assert server.wait(timeout=5) == 0, server.stderr.read()
        finally:
            if server.poll() is None:
                server.kill()
    print(f"HTTP inference: {limit} requests verified")


if __name__ == "__main__":
    check(*sys.argv[1:])
