// P8-09 S3: the embedded asset identity, i.e. the ETag the server hands out.
//
// Native source: upstream/llama.cpp/tools/ui/embed.cpp at base commit
// ec50a32d:
//
//   fnv_hash            FNV-1a over the asset bytes                     47-56
//   the etag literal    "\"0x%016" PRIx64 "\"" of every asset         265-267
//
// embed.cpp writes the hash into ui.cpp as a string literal, and server-http.cpp
// copies that literal into the ETag header (server-http.cpp:370). So the bytes
// of this string are observable on the wire: a browser sends them back in
// If-None-Match and the server compares them byte for byte. Porting the hash and
// the formatting together is what keeps a re-generated ui.cpp byte identical to
// the accepted build.
//
// Native details that are part of the identity:
//   - FNV-1a 64: offset basis 0xcbf29ce484222325, prime 0x100000001b3, xor then
//     multiply per byte, unsigned 64-bit wrap at every step;
//   - the literal is exactly 20 bytes: '"', '0', 'x', 16 lower-case hex digits
//     (zero padded), '"';
//   - the hex is lower case because PRIx64 is; PRIX64 would produce a different
//     ETag for the same bytes and silently invalidate every cached asset;
//   - an empty asset is never hashed by native: embed.cpp refuses empty files,
//     so hash(empty) is the offset basis and has no server-side use.
//
// The prefix is et_. Not owned: reading files, sorting names, writing ui.cpp,
// the request-side 304 compare (S2).

import std;

const ET_OFFSET: u64 = 0xcbf29ce484222325;
const ET_PRIME: u64 = 0x100000001b3;

// FNV-1a over the bytes; out[0] receives the hash bit pattern as i64.
export fn et_hash(b: const u8[:], out: i64[:]) -> i64 {
    var hash: u64 = ET_OFFSET;
    var i: i64 = 0;

    while i < b.len {
        hash = (hash ^ b[i]) * ET_PRIME;
        i = i + 1;
    }
    out[0] = hash as i64;
    return 0;
}

fn et_hex(d: i64) -> u8 {
    if d < 10 {
        return (48 + d) as u8;
    }
    return (97 + (d - 10)) as u8;
}

// The ETag literal of one hash: '\"0x' + 16 lower-case hex + '\"'.
// Returns the number of bytes written (20).
export fn et_text(h: i64, out: u8[:]) -> i64 {
    var p: i64 = 0;

    out[p] = 34;
    p = p + 1;
    out[p] = 48;
    p = p + 1;
    out[p] = 120;
    p = p + 1;
    var shift: i64 = 60;

    while shift >= 0 {
        out[p] = et_hex((h >> shift) & 15);
        p = p + 1;
        shift = shift - 4;
    }
    out[p] = 34;
    p = p + 1;
    return p;
}
