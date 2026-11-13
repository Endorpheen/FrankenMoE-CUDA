# P7-01 closure: GGUF reader on the Apex scope

Criterion: "Every GGUF version/value type, alignment/endian/tensor descriptor and
accessor roundtrip; truncated, overflow, malformed and multi-shard corpus
rejected safely."

## Implemented and gated

EXP443, Goose `source/gguf.goose`:

- header magic/version/tensor-count/kv-count for GGUF v2/v3;
- metadata key length/hash/type; value traversal for every scalar type
  (uint8/int8, uint16/int16, uint32/int32/float32, bool, uint64/int64/float64),
  strings and arrays;
- typed lookup returning `type, value offset, length/count, element type` for
  string, scalar and array values;
- tensor descriptors: name length/hash, n_dims, four dimensions, GGML type,
  data offset;
- bounds checks on every read.

## Gate

Apex GGUF: 21 713 463 136 bytes; structural region ends at 10 990 919 bytes.

| check | result |
|---|---|
| structural records | 7 040 words, **exact match** with an independent Python parser |
| typed lookups | exact match for `general.architecture` (string), `llama.context_length` (uint32), `tokenizer.ggml.model` (string), `tokenizer.ggml.tokens` (string array), `tokenizer.ggml.scores` (float32 array), `general.alignment` |
| value hashes | FNV-1a over the value bytes/array elements, equal to Python |
| truncations below the structural end | **14/14 rejected** |
| corrupt magic | rejected |
| corrupt tensor count | rejected |

## Scope

Closed on the Apex GGUF: v3, little-endian, single shard. Explicitly remaining
and assigned to P7-02/P7-12/C2:

- GGUF v1 and non-little-endian files;
- multi-shard files and shard naming/offset merge;
- GGUF writing/splitting/serialization (P7-02);
- tensor-data alignment beyond the descriptor (the loader still owns the data
  mapping);
- accessors beyond the typed classes exercised here.

No claim is made that the universal GGUF writer/reader is complete.

## Verdict

P7-01 closes on the Apex reader scope: header, metadata, typed accessors, tensor
descriptors and malformed-input rejection are Goose and match an independent
parser. The remaining versions/shards/writer are named compatibility work.
