// Schema introspection in Goose, from common/json-schema.cpp
// (json_type, common_chat_schema::value_types, may_be_string, kind_name,
// type_name).
//
// The tree is the flat encoding the schema builder emits; a const/enum node
// carries the value_type of each literal instead of the JSON node.
//
// type_set is a bitmask over the value_type order: null, boolean, number,
// integer, string, array, object.

import std;
const K_ANY: i64 = 0;
const K_REF: i64 = 1;
const K_ANY_OF: i64 = 2;
const K_ALL_OF: i64 = 3;
const K_CONST: i64 = 4;
const K_ENUM: i64 = 5;
const K_NULL: i64 = 6;
const K_BOOLEAN: i64 = 7;
const K_NUMBER: i64 = 8;
const K_INTEGER: i64 = 9;
const K_STRING: i64 = 10;
const K_ARRAY: i64 = 11;
const K_TUPLE: i64 = 12;
const K_OBJECT: i64 = 13;
const T_NULL: i64 = 0;
const T_BOOLEAN: i64 = 1;
const T_NUMBER: i64 = 2;
const T_INTEGER: i64 = 3;
const T_STRING: i64 = 4;
const T_ARRAY: i64 = 5;
const T_OBJECT: i64 = 6;

fn bit(t: i64) -> i64 {
    return 1 << t;
}

fn all_types() -> i64 {
    return 127;
}

fn vis_find(vis: const i64[:], n: i64, id: i64) -> i64 {
    var i: i64 = 0;

    while i < n {
        if vis[i] == id {
            return i;
        }
        i = i + 1;
    }
    return -1;
}
// state: 0 visited count
recursive fn vt(
    id: i64,
    nkind: const i64[:],
    ncf: const i64[:],
    ncc: const i64[:],
    nchild: const i64[:],
    npf: const i64[:],
    npc: const i64[:],
    pnode: const i64[:],
    nadd: const i64[:],
    nitems: const i64[:],
    ntarget: const i64[:],
    ctype: const i64[:],
    nef: const i64[:],
    nec: const i64[:],
    eval_t: const i64[:],
    vis: i64[:],
    st: i64[:]) -> i64 {
    let k = nkind[id];

    if k == K_ANY {
        return all_types();
    }

    if k == K_NULL {
        return bit(T_NULL);
    }

    if k == K_BOOLEAN {
        return bit(T_BOOLEAN);
    }

    if k == K_NUMBER {
        return bit(T_NUMBER) | bit(T_INTEGER);
    }

    if k == K_INTEGER {
        return bit(T_INTEGER);
    }

    if k == K_STRING {
        return bit(T_STRING);
    }

    if k == K_ARRAY || k == K_TUPLE {
        return bit(T_ARRAY);
    }

    if k == K_OBJECT {
        return bit(T_OBJECT);
    }

    if k == K_CONST {
        return bit(ctype[id]);
    }

    if k == K_ENUM {
        var m: i64 = 0;
        var i: i64 = 0;
        let n = nec[id];

        while i < n {
            m = m | bit(eval_t[nef[id] + i]);
            i = i + 1;
        }
        return m;
    }

    if k == K_REF {
        let t = ntarget[id];

        if t < 0 {
            return 0;
        }

        if vis_find(vis, st[0], t) >= 0 {
            return 0;
        }
        vis[st[0]] = t;
        st[0] = st[0] + 1;
        let m = vt(
            t,
            nkind,
            ncf,
            ncc,
            nchild,
            npf,
            npc,
            pnode,
            nadd,
            nitems,
            ntarget,
            ctype,
            nef,
            nec,
            eval_t,
            vis,
            st);
        st[0] = st[0] - 1;
        return m;
    }

    if k == K_ANY_OF {
        var m: i64 = 0;
        var i: i64 = 0;

        while i < ncc[id] {
            m = m | vt(
                nchild[ncf[id] + i],
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pnode,
                nadd,
                nitems,
                ntarget,
                ctype,
                nef,
                nec,
                eval_t,
                vis,
                st);
            i = i + 1;
        }
        return m;
    }

    if k == K_ALL_OF {
        var m: i64 = all_types();
        var i: i64 = 0;

        while i < ncc[id] {
            m = m & vt(
                nchild[ncf[id] + i],
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pnode,
                nadd,
                nitems,
                ntarget,
                ctype,
                nef,
                nec,
                eval_t,
                vis,
                st);
            i = i + 1;
        }
        return m;
    }
    return 0;
}
recursive fn mbs(
    id: i64,
    nkind: const i64[:],
    ncf: const i64[:],
    ncc: const i64[:],
    nchild: const i64[:],
    ntarget: const i64[:],
    ctype: const i64[:],
    nef: const i64[:],
    nec: const i64[:],
    eval_t: const i64[:],
    vis: i64[:],
    st: i64[:]) -> i64 {
    let k = nkind[id];

    if k == K_STRING {
        return 1;
    }

    if k == K_CONST {
        if ctype[id] == T_STRING {
            return 1;
        }
        return 0;
    }

    if k == K_ENUM {
        var i: i64 = 0;
        let n = nec[id];

        while i < n {
            if eval_t[nef[id] + i] == T_STRING {
                return 1;
            }
            i = i + 1;
        }
        return 0;
    }

    if k == K_REF {
        let t = ntarget[id];

        if t < 0 {
            return 0;
        }

        if vis_find(vis, st[0], t) >= 0 {
            return 0;
        }
        vis[st[0]] = t;
        st[0] = st[0] + 1;
        let r = mbs(t, nkind, ncf, ncc, nchild, ntarget, ctype, nef, nec, eval_t, vis, st);
        st[0] = st[0] - 1;
        return r;
    }

    if k == K_ANY_OF {
        var i: i64 = 0;

        while i < ncc[id] {
            if mbs(nchild[ncf[id] + i], nkind, ncf, ncc, nchild, ntarget, ctype, nef, nec, eval_t, vis, st) != 0 {
                return 1;
            }
            i = i + 1;
        }
        return 0;
    }

    if k == K_ALL_OF {
        var any_string: i64 = 0;
        var i: i64 = 0;

        while i < ncc[id] {
            let c = nchild[ncf[id] + i];

            if nkind[c] != K_ANY {
                if mbs(c, nkind, ncf, ncc, nchild, ntarget, ctype, nef, nec, eval_t, vis, st) == 0 {
                    return 0;
                }
                any_string = 1;
            }
            i = i + 1;
        }
        return any_string;
    }
    return 0;
}

export fn schema_value_types(
    root: i64,
    nkind: const i64[:],
    ncf: const i64[:],
    ncc: const i64[:],
    nchild: const i64[:],
    npf: const i64[:],
    npc: const i64[:],
    pnode: const i64[:],
    nadd: const i64[:],
    nitems: const i64[:],
    ntarget: const i64[:],
    ctype: const i64[:],
    nef: const i64[:],
    nec: const i64[:],
    eval_t: const i64[:],
    vis: i64[:],
    st: i64[:]) -> i64 {
    st[0] = 0;
    return vt(root, nkind, ncf, ncc, nchild, npf, npc, pnode, nadd, nitems, ntarget, ctype, nef, nec, eval_t, vis, st);
}

export fn schema_may_be_string(
    root: i64,
    nkind: const i64[:],
    ncf: const i64[:],
    ncc: const i64[:],
    nchild: const i64[:],
    ntarget: const i64[:],
    ctype: const i64[:],
    nef: const i64[:],
    nec: const i64[:],
    eval_t: const i64[:],
    vis: i64[:],
    st: i64[:]) -> i64 {
    st[0] = 0;
    return mbs(root, nkind, ncf, ncc, nchild, ntarget, ctype, nef, nec, eval_t, vis, st);
}
// kind_name / type_name: the identifier used in diagnostics

export fn schema_kind_name(k: i64, out: u8[:]) -> i64 {
    var n: const u8[:] = "?";

    if k == 0 {
        n = "any";
    } else

    if k == 1 {
        n = "ref";
    } else

    if k == 2 {
        n = "anyOf";
    } else

    if k == 3 {
        n = "allOf";
    } else

    if k == 4 {
        n = "const";
    } else

    if k == 5 {
        n = "enum";
    } else

    if k == 6 {
        n = "null";
    } else

    if k == 7 {
        n = "boolean";
    } else

    if k == 8 {
        n = "number";
    } else

    if k == 9 {
        n = "integer";
    } else

    if k == 10 {
        n = "string";
    } else

    if k == 11 {
        n = "array";
    } else

    if k == 12 {
        n = "tuple";
    } else

    if k == 13 {
        n = "object";
    }
    var i: i64 = 0;

    while i < n.len {
        if i < out.len {
            out[i] = n[i];
        }
        i = i + 1;
    }
    return n.len;
}

export fn schema_type_name(t: i64, out: u8[:]) -> i64 {
    var n: const u8[:] = "?";

    if t == 0 {
        n = "null";
    } else

    if t == 1 {
        n = "boolean";
    } else

    if t == 2 {
        n = "number";
    } else

    if t == 3 {
        n = "integer";
    } else

    if t == 4 {
        n = "string";
    } else

    if t == 5 {
        n = "array";
    } else

    if t == 6 {
        n = "object";
    }
    var i: i64 = 0;

    while i < n.len {
        if i < out.len {
            out[i] = n[i];
        }
        i = i + 1;
    }
    return n.len;
}

