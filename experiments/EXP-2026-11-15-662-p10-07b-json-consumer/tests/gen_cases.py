#!/usr/bin/env python3
"""Write the P10-07 differential corpus: 20 HuggingFace /api/models bodies.

Fields: name, quant argument, what the case pins. Every body is written as
bytes so escapes, duplicate keys and a trailing-garbage case stay literal.
"""
from pathlib import Path

CASES = Path(__file__).resolve().parent / "cases"

# name, quant, note, body
DOCS = [
    ("01-basic", "Q4_K_M", "plain siblings, mixed extensions",
     '{"id":"org/model","siblings":[{"rfilename":"README.md"},'
     '{"rfilename":"model-Q4_K_M.gguf"},{"rfilename":"model-F16.gguf"},'
     '{"rfilename":"model.safetensors"}]}'),

    ("02-split", "Q8_0", "split shards plus mmproj; prefix extraction",
     '{"siblings":[{"rfilename":"model-Q8_0-00001-of-00003.gguf"},'
     '{"rfilename":"model-Q8_0-00002-of-00003.gguf"},'
     '{"rfilename":"model-Q8_0-00003-of-00003.gguf"},'
     '{"rfilename":"mmproj-Q8_0.gguf"}]}'),

    ("03-no-siblings", "Q4_K_M", "no siblings key -> format error",
     '{"id":"org/model","tags":["text-generation"]}'),

    ("04-siblings-object", "Q4_K_M", "siblings is an object, not an array",
     '{"siblings":{"a":{"rfilename":"model-Q4_K_M.gguf"}}}'),

    ("05-empty-siblings", "Q4_K_M", "empty array -> no match",
     '{"siblings":[]}'),

    ("06-no-match", "Q4_K_M", "only an F16 file -> no match",
     '{"siblings":[{"rfilename":"model-F16.gguf"}]}'),

    ("07-escapes", "Q4_K_M", "escaped dot, dash, quote, backslash, newline, latin-1",
     '{"siblings":[{"rfilename":"model\\u002dQ4_K_M.gguf"},'
     '{"rfilename":"weird\\"name-Q4_K_M.gguf"},'
     '{"rfilename":"back\\\\slash-Q4_K_M.gguf"},'
     '{"rfilename":"line\\nbreak-Q4_K_M.gguf"},'
     '{"rfilename":"caf\\u00e9-Q4_K_M.gguf"}]}'),

    ("08-unicode", "Q4_K_M", "raw UTF-8 file names",
     '{"siblings":[{"rfilename":"\u043c\u043e\u0434\u0435\u043b\u044c-Q4_K_M.gguf"},'
     '{"rfilename":"\u6a21\u578b-Q4_K_M.gguf"}]}'),

    ("09-dupkeys", "Q4_K_M", "duplicate siblings key: last value wins",
     '{"siblings":[{"rfilename":"old.gguf"}],'
     '"siblings":[{"rfilename":"new-Q4_K_M.gguf"}]}'),

    ("10-order", "q4_k_m", "lowercase quant, keys out of order",
     '{"license":"apache-2.0","siblings":[{"rfilename":"z-Q4_K_M.gguf"},'
     '{"rfilename":"a-Q4_K_M.gguf"}],"id":"org/model","downloads":7}'),

    ("11-numbers", "Q4_K_M", "u64 max, negatives, floats with exponents",
     '{"downloads":18446744073709551615,"likes":-3,"score":1.5e-3,'
     '"n":0,"siblings":[{"rfilename":"model-Q4_K_M.gguf"}]}'),

    ("12-nested", "Q4_K_M", "deeply nested extra data next to the file list",
     '{"siblings":[{"rfilename":"model-Q4_K_M.gguf","lfs":null,'
     '"meta":{"a":{"b":[1,2,{"c":["x",[true,false,null]]}]}}},'
     '{"rfilename":"other.gguf","meta":{"a":{"b":[]}}}]}'),

    ("13-bad-json", "Q4_K_M", "truncated object -> parse error",
     '{"siblings":[{"rfilename":"model-Q4_K_M.gguf"}'),

    ("14-trailing", "Q4_K_M", "trailing garbage after a valid object",
     '{"siblings":[{"rfilename":"model-Q4_K_M.gguf"}]} x'),

    ("15-empty-body", "Q4_K_M", "empty input -> parse error",
     ''),

    ("16-rfilename-number", "Q4_K_M", "rfilename is a number -> type error",
     '{"siblings":[{"rfilename":42}]}'),

    ("17-root-array", "Q4_K_M", "root is an array -> format error",
     '[{"rfilename":"model-Q4_K_M.gguf"}]'),

    ("18-last-match", "Q4_K_M", "the only match is the last array element",
     '{"siblings":[{"rfilename":"a.txt"},{"rfilename":"b.txt"},'
     '{"rfilename":"z-Q4_K_M.gguf"}]}'),

    ("19-tabs", "Q4_K_M", "pretty-printed with tabs, CRLF and spaces",
     '{\r\n\t"siblings" : [\r\n\t\t{ "rfilename" : "model-Q4_K_M.gguf" },\r\n'
     '\t\t{ "rfilename" : "mmproj-Q4_K_M.gguf" }\r\n\t]\r\n}\r\n'),

    ("20-nul-escape", "Q4_K_M", "\\u0000 inside a file name",
     '{"siblings":[{"rfilename":"model-Q4_K_M.gguf\\u0000tail"}]}'),
]


def main() -> None:
    CASES.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, quant, note, body in DOCS:
        (CASES / (name + ".json")).write_bytes(body.encode("utf-8"))
        rows.append(f"{name}\t{quant}\t{note}")
    (CASES.parent / "cases.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")
    print(f"wrote {len(DOCS)} cases")


if __name__ == "__main__":
    main()
