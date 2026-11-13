// Handler binding and path-parameter extraction in Goose, from
// vendor/cpp-httplib PathParamsMatcher (constructor and match) and
// server-http.cpp registration (handlers.emplace, path_prefix + path).
//
// Owned decisions:
//   - a pattern is a list of static fragments separated by parameters: the
//     matcher consumes each fragment, then captures up to the next '/'; a
//     parameter may capture an empty string, and the path must be fully consumed;
//   - duplicate parameter names in one pattern are a construction error;
//   - handler registration is first-wins (emplace never overwrites);
//   - the registered path is the API prefix followed by the route.
extern "fg462_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn put(out: u64, i: i64, v: i64) {
    st_i64(out, (i * (8 as i64)) as u64, v);
}
// Match a path against fragments and capture parameters.
// frags holds the fragment bytes, foffs/flens locate them; np is the number of
// parameters (captures happen after fragments 0..np-1). out receives
// [matched, n, off0, len0, off1, len1, ...]. Returns 1 when the path matches.

export fn path_match(frags: const u8[:], foffs: i64[:], flens: i64[:], np: i64, path: const u8[:], res: i64[:]) -> i64 {
    var start: i64 = 0 as i64;
    var ncap: i64 = 0 as i64;
    var i: i64 = 0 as i64;
    var ok: i64 = 1 as i64;

    while i < flens.len && ok != 0 as i64 {
        let fl: i64 = flens[i];

        if start + fl > path.len {
            ok = 0 as i64;
        } else {
            let fo: i64 = foffs[i];
            var k: i64 = 0 as i64;

            while k < fl && ok != 0 as i64 {
                if frags[fo + k] != path[start + k] {
                    ok = 0 as i64;
                }
                k = k + 1 as i64;
            }

            if ok != 0 as i64 {
                start = start + fl;

                if i < np {
                    // capture until the next separator or the end
                    var sep: i64 = start;
                    var found: i64 = 0 as i64;

                    while sep < path.len && found == 0 as i64 {
                        if path[sep] == 47 as u8 {
                            found = 1 as i64;
                        } else {
                            sep = sep + 1 as i64;
                        }
                    }

                    if ncap + 1 < np + 2 as i64 {
                        res[2 as i64 + ncap * 2 as i64] = start;
                        res[3 as i64 + ncap * 2 as i64] = sep - start;
                    }
                    ncap = ncap + 1 as i64;
                    start = sep + 1 as i64;
                }
            }
        }
        i = i + 1 as i64;
    }
    var matched: i64 = 0 as i64;

    if ok != 0 as i64 {
        if start >= path.len {
            matched = 1 as i64;
        }
    }
    res[0 as i64] = matched;
    res[1 as i64] = ncap;
    return matched;
}
// Duplicate parameter names are rejected when the matcher is built.

export fn duplicate(names: const u8[:], noffs: i64[:], nlens: i64[:]) -> i64 {
    var i: i64 = 0 as i64;

    while i < noffs.len {
        var j: i64 = i + 1 as i64;

        while j < noffs.len {
            if nlens[i] == nlens[j] {
                var k: i64 = 0 as i64;
                var same: i64 = 1 as i64;

                while k < nlens[i] {
                    if names[noffs[i] + k] != names[noffs[j] + k] {
                        same = 0 as i64;
                    }
                    k = k + 1 as i64;
                }

                if same != 0 as i64 {
                    return 1 as i64;
                }
            }
            j = j + 1 as i64;
        }
        i = i + 1 as i64;
    }
    return 0 as i64;
}
// handlers.emplace: an existing route keeps its first handler.

export fn bind_keep(existing: i64) -> i64 {
    if existing != 0 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}
// full_path = path_prefix + path

export fn full_path(prefix: const u8[:], path: const u8[:], dst: u8[:]) -> i64 {
    var o: i64 = 0 as i64;
    var i: i64 = 0 as i64;

    while i < prefix.len {
        if o < dst.len {
            dst[o] = prefix[i];
        }
        o = o + 1 as i64;
        i = i + 1 as i64;
    }
    i = 0 as i64;

    while i < path.len {
        if o < dst.len {
            dst[o] = path[i];
        }
        o = o + 1 as i64;
        i = i + 1 as i64;
    }
    return o;
}

