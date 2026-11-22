"""Compose the Alpha HTTP host with the pinned Goose inference checkpoint."""

from pathlib import Path
import sys


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise SystemExit(f"Unexpected checkpoint source around {before.splitlines()[0]!r}")
    return text.replace(before, after, 1)


source, http, output = map(Path, sys.argv[1:])
text = source.read_text()
marker = "// ---- composed from goose/server/inference-http.goose ----\n"
if text.count(marker) != 1:
    raise SystemExit("Expected exactly one standalone inference HTTP module")
start = text.index(marker) + len(marker)
end = text.find("// ---- composed from ", start)
if end < 0:
    end = len(text)
text = text[:start] + http.read_text() + "\n" + text[end:]
text = replace_once(
    text,
    "logp: f32[:], pipeline: bool) -> i64 {",
    "logp: f32[:], pipeline: bool, observer: i64[:], stream_text: u8[:], stream_response: u8[:]) -> i64 {",
)
text = replace_once(
    text,
    "        ids[produced] = next;\n        produced += 1;\n        if last {",
    """        ids[produced] = next;
        produced += 1;
        if observer.len == 8 && !is_eog && (produced == 8 || produced % 16 == 0) && !last {
            ih_chat_stream_observe(ids[0 .. produced], observer, stream_text, stream_response);
        }
        if last {""",
)
text = replace_once(
    text,
    "    return q35_sample_generate(tokens, vocabulary, capacity, eog, limit, s, ids, timing,\n"
    "        0, no_ids, no_probs, false);",
    """    var observer: i64[0] = [];
    var stream_text: u8[0] = [];
    var stream_response: u8[0] = [];
    return q35_sample_generate(tokens, vocabulary, capacity, eog, limit, s, ids, timing,
        0, no_ids, no_probs, false, observer[..], stream_text[..], stream_response[..]);""",
)
output.write_text(text)
