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
stream_params = "observer: i64[:], stream_text: u8[:], stream_response: u8[:]"
wrapper_call = ("    let result = q35_sample_generate_impl(tokens, vocabulary, capacity, eog, limit, s, ids, timing, "
                "top, prob_ids, logp, pipeline);")
if text.count(wrapper_call) == 1:
    # Checkpoints from EXP-1724 on split the sampler loop into an implementation
    # and a scope wrapper: both take the stream observer, the wrapper forwards it.
    text = text.replace("logp: f32[:], pipeline: bool) -> i64 {",
                        f"logp: f32[:], pipeline: bool, {stream_params}) -> i64 {{")
    text = replace_once(text, wrapper_call, wrapper_call.replace(
        "logp, pipeline);", "logp, pipeline, observer, stream_text, stream_response);"))
else:
    text = replace_once(
        text,
        "logp: f32[:], pipeline: bool) -> i64 {",
        f"logp: f32[:], pipeline: bool, {stream_params}) -> i64 {{",
    )
text = replace_once(
    text,
    "    var batch_end: i64 = 0;\n    while read < tokens.len {",
    "    var batch_end: i64 = 0;\n    var last_prefill_log = start;\n    while read < tokens.len {",
)
text = replace_once(
    text,
    """        if q35_generation_eval(batch[0 .. count], output, true) != output.len {
            q35_generation_reset();
            return -1;
        }""",
    """        if q35_generation_eval(batch[0 .. count], output, true) != output.len {
            q35_generation_reset();
            return -1;
        }
        if observer.len == 10 && read < tokens.len {
            let now = q35_generation_now();
            if now - last_prefill_log >= 3.0 {
                ih_publish_prefill(read, tokens.len, now - start, observer, stream_response);
                last_prefill_log = now;
            }
        }""",
)
text = replace_once(
    text,
    "            if produced == 0 { prefill_done = q35_generation_now(); }",
    """            if produced == 0 {
                prefill_done = q35_generation_now();
                if observer.len == 10 {
                    ih_publish_prefill(tokens.len, tokens.len, prefill_done - start, observer, stream_response);
                }
            }""",
)
text = replace_once(
    text,
    "        ids[produced] = next;\n        produced += 1;\n        if last {",
    """        ids[produced] = next;
        produced += 1;
        if observer.len == 10 && !is_eog && (produced == 8 || produced % 16 == 0) && !last {
            ih_chat_stream_observe(ids[0 .. produced], tokens.len, start, prefill_done, observer, stream_text, stream_response);
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
