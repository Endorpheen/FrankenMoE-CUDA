# Blockers for the Goose compiler, derived from nine real ports

Rule 27 sets the target: the necessary runtime is Goose, no C++ source and no C++ runtime. These are the specific
capabilities the nine ports of 2026-11-16 needed and did not find. Each one is recorded with the port that hit it, so
the compiler work can be judged by whether the port would then be expressible - not by whether the feature sounds
useful.

## 1. No dynamic memory, so every buffer is owned by the caller

Hit by: every port. `unicode-han`, `unicode-newlines`, `unicode-byte-map`, `unicode-afmoe`, `unicode-gpt2`,
`unicode-qwen35` all receive a preallocated `out` pointer and a `cap`, and refuse to write past it.

Consequence: the module cannot produce a result whose size it does not already know. The C++ host must guess an upper
bound (`cpts.size() + offsets.size() + 8`) and the module must degrade rather than grow. A tokeniser that wants to
return a variable number of spans cannot do it on its own.

Needed: an allocator Goose can call, or a two-pass protocol (size, then fill) built into the language rather than
written by hand in every module.

## 2. No callback, so classification is passed as a bare pointer

Hit by: `unicode-afmoe`, `unicode-gpt2`, `unicode-qwen35`.

The native splitters ask a lambda "what are the flags of position p". Goose cannot be handed a function, so the
bridge passes `fg_unicode_flags_view_c()` - a pointer to a global array - and the module indexes it by codepoint.
That works only because the table happens to be dense and global. A classifier that is not a dense global array
cannot be passed this way at all.

Needed: either first-class function values, or a documented data-table convention the compiler can enforce.

## 3. No string type, so every port is codepoint arrays in, offsets out

Hit by: `unicode-newlines`, `unicode-gpt2`, `unicode-qwen35`, `unicode-hangul`.

The C++ functions take `const std::string &` and return `std::vector<size_t>`. The modules take `const uint32_t *` and
write lengths. The conversion lives in the C++ host (`unicode_cpts_from_utf8`, `std::vector`). So the "ported" module
is a decision function embedded in a C++ host - exactly what rule 27 says is not complete.

Needed: a Goose string/buffer type with UTF-8 in and span list out, and the iteration over it in Goose.

## 4. No tables, so Unicode data still lives in C++

Hit by: `unicode-hangul` (the algorithmic half ported; the table half did not move), and by the unported
`unicode_canonical_decompose`, `unicode_cpts_normalize_nfc`, `unicode_cpt_flags_array`.

`unicode_decomposition` is a `std::unordered_map` in C++. A Goose module cannot reach it, and constructing an
equivalent in Goose needs both a table representation and (per blocker 1) a place to put it.

Needed: static data in Goose, generated from `UnicodeData.txt`, addressable without a C++ container.

## 5. Integer widths are not enforced at the boundary

Hit by: `unicode-qwen35` and `unicode-afmoe`, both of which read 64-bit packet fields through a 32-bit accessor and
truncated a pointer. Both were caught by their gates, not by the compiler.

`goose check` accepts `ld_u32` where `ld_i64` was intended. A pointer-sized load cannot be expressed as "pointer
sized", so the mistake has to be caught by reading carefully.

Needed: a pointer/integer-size type, or a `usize`-style load that cannot silently truncate.

## What this means for the next step

Blockers 1 to 4 together are the reason no module can yet be called complete under rule 27: each of them forces a C++
host even when the decision itself moved. The order to attack them is 3 (string/buffer type), then 1 (allocation),
then 4 (static tables); 2 and 5 are smaller but shape the API.

Until at least 1 and 3 exist, porting more decision functions adds gates and adds no independence - which is what the
nine ports of today did, and the count of "ported modules" must not be read as progress toward the criterion.
