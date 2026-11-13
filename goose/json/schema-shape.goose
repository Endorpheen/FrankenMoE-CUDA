// JSON-schema-to-GBNF shaping decisions in Goose, from common/json-schema-to-grammar.cpp
// (build_repetition and _build_object_rule).
//
// Owned decisions:
//   - which repetition form a (min, max) pair takes: empty, "?", "+", "*" or a
//     bounded "{min,max}";
//   - the separator form recurses with min-1 and an optional wrapper when min is 0;
//   - an object emits required properties in declaration order first, then one
//     alternative branch per optional property, with additionalProperties counted
//     as an extra "*" optional;
//   - an object with no properties at all emits the empty-object form.
//
// The rule text and the name map stay native; this module decides the shapes.
extern "fg472_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn put(out: u64, i: i64, v: i64) {
    st_i64(out, (i * (8 as i64)) as u64, v);
}
const INT_MAX: i64 = 2147483647 as i64;
// kinds: 0 empty, 1 optional "?", 2 plus, 3 star, 4 bounded range,
// 5 separator form (recursive item with min-1).

export fn repetition_kind(min_items: i64, max_items: i64, has_separator: i64, out: u64) -> i64 {
    var has_max: i64 = 0 as i64;

    if max_items != INT_MAX as i64 {
        has_max = 1 as i64;
    }
    var kind: i64 = 4 as i64;

    if max_items == 0 as i64 {
        kind = 0 as i64;
    } else {
        if min_items == 0 as i64 && max_items == 1 as i64 {
            kind = 1 as i64;
        } else {
            if has_separator == 0 as i64 {
                if min_items == 1 as i64 && has_max == 0 as i64 {
                    kind = 2 as i64;
                }

                if min_items == 0 as i64 && has_max == 0 as i64 {
                    kind = 3 as i64;
                }
            } else {
                kind = 5 as i64;
            }
        }
    }
    put(out, 0 as i64, kind);
    return kind;
}
// For the separator form: the recursive minimum and whether the whole thing is
// wrapped as optional.

export fn separator_form(min_items: i64, out: u64) -> i64 {
    var inner_min: i64 = min_items - (1 as i64);

    if min_items == 0 as i64 {
        inner_min = 0 as i64;
    }
    var wrapped: i64 = 0 as i64;

    if min_items == 0 as i64 {
        wrapped = 1 as i64;
    }
    put(out, 0 as i64, inner_min);
    put(out, 1 as i64, wrapped);
    return inner_min;
}
// Object shape: flags[i] is 1 when property i is required. Writes required count,
// optional count (with additionalProperties as one extra), the number of optional
// branches, and whether the object is empty.

export fn object_shape(flags: i64[:], has_additional: i64, out: u64) -> i64 {
    var req: i64 = 0 as i64;
    var opt: i64 = 0 as i64;
    var i: i64 = 0 as i64;

    while i < flags.len {
        if flags[i] != 0 as i64 {
            req = req + 1 as i64;
        } else {
            opt = opt + 1 as i64;
        }
        i = i + 1 as i64;
    }

    if has_additional != 0 as i64 {
        opt = opt + 1 as i64;
    }
    var empty: i64 = 0 as i64;

    if req == 0 as i64 && opt == 0 as i64 {
        empty = 1 as i64;
    }
    put(out, 0 as i64, req);
    put(out, 1 as i64, opt);
    put(out, 2 as i64, opt);
    // one branch per optional property
    put(out, 3 as i64, empty);
    return 0 as i64;
}
// Whether the comma separator is needed before the optional group.

export fn optional_group_prefix(req: i64, opt: i64, out: u64) -> i64 {
    var sep: i64 = 0 as i64;

    if req > 0 as i64 && opt > 0 as i64 {
        sep = 1 as i64;
    }
    put(out, 0 as i64, sep);
    return sep;
}

