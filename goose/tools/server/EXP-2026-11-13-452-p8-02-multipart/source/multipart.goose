// Multipart / path-parameter helpers in Goose, from tools/server/server-http.cpp
// (decode_path_component, get_params) and server_http_context::post (multipart
// form translation).
//
// Owned decisions:
//   1. path-component percent-decoding: "%HH" is decoded only when both digits
//      are hex; anything else is copied literally (no '+' handling, that is
//      query-string semantics and lives in httplib);
//   2. form-field grouping: the first occurrence of a key becomes a JSON string,
//      later occurrences append to an array, comparing keys with the language's
//      own byte-slice equality (no hand-written byte loop);
//   3. parameter precedence: a path parameter overrides a query parameter of the
//      same name, and only the path value is percent-decoded.
//
// The multipart boundary parsing itself stays with httplib.
extern "fg452_ld_u8" fn ld_u8(p: u64, o: u64) -> u32;
extern "fg452_st_u8" fn st_u8(p: u64, o: u64, v: u32);
extern "fg452_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn hexval(c: u32) -> i64 {
    if c >= 48 as u32 {
        if c <= 57 as u32 {
            return (c - 48 as u32) as i64;
        }
    }

    if c >= 97 as u32 {
        if c <= 102 as u32 {
            return ( (c - 97 as u32) as i64) + 10 as i64;
        }
    }

    if c >= 65 as u32 {
        if c <= 70 as u32 {
            return ( (c - 65 as u32) as i64) + 10 as i64;
        }
    }
    return -(1 as i64);
}
// Decode into out (capacity cap bytes) and return the full decoded length.

export fn decode_component(p: u64, n: i64, out: u64, cap: i64) -> i64 {
    var o: i64 = 0 as i64;
    var i: i64 = 0 as i64;

    while i < n {
        var emit: i64 = 1 as i64;

        if ld_u8(p, i as u64) == 37 as u32 {
            if i + 2 < n {
                let hi: i64 = hexval(ld_u8(p, (i + 1 as i64) as u64));
                let lo: i64 = hexval(ld_u8(p, (i + 2 as i64) as u64));
                var ok: i64 = 0 as i64;

                if hi >= 0 as i64 {
                    if lo >= 0 as i64 {
                        ok = 1 as i64;
                    }
                }

                if ok != 0 as i64 {
                    if o < cap {
                        st_u8(out, o as u64, ( ( (hi << 4) | lo) as u32));
                    }
                    o = o + 1 as i64;
                    i = i + 3 as i64;
                    emit = 0 as i64;
                }
            }
        }

        if emit != 0 as i64 {
            if o < cap {
                st_u8(out, o as u64, ld_u8(p, i as u64));
            }
            o = o + 1 as i64;
            i = i + 1 as i64;
        }
    }
    return o;
}
// Slice equality is the language's own comparison: no byte loop.

fn key_eq(keys: const u8[:], ao: i64, an: i64, bo: i64, bn: i64) -> bool {
    if an != bn {
        return false;
    }

    if an == 0 as i64 {
        return true;
    }
    return keys[ao .. ao + an] == keys[bo .. bo + bn];
}
// 1 when an earlier field already used this key (native appends to an array),
// 0 for the first occurrence. koffs/klens are i64 arrays of length idx.

fn form_repeat(idx: i64, keys: const u8[:], koffs: i64[:], klens: i64[:]) -> i64 {
    let bo: i64 = koffs[idx];
    let bn: i64 = klens[idx];
    var j: i64 = 0 as i64;

    while j < idx {
        let ao: i64 = koffs[j];
        let an: i64 = klens[j];
        let same: bool = key_eq(keys, ao, an, bo, bn);

        if same {
            return 1 as i64;
        }
        j = j + 1 as i64;
    }
    return 0 as i64;
}
// Writes one flag per field: 0 -> new JSON string, 1 -> append to the array.

export fn form_actions(n: i64, keys: const u8[:], koffs: i64[:], klens: i64[:], out: i64[:]) -> i64 {
    var i: i64 = 0 as i64;

    while i < n {
        let r: i64 = form_repeat(i, keys, koffs, klens);
        out[i] = r;
        i = i + 1 as i64;
    }
    return n;
}
// Parameter source: 0 absent, 1 query value, 2 path value (decoded, wins).

export fn param_source(has_query: i64, has_path: i64) -> i64 {
    if has_path != 0 as i64 {
        return 2 as i64;
    }

    if has_query != 0 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}

