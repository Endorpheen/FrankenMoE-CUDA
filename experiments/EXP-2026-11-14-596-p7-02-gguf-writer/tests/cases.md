# Gate cases — P7-02 S2 GGUF writer

Every case is applied twice: native `gguf_set_*` on a fresh `gguf_context`
(`ggml/src/gguf.cpp`) and the Goose port (`s1_*`). The fixture then compares

1. the canonical KV dump of both contexts (name, type, array flag, element
   count, values as hex), and
2. the serialized bytes for `only_meta=true` and `only_meta=false`.

Sizes are compared before bytes; on mismatch the fixture prints the first
differing offset and 16 bytes around it from both sides.

## Layout expectations

```
magic "GGUF" | u32 version | u64 n_tensors | u64 n_kv
KV* | tensor info* | zero padding to alignment
```

* magic bytes `47 47 55 46`;
* version `3` (little-endian u32);
* `n_tensors` is 0 in this slice (tensor descriptors and the data section are
  S3), `n_kv` is `st[0]`;
* KV key: `u64` byte length followed by the raw key bytes;
* scalar: `u32` type id, then the value; strings are `u64` length + bytes;
* array: `u32` type id 9, `u32` element type, `u64` element count, then the
  payload: raw element bytes for numeric/bool arrays, and one `u64` length +
  bytes per element for string arrays;
* padding after the KV block: zero bytes until the length is a multiple of the
  alignment (32 native default, 64/1 covered through reload);
* all integers little-endian.

## Cases

| Case | Operations | What it pins |
|------|------------|--------------|
| `empty` | none | header only (24 bytes) + padding to 32 |
| `u8`, `i8`, `u16`, `i16`, `u32`, `i32` | one scalar | type id and 1/2/4-byte encoding |
| `u64`, `i64` | one scalar | 8-byte encoding, high bits and negative values |
| `f32`, `f64` | one scalar | IEEE-754 bit image (`1.5f`, `-3.25`) |
| `bool` | one scalar | one byte, `1` |
| `str` | `"hello"` | scalar string prefix |
| `str_empty` | `""` | zero-length string prefix |
| `arr_u8` | `{1,2,255}` | 1-byte numeric array |
| `arr_i32` | `{-1,0,7}` | 4-byte array, element count vs byte length |
| `arr_f32` | `{0.5,-1.5}` | float array payload |
| `arr_bool` | `{1,0,1}` | bool array written as one byte per element |
| `arr_str` | `{"a","bc",""}` | NUL-separated blob, empty element, per-element prefix |
| `arr_str_empty` | count 0 | array header with no payload |
| `arr_u8_empty` | count 0 | numeric array header with no payload |
| `replace_reserved` | `k=1`, `k=2`, `general.alignment=64` | replacement moves to the end; the alignment KV is data, not writer state |
| `remove` | set a,b,c; remove b; remove zz | removal keeps list order; absent key is a no-op |
| `set_kv` | copy u16, str, u32 array, string array over a duplicate | full copy path feeds the writer |
| `long_str` | 40-byte key, 300-byte value | multi-byte `u64` length prefixes |
| `long_arr_str` | 40-byte key, 200/1/0-byte elements | long string array elements |
| `utf8` | Cyrillic value with `/模型`, emoji key | byte lengths, not code points |
| `mixed` | 7 KVs in a mixed order | KV order and type interleaving |
| `align64_mixed` | 4 KVs + `general.alignment=64` | padding 64, reload path |
| `align64_kv_only` | only `general.alignment=64` | padding 64 on a minimal body |
| `align1_mixed` | 2 KVs + `general.alignment=1` | no padding at all |

## Reload path for non-default alignment

`gguf_init_empty()` always leaves `ctx->alignment = 32`; the public API can only
change it by reading a file that carries `general.alignment`. For the three
alignment cases the fixture therefore writes the port bytes to a temporary file,
loads them with `gguf_init_from_file` (metadata only), asserts
`gguf_get_alignment()`, compares the KV dump of the loaded context, and
re-serializes with the native writer. The native output is then byte-compared
with the port output. A wrong pad length or pad value in the port changes the
re-serialized native bytes and fails the case.

## Totals

31 cases, 161 checks, 0 failures: 62 byte comparisons (56 direct
`only_meta` pairs + 6 reload), 37 KV dumps, 6 native alignment assertions,
56 port `only_meta` invariance checks.
