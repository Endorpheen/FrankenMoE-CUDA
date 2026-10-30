"""Read curl traces offline; never send requests or launch a model."""

import re
from pathlib import Path


def first_token_seconds(path):
    """Measure send-header to receipt of a nonempty generated-token array.

    HTTP headers, progress events and empty content events do not count as a
    generated token. Accumulation handles a token field split across receives.
    Native completion writes one JSON SSE event per HTTP chunk, so chunk
    framing cannot split the token array within that event.
    """
    begin = first = stamp = None
    receive = False
    payload = ""
    for line in Path(path).read_text().splitlines():
        timing = re.match(r"(\d\d):(\d\d):(\d\d)\.(\d{6}) ", line)
        if timing:
            hour, minute, second, micro = map(int, timing.groups())
            stamp = hour * 3600 + minute * 60 + second + micro / 1e6
            if "=> Send header" in line and begin is None:
                begin = stamp
            receive = "<= Recv data" in line
            if not receive:
                payload = ""
        elif receive:
            part = re.match(r"[0-9a-f]+: (.*)", line)
            if part:
                payload += part.group(1)
                if re.search(r'"tokens"\s*:\s*\[\s*\d+', payload):
                    first = stamp
                    break
    if begin is None or first is None:
        raise ValueError(f"{path}: missing request start or generated token")
    elapsed = first - begin
    if elapsed < 0:
        elapsed += 86400
    if not 0 < elapsed < 600:
        raise ValueError(f"{path}: invalid TTFT {elapsed}")
    return elapsed
