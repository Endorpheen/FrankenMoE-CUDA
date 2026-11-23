"""Check Apex chat accounting and tool continuation against an isolated server."""
import argparse
import copy
import json
from urllib.request import Request, urlopen


def chat(url, messages, *, stream=False):
    body = {
        "model": "Apex-35B-V1.0",
        "messages": messages,
        "max_tokens": 96,
        "temperature": 0,
        "seed": 42,
        "stream": stream,
    }
    if stream:
        body["stream_options"] = {"include_usage": True}
    request = Request(
        url + "/v1/chat/completions",
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"},
    )
    with urlopen(request, timeout=60) as response:
        if not stream:
            packet = json.load(response)
            return packet["usage"], packet["choices"][0]["message"].get("reasoning_content", "")
        final = None
        reasoning = []
        done = False
        for line in response:
            if not line.startswith(b"data: "):
                continue
            data = line[6:].strip()
            if data == b"[DONE]":
                done = True
                break
            packet = json.loads(data)
            for choice in packet["choices"]:
                reasoning.append(choice["delta"].get("reasoning_content", ""))
            if "usage" in packet:
                assert final is None, "multiple final usage records"
                final = packet["usage"]
        assert done and final is not None, "missing SSE completion or usage"
        return final, "".join(reasoning)


def check_usage(usage, reasoning):
    count = usage["completion_tokens"]
    detail = usage["completion_tokens_details"]["reasoning_tokens"]
    assert usage["total_tokens"] == usage["prompt_tokens"] + count, usage
    assert 0 <= detail <= count <= 96, usage
    if reasoning.strip():
        assert detail > 0, "reasoning text arrived without reasoning token usage"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, type=int)
    url = f"http://127.0.0.1:{parser.parse_args().port}"
    history = [
        {"role": "user", "content": "Read the tool result and answer with the number."},
        {"role": "assistant", "content": None, "tool_calls": [{
            "id": "call_fixture", "type": "function", "function": {
                "name": "read_result", "arguments": "{}",
            },
        }]},
        {"role": "tool", "tool_call_id": "call_fixture", "content": "The number is 42."},
    ]
    retained = copy.deepcopy(history)
    retained[1]["reasoning_content"] = "Remember the result from this fixture. " * 15

    control, control_reasoning = chat(url, history)
    active, active_reasoning = chat(url, retained)
    check_usage(control, control_reasoning)
    check_usage(active, active_reasoning)
    assert active["prompt_tokens"] > control["prompt_tokens"] + 15, (
        "current-turn reasoning did not reach the model prompt", control, active,
    )

    new_turn = [{"role": "user", "content": "New question: what is 17 times 19?"}]
    old_control, old_reasoning = chat(url, history + new_turn)
    old_active, old_active_reasoning = chat(url, retained + new_turn)
    check_usage(old_control, old_reasoning)
    check_usage(old_active, old_active_reasoning)
    assert old_control["prompt_tokens"] == old_active["prompt_tokens"], (
        "reasoning from an old turn leaked into the prompt", old_control, old_active,
    )

    streamed, streamed_reasoning = chat(url, retained, stream=True)
    check_usage(streamed, streamed_reasoning)
    assert streamed == active, ("streaming and JSON usage differ", streamed, active)
    assert any(value.strip() for value in (control_reasoning, active_reasoning, streamed_reasoning)), (
        "the fixture did not exercise reasoning output"
    )
    print(f"Reasoning replay: {control['prompt_tokens']} -> {active['prompt_tokens']} prompt tokens")
    print(f"New user turn: {old_control['prompt_tokens']} prompt tokens in both cases")
    print("JSON and SSE usage:", streamed)


if __name__ == "__main__":
    main()
