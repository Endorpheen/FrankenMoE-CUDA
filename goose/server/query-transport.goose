// P8-09 S5: percent decoding of path components and the query transport.
//
// Native source: upstream/llama.cpp/tools/server/server-http.cpp at base commit
// 050354e9, plus the vendored cpp-httplib code those helpers delegate to:
//
//   decode_path_component      488-506   %XX in a path param, local hex table
//   get_params                 512-525   query params, then path params, both
//                                        assigned into a std::map
//   build_query_string         531-541   the same params re-encoded
//   httplib::detail::parse_query_text    vendored split/trim/dedupe/divide
//   httplib::detail::decode_query_component  vendored %XX plus form '+'
//   httplib::encode_query_component          vendored RFC 3986 encoder
//
// The module owns four rules that native code spells out and nothing else:
//
//   - a path component decodes %XX for any two following hex digits, lower or
//     upper case; an invalid pair leaves the '%' in place and decoding resumes
//     at the next byte, so "%zz" is "%zz" and "%4" keeps its lone '%'
//     (the native test is `i + 2 < size`, i.e. two bytes must follow); a %00
//     decodes to a real NUL inside the value;
//   - a query text splits on '&', each raw token is trimmed of spaces and tabs,
//     empty tokens are dropped, an exactly repeated raw token is dropped as
//     well (httplib keeps a std::set of the raw tokens for one parse call), the
//     token divides at its first '=', keys are not re-trimmed, and a token with
//     an empty key is cached but not entered;
//   - each key and value then decodes as a form component: %XX as above and
//     '+' as a space; the value side never invents an '=' and a bare "flag"
//     keeps an empty value;
//   - the params map that get_params builds is a std::map: keys sorted by
//     unsigned byte order, the last assignment per key wins, path params are
//     assigned after query params so a path param always overrides the query
//     one, and a path param value is decoded with the path rule, not the form
//     rule.
//
// build_query_string iterates the httplib Params container, which keeps
// insertion order, so its output is the parsed pair list re-encoded in the same
// order, with duplicates that survived the parse still repeated. The encoder is
// the vendored one: ASCII alphanumerics and "-._~" pass through, ' ' becomes
// '+', '+' becomes "%2B", and "!$'()*,;:@/?" pass through unchanged; everything
// else, high bytes included, becomes an upper-case %XX triplet.
//
// The module never allocates: keys, values and the raw token offsets live in
// caller-owned buffers. Tables use "packed blob plus count + 1 offsets", the
// layout goose/server/web-assets.goose already uses, so a caller cannot read a
// length that disagrees with its own offsets.
//
// The prefix is qry_. Not owned: the HTTP request/response objects, the header
// and body transport, the route table that produces path params, and the
// decision to parse a form body after the URL query (the caller re-enters
// qry_parse with the pair count left in st[0], which is also how httplib ends up
// with URL params before body params in one Params container).

import std;

// out[at ..] = s, and -1 when the caller buffer cannot hold the text.
fn qry_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    if at < 0 || at + s.len > out.len {
        return 0 - 1;
    }
    var i: i64 = 0;

    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

// The value of one hex digit, -1 when it is not one.
fn qry_hexval(c: u8) -> i64 {
    if c >= 48 && c <= 57 {
        return c - 48;
    }
    if c >= 97 && c <= 102 {
        return c - 87;
    }
    if c >= 65 && c <= 70 {
        return c - 55;
    }
    return 0 - 1;
}

fn qry_is_space_or_tab(c: u8) -> bool {
    return c == 32 || c == 9;
}

fn qry_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// Unsigned byte order, the std::map<std::string> order of the native params.
fn qry_cmp(a: const u8[:], b: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < a.len && i < b.len {
        if a[i] != b[i] {
            if a[i] < b[i] {
                return 0 - 1;
            }
            return 1;
        }
        i = i + 1;
    }
    if a.len == b.len {
        return 0;
    }
    if a.len < b.len {
        return 0 - 1;
    }
    return 1;
}

// Core decoder. plus_space turns '+' into ' ' (a form component); the path
// decoder keeps it. Returns the bytes written to out, -1 when out is too small.
fn qry_decode(src: const u8[:], plus_space: i64, out: u8[:]) -> i64 {
    var i: i64 = 0;
    var n: i64 = 0;

    while i < src.len {
        if src[i] == 37 && i + 2 < src.len {
            let hi = qry_hexval(src[i + 1]);
            let lo = qry_hexval(src[i + 2]);

            if hi >= 0 && lo >= 0 {
                if n >= out.len {
                    return 0 - 1;
                }
                out[n] = ((hi << 4) | lo) as u8;
                n = n + 1;
                i = i + 3;
            } else {
                if n >= out.len {
                    return 0 - 1;
                }
                out[n] = src[i];
                n = n + 1;
                i = i + 1;
            }
        } else if src[i] == 43 && plus_space != 0 {
            if n >= out.len {
                return 0 - 1;
            }
            out[n] = 32;
            n = n + 1;
            i = i + 1;
        } else {
            if n >= out.len {
                return 0 - 1;
            }
            out[n] = src[i];
            n = n + 1;
            i = i + 1;
        }
    }
    return n;
}

// decode_path_component: %XX only, '+' stays a plus.
export fn qry_decode_path(src: const u8[:], out: u8[:]) -> i64 {
    return qry_decode(src, 0, out);
}

// httplib::decode_query_component: %XX and '+' as a space.
export fn qry_decode_query(src: const u8[:], out: u8[:]) -> i64 {
    return qry_decode(src, 1, out);
}

fn qry_is_alnum(c: u8) -> bool {
    if c >= 48 && c <= 57 {
        return true;
    }
    if c >= 65 && c <= 90 {
        return true;
    }
    if c >= 97 && c <= 122 {
        return true;
    }
    return false;
}

fn qry_hex_text(c: u8, out: u8[:], at: i64) -> i64 {
    let hi: u8 = c >> 4;
    let lo: u8 = c & 15;

    if at + 2 > out.len {
        return 0 - 1;
    }
    if hi < 10 {
        out[at] = 48 + hi;
    } else {
        out[at] = 55 + hi;
    }
    if lo < 10 {
        out[at + 1] = 48 + lo;
    } else {
        out[at + 1] = 55 + lo;
    }
    return at + 2;
}

// Next write offset after appending the encoded form of src[from .. to].
fn qry_encode_at(src: const u8[:], pos_from: i64, to: i64, out: u8[:], at: i64) -> i64 {
    var i: i64 = pos_from;
    var n: i64 = at;

    while i < to {
        let c = src[i];

        if qry_is_alnum(c) || c == 45 || c == 46 || c == 95 || c == 126 {
            if n >= out.len {
                return 0 - 1;
            }
            out[n] = c;
            n = n + 1;
        } else if c == 32 {
            if n >= out.len {
                return 0 - 1;
            }
            out[n] = 43;
            n = n + 1;
        } else if c == 43 {
            if n + 3 > out.len {
                return 0 - 1;
            }
            out[n] = 37;
            out[n + 1] = 50;
            out[n + 2] = 66;
            n = n + 3;
        } else if c == 33 || c == 36 || c == 39 || c == 40 || c == 41
            || c == 42 || c == 44 || c == 59 || c == 58 || c == 64
            || c == 47 || c == 63 {
            if n >= out.len {
                return 0 - 1;
            }
            out[n] = c;
            n = n + 1;
        } else {
            if n + 1 > out.len {
                return 0 - 1;
            }
            out[n] = 37;
            n = qry_hex_text(c, out, n + 1);
            if n < 0 {
                return 0 - 1;
            }
        }
        i = i + 1;
    }
    return n;
}

// httplib::encode_query_component with space_as_plus = true.
export fn qry_encode_component(src: const u8[:], out: u8[:]) -> i64 {
    return qry_encode_at(src, 0, src.len, out, 0);
}

fn qry_blob_at(blob: const u8[:], off: const i64[:], i: i64) -> const u8[:] {
    let pos_from: i64 = off[i];
    let to: i64 = off[i + 1];

    if pos_from < 0 || to < pos_from || to > blob.len {
        return blob[0 .. 0];
    }
    return blob[pos_from .. to];
}

// One raw token of the split, [a, b) already trimmed, either entered into the
// pair table or dropped. acc = [pairs, key bytes used, value bytes used,
// tokens seen]; a token with an empty key still counts as seen, exactly like
// the native cache that is filled before the key test.
fn qry_take(
    query: const u8[:],
    a: i64,
    b: i64,
    k_blob: u8[:],
    k_off: i64[:],
    v_blob: u8[:],
    v_off: i64[:],
    tok: i64[:],
    max: i64,
    base: i64,
    acc: i64[:]) -> bool {
    if a >= b {
        return true;
    }
    // the raw-token cache of one parse call: byte equality of the trimmed
    // tokens, since the same token sits at a different offset each time
    var j: i64 = base;

    while j < acc[3] {
        let ta: i64 = tok[2 * j];
        let tb: i64 = tok[2 * j + 1];

        if tb - ta == b - a {
            var k: i64 = 0;
            var same: bool = true;

            while k < b - a {
                if query[ta + k] != query[a + k] {
                    same = false;
                    k = b - a;
                } else {
                    k = k + 1;
                }
            }
            if same {
                return true;
            }
        }
        j = j + 1;
    }
    if 2 * acc[3] + 1 >= tok.len {
        return false;
    }
    tok[2 * acc[3]] = a;
    tok[2 * acc[3] + 1] = b;
    acc[3] = acc[3] + 1;

    // divide at the first '='; without one the whole token is the key
    var key_end: i64 = b;
    var val_from: i64 = b;
    var s: i64 = a;
    var found: bool = false;

    while s < b && !found {
        if query[s] == 61 {
            key_end = s;
            val_from = s + 1;
            found = true;
        } else {
            s = s + 1;
        }
    }
    if key_end == a {
        // an empty key is cached but never entered
        return true;
    }
    if acc[0] >= max || acc[0] + 1 >= k_off.len || acc[0] + 1 >= v_off.len {
        return false;
    }
    let k_at = qry_decode(query[a .. key_end], 1, k_blob[acc[1] .. k_blob.len]);

    if k_at < 0 {
        return false;
    }
    let v_at = qry_decode(query[val_from .. b], 1, v_blob[acc[2] .. v_blob.len]);

    if v_at < 0 {
        return false;
    }
    acc[0] = acc[0] + 1;
    acc[1] = acc[1] + k_at;
    acc[2] = acc[2] + v_at;
    k_off[acc[0]] = acc[1];
    v_off[acc[0]] = acc[2];
    return true;
}

// Parse one query text into the pair table, in native insertion order.
//
//   k_off / v_off   max + 1 offsets; k_off[0] and v_off[0] must be the bytes
//                   already used, so a second call appends
//   tok             2 * max slots: the trimmed raw token ranges of this call
//   st[0]           pairs already present on entry; the new count on exit
//
// Returns 0, or -1 when a caller buffer is too small.
export fn qry_parse(
    query: const u8[:],
    k_blob: u8[:],
    k_off: i64[:],
    v_blob: u8[:],
    v_off: i64[:],
    tok: i64[:],
    max: i64,
    st: i64[:]) -> i64 {
    let base: i64 = st[0];

    if base < 0 || base > max || base + 1 >= k_off.len || base + 1 >= v_off.len {
        return 0 - 1;
    }

    var acc: i64[4] = [base, k_off[base], v_off[base], base];
    var i: i64 = 0;
    var beg: i64 = 0;

    while i < query.len {
        if query[i] == 38 {
            var a: i64 = beg;
            var b: i64 = i;

            while a < b && qry_is_space_or_tab(query[a]) {
                a = a + 1;
            }
            while b > a && qry_is_space_or_tab(query[b - 1]) {
                b = b - 1;
            }
            if !qry_take(query, a, b, k_blob, k_off, v_blob, v_off, tok, max, base, acc) {
                return 0 - 1;
            }
            beg = i + 1;
        }
        i = i + 1;
    }
    if query.len > 0 {
        var a: i64 = beg;
        var b: i64 = query.len;

        while a < b && qry_is_space_or_tab(query[a]) {
            a = a + 1;
        }
        while b > a && qry_is_space_or_tab(query[b - 1]) {
            b = b - 1;
        }
        if !qry_take(query, a, b, k_blob, k_off, v_blob, v_off, tok, max, base, acc) {
            return 0 - 1;
        }
    }
    st[0] = acc[0];
    return 0;
}

// Re-encode a parsed pair list, in the order qry_parse produced it.
export fn qry_build(
    np: i64,
    k_blob: const u8[:],
    k_off: const i64[:],
    v_blob: const u8[:],
    v_off: const i64[:],
    out: u8[:]) -> i64 {
    if np < 0 || np + 1 > k_off.len || np + 1 > v_off.len {
        return 0 - 1;
    }

    var n: i64 = 0;
    var i: i64 = 0;

    while i < np {
        if i != 0 {
            if n >= out.len {
                return 0 - 1;
            }
            out[n] = 38;
            n = n + 1;
        }
        n = qry_encode_at(k_blob, k_off[i], k_off[i + 1], out, n);
        if n < 0 {
            return 0 - 1;
        }
        if n >= out.len {
            return 0 - 1;
        }
        out[n] = 61;
        n = n + 1;
        n = qry_encode_at(v_blob, v_off[i], v_off[i + 1], out, n);
        if n < 0 {
            return 0 - 1;
        }
        i = i + 1;
    }
    return n;
}

fn qry_key_of(
    i: i64,
    np: i64,
    k_blob: const u8[:],
    k_off: const i64[:],
    pk_blob: const u8[:],
    pk_off: const i64[:]) -> const u8[:] {
    if i < np {
        return qry_blob_at(k_blob, k_off, i);
    }
    return qry_blob_at(pk_blob, pk_off, i - np);
}

fn qry_raw_value_of(
    i: i64,
    np: i64,
    v_blob: const u8[:],
    v_off: const i64[:],
    pv_blob: const u8[:],
    pv_off: const i64[:]) -> const u8[:] {
    if i < np {
        return qry_blob_at(v_blob, v_off, i);
    }
    return qry_blob_at(pv_blob, pv_off, i - np);
}

// get_params: the query pairs and then the path pairs assigned into one map.
//
// A path pair carries the value exactly as the route produced it, so the path
// rule (no '+' handling) is applied here, not by the caller. Output is sorted by
// key with the last assignment per key kept, i.e. the native std::map.
//
// Returns 0, or -1 when a caller buffer is too small.
export fn qry_params(
    np: i64,
    k_blob: const u8[:],
    k_off: const i64[:],
    v_blob: const u8[:],
    v_off: const i64[:],
    pnp: i64,
    pk_blob: const u8[:],
    pk_off: const i64[:],
    pv_blob: const u8[:],
    pv_off: const i64[:],
    ok_blob: u8[:],
    ok_off: i64[:],
    ov_blob: u8[:],
    ov_off: i64[:],
    out_np: i64[:]) -> i64 {
    let total: i64 = np + pnp;

    out_np[0] = 0;
    if np < 0 || pnp < 0 || np + 1 > k_off.len || np + 1 > v_off.len
        || pnp + 1 > pk_off.len || pnp + 1 > pv_off.len {
        return 0 - 1;
    }
    ok_off[0] = 0;
    ov_off[0] = 0;
    if total > ok_off.len - 1 || total > ov_off.len - 1 {
        return 0 - 1;
    }

    var have_prev: bool = false;
    var prev: const u8[:] = k_blob[0 .. 0];
    var out_n: i64 = 0;
    var k_used: i64 = 0;
    var v_used: i64 = 0;

    while true {
        // the smallest key above the previous one
        var best: i64 = 0 - 1;
        var i: i64 = 0;

        while i < total {
            let key = qry_key_of(i, np, k_blob, k_off, pk_blob, pk_off);
            var above: bool = true;

            if have_prev && qry_cmp(key, prev) <= 0 {
                above = false;
            }
            if above {
                if best < 0 {
                    best = i;
                } else {
                    let bkey = qry_key_of(best, np, k_blob, k_off, pk_blob, pk_off);

                    if qry_cmp(key, bkey) < 0 {
                        best = i;
                    }
                }
            }
            i = i + 1;
        }
        if best < 0 {
            return 0;
        }
        // the last assignment of this key wins
        let bkey = qry_key_of(best, np, k_blob, k_off, pk_blob, pk_off);
        var last: i64 = best;
        var j: i64 = best + 1;

        while j < total {
            if qry_cmp(qry_key_of(j, np, k_blob, k_off, pk_blob, pk_off), bkey) == 0 {
                last = j;
            }
            j = j + 1;
        }
        let key = bkey;

        if out_n + 1 >= ok_off.len || out_n + 1 >= ov_off.len {
            return 0 - 1;
        }
        let k_at = qry_put(ok_blob, k_used, key);

        if k_at < 0 {
            return 0 - 1;
        }
        let raw = qry_raw_value_of(last, np, v_blob, v_off, pv_blob, pv_off);

        if last < np {
            let v_at = qry_put(ov_blob, v_used, raw);

            if v_at < 0 {
                return 0 - 1;
            }
            v_used = v_at;
        } else {
            let v_at = qry_decode(raw, 0, ov_blob[v_used .. ov_blob.len]);

            if v_at < 0 {
                return 0 - 1;
            }
            v_used = v_used + v_at;
        }
        k_used = k_at;
        out_n = out_n + 1;
        out_np[0] = out_n;
        ok_off[out_n] = k_used;
        ov_off[out_n] = v_used;
        prev = key;
        have_prev = true;
    }
    return 0;
}
