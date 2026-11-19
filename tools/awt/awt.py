#!/usr/bin/env python3
"""Agent Workload Test - a session shaped like a coding agent, not like a benchmark.

The acceptance request carries token ids, so it never exercises the chat template, the tokenizer, the transition from a
long prefill into the first sampled token, or a generation that runs for thousands of tokens. A real client does all four,
and two crashes so far happened exactly there. This test drives the OpenAI-compatible endpoint the way a client does and
checks, at every step, that the answer is well formed, that the server is still alive, and that the shipped runtime is
actually answering from Goose rather than silently from C++.

Modes:
  FAST   three steps, small context, about a minute - a pre-commit gate
  CHECK  a typical agent session: files, search, tool results, continuation, about twenty thousand tokens of context
  FULL   the same, to thirty-two thousand tokens with a three-thousand-token generation
  REPRO  the scenario that failed: several requests, then one long generation, matching n_prompt 21329 / n_gen 3383

Every run writes results/awt/<tag>-<timestamp>/ with the requests, the responses, the server log and summary.json
(git sha, config, seed, verdict, failure reason). A failure is reported as a failure: the first one stops the run.
"""
import argparse
import json
import os
import re
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request

GOOSE_MARKERS = {
    "tokenizer_from_port": ("FG_TK_BPE_SERVED", "the encode path answered from the port"),
    "sampler_stage":       ("FG171_TOP_P", "the ported sampling stage ran"),
    "dist_stage":          ("FG171_DIST", "the ported distribution stage ran"),
    "gguf_diag":           ("FG_GGUF_PARSE", "the GGUF reader check ran (only with FG_GGUF_DIAG)"),
}

TOOLS = [
    {"type": "function", "function": {
        "name": "read_file", "description": "Read a file",
        "parameters": {"type": "object",
                       "properties": {"path": {"type": "string"}, "limit": {"type": "integer"}},
                       "required": ["path"]}}},
    {"type": "function", "function": {
        "name": "search", "description": "Search the workspace",
        "parameters": {"type": "object",
                       "properties": {"query": {"type": "string"}, "glob": {"type": "string"}},
                       "required": ["query"]}}},
    {"type": "function", "function": {
        "name": "run", "description": "Run a command",
        "parameters": {"type": "object",
                       "properties": {"cmd": {"type": "string"}, "timeout_ms": {"type": "integer"}},
                       "required": ["cmd"]}}},
]

WORDS = ("the runtime serves a model through a chat template and a tokenizer; a coding agent reads files, searches "
         "the tree, runs a command and continues the conversation with the result. ")


class Failure(Exception):
    pass


def post(url, payload, stream, timeout):
    body = json.dumps(payload).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    if not stream:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    out = []
    chunks = 0
    done = False
    with urllib.request.urlopen(req, timeout=timeout) as r:
        for raw in r:
            line = raw.decode("utf-8", "replace").strip()
            if not line.startswith("data: "):
                continue
            payload_s = line[6:]
            if payload_s == "[DONE]":
                done = True
                break
            chunks += 1
            try:
                out.append(json.loads(payload_s))
            except json.JSONDecodeError as e:
                raise Failure(f"streamed chunk is not JSON: {e}: {payload_s[:120]}")
    if not done:
        raise Failure("the stream ended without [DONE]")
    if chunks == 0:
        raise Failure("the stream carried no chunks")
    return out


def assemble(chunks):
    """content and tool calls from streamed deltas, with the delta indices checked for gaps and repeats."""
    content = []
    calls = {}
    finish = None
    for c in chunks:
        for ch in c.get("choices", []):
            if ch.get("finish_reason"):
                finish = ch["finish_reason"]
            d = ch.get("delta", {})
            if d.get("content"):
                content.append(d["content"])
            for tc in d.get("tool_calls", []) or []:
                i = tc.get("index")
                if i is None:
                    raise Failure("a streamed tool call has no index")
                slot = calls.setdefault(i, {"name": "", "arguments": ""})
                fn = tc.get("function", {})
                if fn.get("name"):
                    slot["name"] += fn["name"]
                if fn.get("arguments"):
                    slot["arguments"] += fn["arguments"]
    return "".join(content), [calls[k] for k in sorted(calls)], finish


def check_tools(calls):
    for i, c in enumerate(calls):
        if not c["name"]:
            raise Failure(f"tool call {i} has no name")
        try:
            args = json.loads(c["arguments"])
        except json.JSONDecodeError as e:
            raise Failure(f"tool call {i} ({c['name']}) arguments are not JSON: {e}: {c['arguments'][:160]}")
        if not isinstance(args, dict):
            raise Failure(f"tool call {i} ({c['name']}) arguments are not an object")


class Session:
    def __init__(self, base, tag, out_dir, seed, verbose):
        self.base = base.rstrip("/")
        self.tag = tag
        self.out = out_dir
        self.seed = seed
        self.verbose = verbose
        self.messages = []
        self.steps = 0
        self.gen_tokens = 0
        self.log = []

    def url(self, path):
        return f"{self.base}{path}"

    def alive(self, where):
        try:
            with urllib.request.urlopen(self.url("/health"), timeout=30) as r:
                if r.status != 200:
                    raise Failure(f"health returned {r.status} after {where}")
        except urllib.error.URLError as e:
            raise Failure(f"the server is not reachable after {where}: {e}")
        if self.server_log_has("FG_SERVER_INVALID_TOKEN"):
            raise Failure(f"the sampler produced an invalid token before {where}")
        if self.server_log_has("failed to add token"):
            raise Failure(f"a batch refused a token before {where}")

    def server_log_path(self):
        return os.path.join(self.out, "server.log")

    def server_log_has(self, needle):
        p = self.server_log_path()
        if not os.path.exists(p):
            return False
        with open(p, "rb") as f:
            return needle.encode() in f.read()

    def refuse(self, where):
        """A request that cannot be served must be refused, and the server must go on serving afterwards.

        This is the recovery property: whatever the reason a request fails - a context that cannot hold the answer, a bad
        id, an exhausted cache - the next request must succeed. It also proves the failure is reported rather than silently
        turning into a short answer.
        """
        payload = {"messages": [{"role": "user", "content": "hi"}], "max_tokens": 400000, "temperature": 0, "stream": False}
        status, detail = 0, ""
        try:
            post(self.url("/v1/chat/completions"), payload, False, 300)
            status = 200
        except urllib.error.HTTPError as e:
            status, detail = e.code, e.read(300).decode(errors="replace")
        except (urllib.error.URLError, socket.timeout) as e:
            self.alive(f"the refused request ({where})")
            raise Failure(f"the refused request was not answered at all: {e}")
        self.alive(f"the refused request ({where})")
        if status == 200:
            raise Failure(f"a request for {400000} tokens was answered instead of refused ({where})")
        self.log.append({"step": f"refuse:{where}", "status": status, "detail": detail[:160]})
        return status

    def hostile(self, where):
        """A request the server must refuse without dying.

        The tokens arrive from a client, and an id outside the vocabulary used to reach the batch builder, which aborts on
        it with "failed to add token". The server already refuses it in server_tokens::validate and answers a request error;
        this asks for that answer and for the process to still be there afterwards.
        """
        body = json.dumps({"prompt": [1, 2, 99999999], "n_predict": 1}).encode()
        req = urllib.request.Request(self.url("/completion"), data=body, method="POST",
                                     headers={"Content-Type": "application/json"})
        status, detail = 0, ""
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                status, detail = r.status, r.read(200).decode(errors="replace")
        except urllib.error.HTTPError as e:
            status, detail = e.code, e.read(200).decode(errors="replace")
        except urllib.error.URLError as e:
            self.alive(f"the hostile request ({where})")
            raise Failure(f"the hostile request could not be sent: {e}")
        self.alive(f"the hostile request ({where})")
        if status == 200:
            raise Failure(f"a request with a token id outside the vocabulary was answered as if it were valid ({where})")
        if status == 500:
            raise Failure(f"a bad token id produced a server error instead of a request error ({where}): {detail[:120]}")
        self.log.append({"step": f"hostile:{where}", "status": status, "detail": detail[:120]})
        return status

    def step(self, user_text, tools=True, max_tokens=256, stream=True, expect_tool=False):
        self.steps += 1
        self.messages.append({"role": "user", "content": user_text})
        payload = {"messages": self.messages, "max_tokens": max_tokens, "temperature": 1,
                   "top_p": 0.95, "top_k": 20, "seed": self.seed, "stream": stream}
        if stream:
            # without this the stream carries no usage and every step reports zero generated tokens
            payload["stream_options"] = {"include_usage": True}
        if tools:
            payload["tools"] = TOOLS
        t0 = time.time()
        try:
            if stream:
                chunks = post(self.url("/v1/chat/completions"), payload, True, 1800)
                content, calls, finish = assemble(chunks)
                usage = {}
                for c in chunks:
                    if c.get("usage"):
                        usage = c["usage"]
            else:
                r = post(self.url("/v1/chat/completions"), payload, False, 1800)
                ch = r["choices"][0]
                content, calls, finish = ch["message"].get("content") or "", \
                    [{"name": t["function"]["name"], "arguments": t["function"]["arguments"]}
                     for t in (ch["message"].get("tool_calls") or [])], ch.get("finish_reason")
                usage = r.get("usage", {})
        except (urllib.error.HTTPError, urllib.error.URLError, socket.timeout) as e:
            raise Failure(f"step {self.steps} request failed: {e}")
        dt = time.time() - t0

        if calls:
            check_tools(calls)
        if expect_tool and not calls:
            raise Failure(f"step {self.steps} expected a tool call and got finish_reason={finish}")
        done = usage.get("completion_tokens") or 0
        self.gen_tokens += done

        self.messages.append({"role": "assistant", "content": content or "",
                              **({"tool_calls": [{"id": f"call_{self.steps}_{i}", "type": "function",
                                                  "function": {"name": c["name"], "arguments": c["arguments"]}}
                                                 for i, c in enumerate(calls)]} if calls else {})})
        for i, c in enumerate(calls):
            self.messages.append({"role": "tool", "tool_call_id": f"call_{self.steps}_{i}",
                                  "content": json.dumps({"ok": True, "result": f"simulated result {i}",
                                                         "path": "/tmp/x", "lines": 42})})

        prompt_tokens = usage.get("prompt_tokens") or 0
        self.prompt_tokens_max = max(getattr(self, "prompt_tokens_max", 0), prompt_tokens)
        rec = {"step": self.steps, "stream": stream, "seconds": round(dt, 2), "finish": finish,
               "content_len": len(content), "tool_calls": [c["name"] for c in calls],
               "completion_tokens": done, "prompt_tokens": prompt_tokens, "messages": len(self.messages)}
        self.log.append(rec)
        if self.verbose:
            print(f"  step {self.steps:2d} {dt:6.1f}s finish={finish} content={len(content):5d} "
                  f"tools={rec['tool_calls']} gen={done}")
        self.alive(f"step {self.steps}")
        return rec

    def pad(self, tokens):
        """Grow the conversation with a synthetic tool result of roughly the requested token size."""
        need = tokens - sum(len(m.get("content") or "") for m in self.messages) // 4
        if need <= 0:
            return
        self.messages.append({"role": "tool", "tool_call_id": "pad", "content": (WORDS * (need // 40 + 1))[:need * 4]})

    def goose_activity(self):
        """Which ported paths actually answered. A missing marker is a silent fallback, which is a failure, not a note."""
        found = {}
        for name, (needle, why) in GOOSE_MARKERS.items():
            found[name] = self.server_log_has(needle)
        return found


def run(mode, args):
    # The runner creates the directory and the server log; using a second timestamp here put the log and the test in
    # different directories and every marker check read an empty file (measured on the first run).
    if args.out:
        out = args.out
    else:
        out = os.path.join(args.results, f"{args.tag}-{time.strftime('%Y%m%d-%H%M%S')}")
    os.makedirs(out, exist_ok=True)
    stamp = os.path.basename(out)
    shutil_copy = None
    S = Session(args.base, args.tag, out, args.seed, args.verbose)
    verdict = "PASS"
    reason = ""
    try:
        with urllib.request.urlopen(S.url("/health"), timeout=30) as r:
            if r.status != 200:
                raise Failure("the server is not healthy before the run")

        if mode == "FAST":
            S.step("Привет. Ты тут?", max_tokens=64)
            S.step("Прочитай файл /tmp/x и скажи, сколько там строк.", max_tokens=128, expect_tool=True)
            S.step("Продолжи.", max_tokens=64)
        elif mode in ("CHECK", "FULL"):
            big = 20000 if mode == "CHECK" else 32000
            long_gen = 512 if mode == "CHECK" else 3000
            S.step("Привет. Ты тут?", max_tokens=32)
            S.step("Прочитай /home/end0/AI/FrankenMoE-CUDA/AGENTS.md и перескажи правила.", max_tokens=256,
                   expect_tool=True)
            S.step("Найди в дереве все места, где вызывается fg_sampler_top_p_call.", max_tokens=256, expect_tool=True)
            S.step("Запусти git status и объясни вывод.", max_tokens=256, expect_tool=True)
            S.pad(big // 2)
            S.step("Продолжи работу: какие шаги остались по миграции?", max_tokens=256)
            S.pad(big)
            S.step("Суммируй всё сказанное выше одним абзацем.", max_tokens=256)
            S.step("Напиши длинный разбор архитектуры рантайма.", max_tokens=long_gen, stream=True)
            # the padding is only a means: what matters is how many prompt tokens the server reports, because that is
            # the context the request actually carried
            need_prompt = 18000 if mode == "CHECK" else 25000
            got_prompt = S.log[-1].get("prompt_tokens", 0)
            if got_prompt < need_prompt:
                raise Failure(f"{mode} reached {got_prompt} prompt tokens, below the {need_prompt} this mode exists for")
            S.step("Проверь, что предыдущий ответ не оборвался, и продолжи с того места.", max_tokens=256)
        elif mode == "REPRO":
            # the shape that failed: several requests building context, then one long generation
            S.step("Привет. Ты тут?", max_tokens=64)
            S.step("Прочитай /home/end0/AI/FrankenMoE-CUDA/AGENTS.md.", max_tokens=256, expect_tool=True)
            S.pad(21000)
            S.step("Продолжи работу по плану миграции и не останавливайся.", max_tokens=3383, stream=True)

        if mode in ("CHECK", "FULL", "REPRO"):
            S.hostile("after the session")
            S.step("Ещё один короткий ответ после ошибочного запроса.", max_tokens=64)
            S.refuse("after the session")
            S.step("И ещё один ответ после отказа.", max_tokens=64)

        found = S.goose_activity()
        required = ["tokenizer_from_port", "sampler_stage"]
        missing = [n for n in required if not found[n]]
        if missing:
            raise Failure(f"the shipped path did not answer from Goose for: {', '.join(missing)} - "
                          f"a silent fallback to C++ is a failure, not a note")
    except Failure as e:
        verdict, reason = "FAIL", str(e)
    except Exception as e:                                    # noqa: BLE001 - any surprise is a failure
        verdict, reason = "FAIL", f"{type(e).__name__}: {e}"

    summary = {
        "tag": args.tag, "mode": mode, "verdict": verdict, "reason": reason,
        "git_sha": subprocess.run(["git", "-C", args.repo, "rev-parse", "HEAD"],
                                  capture_output=True, text=True).stdout.strip(),
        "base": args.base, "seed": args.seed, "steps": S.steps, "generated_tokens": S.gen_tokens,
        "goose_paths": S.goose_activity() if S else {},
        "step_log": S.log,
        "started": stamp,
    }
    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump(summary, f, ensure_ascii=False, indent=1)
    print(f"{verdict}: {mode} - {S.steps} steps, {S.gen_tokens} generated tokens")
    if reason:
        print(f"  reason: {reason}")
    print(f"  artefacts: {out}")
    return 0 if verdict == "PASS" else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["FAST", "CHECK", "FULL", "REPRO"])
    ap.add_argument("--base", default="http://127.0.0.1:9101")
    ap.add_argument("--tag", default="awt")
    ap.add_argument("--seed", type=int, default=20261119)
    ap.add_argument("--results", default="results/awt")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out", default="")
    ap.add_argument("--verbose", action="store_true", default=True)
    args = ap.parse_args()
    return run(args.mode, args)


if __name__ == "__main__":
    sys.exit(main())
