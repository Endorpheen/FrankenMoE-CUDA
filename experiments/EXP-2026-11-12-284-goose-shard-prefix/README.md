# EXP284: multi-shard file-name rule in Goose

Part of P7-03 (model loader, mmap and file I/O ownership). One pure rule from `src/llama.cpp`,
`llama_split_prefix()`: given a shard path, an index and a shard count, decide the prefix length when the path ends
with the expected `-NNNNN-of-MMMMM.gguf` postfix.

## The rule

- the expected postfix is `-%05d-of-%05d.gguf` with `(split_no + 1, split_count)`, where `%05d` pads to five digits but
  prints more when the number needs them;
- no match when the path is not longer than the postfix;
- no match when the suffix differs;
- otherwise the prefix length is `path_len - postfix_len`.

The module returns the length or `-1`; it never copies, so the caller keeps its buffer.

## Gate

    match 2 of 4 / match 1 of 4                    OK (5)
    wrong count / wrong index                      OK (-1)
    no postfix / too short                         OK (-1)
    directories in path                            OK (10)
    six-digit count / six-digit index              OK (1)

Nine checks against a `snprintf` transcription, zero failures. Six-digit cases matter: `%05d` prints six digits for
100000, so the postfix length is not a constant, and a port that hardcodes twenty bytes fails them.

The gate was shown to fail: changing the padding width from five to four digits breaks four cases (9/4). Restored, 9/0.
