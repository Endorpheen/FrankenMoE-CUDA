// JSON-schema-to-GBNF visit() dispatch, rule map and $ref targets in Goose,
// from common/json-schema-to-grammar.cpp (visit, _add_rule, _add_primitive,
// _resolve_ref, _generate_union_rule, _build_object_rule).
//
// Owned decisions:
//   - the rule-name policy: a reserved name gets a trailing '-', an empty name
//     becomes "root", and a sub-name is the name plus '-' when non-empty;
//   - the kind dispatch for ANY, REF, ANY_OF, OBJECT, STRING, BOOLEAN, NUMBER
//     and NULL (ALL_OF, patterns, const/enum, tuples, arrays and integer bounds
//     are ported in their own slices and are not part of this corpus);
//   - $ref resolution: the fragment after the first '#', the "ref"-prefixed name,
//     visiting the target under that name only when the rule is absent and the
//     ref is not already being resolved, and an unresolved ref producing an
//     empty rule;
//   - the primitive dependency graph and the "root" override;
//   - the object body: property kv rules, the required chain and the optional
//     alternatives with their recursive rest rules;
//   - anyOf: alternatives named <name>-<i> (or alternative-<i> when the name is
//     empty) joined by " | ".
//
// The primitive rule contents are native data passed in as a table; the schema
// walk above them and the rule map storage are Goose here.

import std;

fn append_s(dst: u8[:], at: i64, s: const u8[:], off: i64, len: i64) -> i64 {
    var k: i64 = 0;

    while k < len {
        if at + k < dst.len {
            dst[at + k] = s[off + k];
        }
        k = k + 1;
    }
    return at + len;
}

fn append_c(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn append_dec(dst: u8[:], at: i64, v: i64) -> i64 {
    var p: i64 = at;
    var x: i64 = v;

    if x < 0 {
        p = append_c(dst, p, '-');
        x = 0 - x;
    }
    var digits: i64 = 1;
    var y: i64 = x;

    while y >= 10 {
        digits = digits + 1;
        y = y / 10;
    }
    var pos: i64 = p + digits - 1;
    y = x;

    while y > 0 {
        if pos < dst.len {
            dst[pos] = (48 as u8) + ( (y % 10) as u8);
        }
        pos = pos - 1;
        y = y / 10;
    }

    if x == 0 {
        if p < dst.len {
            dst[p] = '0';
        }
    }
    return p + digits;
}

fn eq_bytes(a: const u8[:], b: const u8[:]) -> bool {
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

fn sanitize_into(s: const u8[:], dst: u8[:], at: i64) -> i64 {
    var i: i64 = 0;
    var n: i64 = 0;
    var run: bool = false;

    while i < s.len {
        let b = s[i];
        var ok: bool = false;

        if b >= 'a' && b <= 'z' {
            ok = true;
        }

        if b >= 'A' && b <= 'Z' {
            ok = true;
        }

        if b >= '0' && b <= '9' {
            ok = true;
        }

        if b == '-' {
            ok = true;
        }

        if ok {
            if at + n < dst.len {
                dst[at + n] = b;
            }
            n = n + 1;
            run = false;
        } else {
            if !run {
                if at + n < dst.len {
                    dst[at + n] = '-';
                }
                n = n + 1;
                run = true;
            }
        }
        i = i + 1;
    }
    return n;
}

fn is_reserved(name: const u8[:]) -> bool {
    let names: const u8[:][> ..] = ["root", "boolean", "decimal-part", "integral-part", "number", "integer", "value", "object", "array", "uuid", "char", "string", "null", "date", "time", "date-time", "date-string", "time-string", "date-time-string",];
    for & n in names {
        if eq_bytes(name, n) {
            return true;
        }
    }
    return false;
}
// rule_name: reserved -> name+"-", empty -> "root", else the name.

fn mk_rule_name(name: const u8[:], tb: u8[:], at: i64) -> i64 {
    var p: i64 = at;

    if is_reserved(name) {
        p = append_s(tb, p, name, 0, name.len);
        p = append_c(tb, p, '-');
        return p - at;
    }

    if name.len == 0 {
        return append_s(tb, at, "root", 0, 4) - at;
    }
    return append_s(tb, at, name, 0, name.len) - at;
}

fn mk_sub_name(name: const u8[:], tb: u8[:], at: i64) -> i64 {
    var p = append_s(tb, at, name, 0, name.len);

    if name.len != 0 {
        p = append_c(tb, p, '-');
    }
    return p - at;
}

fn format_literal_at(dump: const u8[:], out: u8[:], at: i64) -> i64 {
    var p = append_c(out, at, '"');
    var i: i64 = 0;

    while i < dump.len {
        let c = dump[i];

        if c == (13 as u8) {
            p = append_c(out, p, '\\');
            p = append_c(out, p, 'r');
        } else

        if c == (10 as u8) {
            p = append_c(out, p, '\\');
            p = append_c(out, p, 'n');
        } else

        if c == '"' {
            p = append_c(out, p, '\\');
            p = append_c(out, p, '"');
        } else

        if c == '\\' {
            p = append_c(out, p, '\\');
            p = append_c(out, p, '\\');
        } else {
            p = append_c(out, p, c);
        }
        i = i + 1;
    }
    return append_c(out, p, '"');
}

fn find_entry(mo: const i64[:], ml: const i64[:], mb: const u8[:], key: const u8[:], mc: i64) -> i64 {
    var i: i64 = 0;

    while i < mc {
        if ml[i] == key.len {
            if eq_bytes(mb[mo[i] .. mo[i] + ml[i]], key) {
                return i;
            }
        }
        i = i + 1;
    }
    return -1;
}
// _add_rule over the caller buffers. key and text are slices into tb.

fn add_rule(
    mo: i64[:],
    ml: i64[:],
    vo: i64[:],
    vl: i64[:],
    mb: u8[:],
    st: i64[:],
    key: const u8[:],
    text: const u8[:]) -> i64, i64 {
    let esc_at = st[5];
    let esc_len = sanitize_into(key, mb, esc_at);
    st[5] = esc_at + esc_len;
    let esc = mb[esc_at .. esc_at + esc_len];
    let found = find_entry(mo, ml, mb, esc, st[0]);

    if found < 0 {
        let idx = st[0];
        let to = st[5];
        let te = append_s(mb, to, text, 0, text.len);
        st[5] = te;
        mo[idx] = esc_at;
        ml[idx] = esc_len;
        vo[idx] = to;
        vl[idx] = text.len;
        st[0] = idx + 1;
        return esc_at, esc_len;
    }

    if eq_bytes(mb[vo[found] .. vo[found] + vl[found]], text) {
        return esc_at, esc_len;
    }
    var i: i64 = 0;

    while i >= 0 {
        let suf_at = st[5];
        var p = append_s(mb, suf_at, esc, 0, esc_len);
        p = append_dec(mb, p, i);
        st[5] = p;
        let cand = mb[suf_at .. suf_at + (p - suf_at)];
        let f2 = find_entry(mo, ml, mb, cand, st[0]);

        if f2 < 0 {
            let idx = st[0];
            let to2 = st[5];
            let te2 = append_s(mb, to2, text, 0, text.len);
            st[5] = te2;
            mo[idx] = suf_at;
            ml[idx] = p - suf_at;
            vo[idx] = to2;
            vl[idx] = text.len;
            st[0] = idx + 1;
            return suf_at, p - suf_at;
        }

        if eq_bytes(mb[vo[f2] .. vo[f2] + vl[f2]], text) {
            return suf_at, p - suf_at;
        }
        i = i + 1;
    }
    return esc_at, esc_len;
}
// primitive table: ptname/ptcontent are string indices, ptdeps has 6 type ids
// per type (-1 padded). Types: 0 value,1 object,2 array,3 string,4 number,
// 5 boolean,6 null,7 char,8 integral,9 decimal.
recursive fn add_primitive(
    typ: i64,
    nm: const u8[:],
    strs: const u8[:],
    sdir: const i64[:],
    ptname: const i64[:],
    ptcontent: const i64[:],
    ptdeps: const i64[:],
    mo: i64[:],
    ml: i64[:],
    vo: i64[:],
    vl: i64[:],
    mb: u8[:],
    st: i64[:]) -> i64, i64 {
    let csi = ptcontent[typ];
    let co = sdir[2 * csi + 0];
    let cl = sdir[2 * csi + 1];
    let rn_a, rn_b = add_rule(mo, ml, vo, vl, mb, st, nm, strs[co .. co + cl]);
    var d: i64 = 0;

    while d < 6 {
        let dep = ptdeps[typ * 6 + d];

        if dep >= 0 {
            let dsi = ptname[dep];
            let do0 = sdir[2 * dsi + 0];
            let dl = sdir[2 * dsi + 1];
            let dn = strs[do0 .. do0 + dl];

            if find_entry(mo, ml, mb, dn, st[0]) < 0 {
                let da, db = add_primitive(dep, dn, strs, sdir, ptname, ptcontent, ptdeps, mo, ml, vo, vl, mb, st);
            }
        }
        d = d + 1;
    }
    return rn_a, rn_b;
}

fn visit_primitive(
    rule_name: const u8[:],
    typ: i64,
    strs: const u8[:],
    sdir: const i64[:],
    ptname: const i64[:],
    ptcontent: const i64[:],
    ptdeps: const i64[:],
    mo: i64[:],
    ml: i64[:],
    vo: i64[:],
    vl: i64[:],
    mb: u8[:],
    st: i64[:]) -> i64, i64 {
    var nm: const u8[:] = rule_name;

    if eq_bytes(rule_name, "root") {
        nm = "root";
    } else {
        let dsi = ptname[typ];
        let do0 = sdir[2 * dsi + 0];
        let dl = sdir[2 * dsi + 1];
        nm = strs[do0 .. do0 + dl];
    }
    let a, b = add_primitive(typ, nm, strs, sdir, ptname, ptcontent, ptdeps, mo, ml, vo, vl, mb, st);
    return a, b;
}
// visit(si, name) -> the rule name returned by the native visit.
recursive fn visit(
    si: i64,
    name: const u8[:],
    kind: const i64[:],
    pfirst: const i64[:],
    pcount: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: const i64[:],
    add_kind: const i64[:],
    target: const i64[:],
    refstr: const i64[:],
    pname: const i64[:],
    pschema: const i64[:],
    preq: const i64[:],
    pdump: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    ptname: const i64[:],
    ptcontent: const i64[:],
    ptdeps: const i64[:],
    mo: i64[:],
    ml: i64[:],
    vo: i64[:],
    vl: i64[:],
    mb: u8[:],
    tb: u8[:],
    ob: u8[:],
    st: i64[:],
    rref: i64[:]) -> i64, i64 {
    let rn_at = st[1];
    let rn_len = mk_rule_name(name, tb, rn_at);
    st[1] = rn_at + rn_len;
    let rule_name = tb[rn_at .. rn_at + rn_len];
    let sn_at = st[1];
    let sn_len = mk_sub_name(name, tb, sn_at);
    st[1] = sn_at + sn_len;
    let sub_name = tb[sn_at .. sn_at + sn_len];
    let k = kind[si];

    if k == 0 {
        let va, vb = add_primitive(0, "value", strs, sdir, ptname, ptcontent, ptdeps, mo, ml, vo, vl, mb, st);
        return add_rule(mo, ml, vo, vl, mb, st, rule_name, mb[va .. va + vb]);
    }

    if k == 4 {
        let a3, b3 = visit_primitive(rule_name, 3, strs, sdir, ptname, ptcontent, ptdeps, mo, ml, vo, vl, mb, st);
        return a3, b3;
    }

    if k == 5 {
        let a5, b5 = visit_primitive(rule_name, 5, strs, sdir, ptname, ptcontent, ptdeps, mo, ml, vo, vl, mb, st);
        return a5, b5;
    }

    if k == 6 {
        let a4, b4 = visit_primitive(rule_name, 4, strs, sdir, ptname, ptcontent, ptdeps, mo, ml, vo, vl, mb, st);
        return a4, b4;
    }

    if k == 7 {
        let a6, b6 = visit_primitive(rule_name, 6, strs, sdir, ptname, ptcontent, ptdeps, mo, ml, vo, vl, mb, st);
        return a6, b6;
    }

    if k == 1 {
        // $ref
        let rid = refstr[si];
        let rso = sdir[2 * rid + 0];
        let rsl = sdir[2 * rid + 1];
        let ref_text = strs[rso .. rso + rsl];
        var cut: i64 = 0;
        var ci: i64 = 0;

        while ci < ref_text.len {
            if ref_text[ci] == '#' {
                cut = ci + 1;
                ci = ref_text.len;
            } else {
                ci = ci + 1;
            }
        }
        let frag_at = st[1];
        var fp = append_s(tb, frag_at, "ref", 0, 3);
        let frag_len = sanitize_into(ref_text[cut .. ref_text.len], tb, fp);
        st[1] = fp + frag_len;
        let ref_name = tb[frag_at .. frag_at + 3 + frag_len];
        var resolving: bool = false;
        var ri: i64 = 0;

        while ri < st[2] {
            if rref[ri] == rid {
                resolving = true;
            }
            ri = ri + 1;
        }

        if !resolving {
            if find_entry(mo, ml, mb, ref_name, st[0]) < 0 {
                let tgt = target[si];

                if tgt < 0 {
                    st[4] = st[4] + 1;
                    let empty: const u8[:] = "";
                    return add_rule(mo, ml, vo, vl, mb, st, rule_name, empty);
                }
                rref[st[2]] = rid;
                st[2] = st[2] + 1;
                let tn_off, tn_len = visit(
                    tgt,
                    ref_name,
                    kind,
                    pfirst,
                    pcount,
                    cfirst,
                    ccount,
                    children,
                    add_kind,
                    target,
                    refstr,
                    pname,
                    pschema,
                    preq,
                    pdump,
                    strs,
                    sdir,
                    ptname,
                    ptcontent,
                    ptdeps,
                    mo,
                    ml,
                    vo,
                    vl,
                    mb,
                    tb,
                    ob,
                    st,
                    rref);
                st[2] = st[2] - 1;
                return add_rule(mo, ml, vo, vl, mb, st, rule_name, mb[tn_off .. tn_off + tn_len]);
            }
        }
        return add_rule(mo, ml, vo, vl, mb, st, rule_name, ref_name);
    }

    if k == 2 {
        // anyOf: alternatives joined by " | "
        let body_at = st[1];
        var p: i64 = body_at;
        let base = cfirst[si];
        let n = ccount[si];
        var i: i64 = 0;

        while i < n {
            let an_at = st[1];
            var ap = append_s(tb, an_at, name, 0, name.len);

            if name.len == 0 {
                ap = append_s(tb, ap, "alternative-", 0, 12);
            } else {
                ap = append_c(tb, ap, '-');
            }
            ap = append_dec(tb, ap, i);
            st[1] = ap;
            let alt_name = tb[an_at .. an_at + (ap - an_at)];
            let co, cl = visit(
                children[cfirst[si] + i],
                alt_name,
                kind,
                pfirst,
                pcount,
                cfirst,
                ccount,
                children,
                add_kind,
                target,
                refstr,
                pname,
                pschema,
                preq,
                pdump,
                strs,
                sdir,
                ptname,
                ptcontent,
                ptdeps,
                mo,
                ml,
                vo,
                vl,
                mb,
                tb,
                ob,
                st,
                rref);

            if i > 0 {
                p = append_s(tb, p, " | ", 0, 3);
            }
            p = append_s(tb, p, mb, co, cl);
            i = i + 1;
        }
        st[1] = p;
        return add_rule(mo, ml, vo, vl, mb, st, rule_name, tb[body_at .. body_at + (p - body_at)]);
    }
    // object
    let np = pcount[si];

    if np == 0 && add_kind[si] == 1 {
        let oa, ob = add_primitive(1, "object", strs, sdir, ptname, ptcontent, ptdeps, mo, ml, vo, vl, mb, st);
        return add_rule(mo, ml, vo, vl, mb, st, rule_name, mb[oa .. oa + ob]);
    }
    // kv rules for every property
    var kvoff: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var kvlen: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var opt: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var keyname: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var oc: i64 = 0;

    if np > 0 {
        let pbase = pfirst[si];
        var i: i64 = 0;

        while i < np {
            let prop = pbase + i;
            let nsi = pname[prop];
            let no = sdir[2 * nsi + 0];
            let nl = sdir[2 * nsi + 1];
            let pn = strs[no .. no + nl];
            // child visit name: sub_name + property name
            let cn_at = st[1];
            var cp = append_s(tb, cn_at, sub_name, 0, sub_name.len);
            cp = append_s(tb, cp, pn, 0, pn.len);
            st[1] = cp;
            let child_name = tb[cn_at .. cn_at + (cp - cn_at)];
            let co, cl = visit(
                pschema[prop],
                child_name,
                kind,
                pfirst,
                pcount,
                cfirst,
                ccount,
                children,
                add_kind,
                target,
                refstr,
                pname,
                pschema,
                preq,
                pdump,
                strs,
                sdir,
                ptname,
                ptcontent,
                ptdeps,
                mo,
                ml,
                vo,
                vl,
                mb,
                tb,
                ob,
                st,
                rref);
            // kv text: format_literal(dump) + ' space ":" space ' + child
            let kt_at = st[1];
            let dsi = pdump[prop];
            let do0 = sdir[2 * dsi + 0];
            let dl = sdir[2 * dsi + 1];
            var kp = format_literal_at(strs[do0 .. do0 + dl], tb, kt_at);
            kp = append_s(tb, kp, " space \":\" space ", 0, 17);
            kp = append_s(tb, kp, mb, co, cl);
            st[1] = kp;
            // kv rule name: sub_name + property name + "-kv"
            let kn_at = st[1];
            var kn = append_s(tb, kn_at, sub_name, 0, sub_name.len);
            kn = append_s(tb, kn, pn, 0, pn.len);
            kn = append_s(tb, kn, "-kv", 0, 3);
            st[1] = kn;
            let ko, kl = add_rule(
                mo,
                ml,
                vo,
                vl,
                mb,
                st,
                tb[kn_at .. kn_at + (kn - kn_at)],
                tb[kt_at .. kt_at + (kp - kt_at)]);
            kvoff[i] = ko;
            kvlen[i] = kl;
            keyname[i] = nsi;

            if preq[prop] == 0 {
                opt[oc] = i;
                oc = oc + 1;
            }
            i = i + 1;
        }
    }
    // body
    let b_at = st[1];
    var p: i64 = b_at;

    if np == 0 {
        p = append_s(tb, p, "\"{\" space \"}\"", 0, 13);
        st[1] = p;
        return add_rule(mo, ml, vo, vl, mb, st, rule_name, tb[b_at .. b_at + (p - b_at)]);
    }
    p = append_s(tb, p, "\"{\" space ", 0, 10);
    var have_req: bool = false;
    var i2: i64 = 0;

    while i2 < np {
        let prop = pfirst[si] + i2;

        if preq[prop] != 0 {
            if have_req {
                p = append_s(tb, p, " \",\" space ", 0, 11);
            }
            p = append_s(tb, p, mb, kvoff[i2], kvlen[i2]);
            have_req = true;
        }
        i2 = i2 + 1;
    }
    st[1] = p;

    if oc > 0 {
        p = append_s(tb, p, " (", 0, 2);

        if have_req {
            p = append_s(tb, p, " \",\" space ( ", 0, 13);
        }
        // build the native optional chain backwards: alt_i = kv_i [" " rest_i],
        // rest_i = "<name>-<key_i>-rest" ::= chain(i+1)
        var alt_off: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        var alt_len: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        var chain_off: i64 = 0;
        var chain_len: i64 = 0;
        var have_chain: bool = false;
        var ii: i64 = oc - 1;

        while ii >= 0 {
            let pi = opt[ii];
            let kv = mb[kvoff[pi] .. kvoff[pi] + kvlen[pi]];

            if have_chain {
                // rest rule for ii: name + "-" + key + "-rest", content chain(ii+1)
                let k_at = st[6];
                var kp = append_s(ob, k_at, name, 0, name.len);

                if name.len != 0 {
                    kp = append_c(ob, kp, '-');
                }
                let kns = keyname[pi];
                kp = append_s(ob, kp, strs, sdir[2 * kns + 0], sdir[2 * kns + 1]);
                kp = append_s(ob, kp, "-rest", 0, 5);
                st[6] = kp;
                let rn_a, rn_b = add_rule(
                    mo,
                    ml,
                    vo,
                    vl,
                    mb,
                    st,
                    ob[k_at .. k_at + (kp - k_at)],
                    ob[chain_off .. chain_off + chain_len]);
                let a_at = st[6];
                var ap = append_s(ob, a_at, kv, 0, kv.len);
                ap = append_c(ob, ap, ' ');
                ap = append_s(ob, ap, mb, rn_a, rn_b);
                st[6] = ap;
                alt_off[ii] = a_at;
                alt_len[ii] = ap - a_at;
                let c_at = st[6];
                var cp = append_s(ob, c_at, "( \",\" space ", 0, 12);
                cp = append_s(ob, cp, kv, 0, kv.len);
                cp = append_s(ob, cp, " )?", 0, 3);
                cp = append_c(ob, cp, ' ');
                cp = append_s(ob, cp, mb, rn_a, rn_b);
                st[6] = cp;
                chain_off = c_at;
                chain_len = cp - c_at;
            } else {
                let a_at = st[6];
                var ap = append_s(ob, a_at, kv, 0, kv.len);
                st[6] = ap;
                alt_off[ii] = a_at;
                alt_len[ii] = ap - a_at;
                let c_at = st[6];
                var cp = append_s(ob, c_at, "( \",\" space ", 0, 12);
                cp = append_s(ob, cp, kv, 0, kv.len);
                cp = append_s(ob, cp, " )?", 0, 3);
                st[6] = cp;
                chain_off = c_at;
                chain_len = cp - c_at;
                have_chain = true;
            }
            ii = ii - 1;
        }
        var j: i64 = 0;

        while j < oc {
            if j > 0 {
                p = append_s(tb, p, " | ", 0, 3);
            }
            p = append_s(tb, p, ob, alt_off[j], alt_len[j]);
            j = j + 1;
        }

        if have_req {
            p = append_s(tb, p, " )", 0, 2);
        }
        p = append_s(tb, p, " )?", 0, 3);
    }
    p = append_s(tb, p, " space \"}\"", 0, 10);
    st[1] = p;
    return add_rule(mo, ml, vo, vl, mb, st, rule_name, tb[b_at .. b_at + (p - b_at)]);
}
// Entry: visit the root schema under an empty name.

export fn convert(
    root: i64,
    kind: const i64[:],
    pfirst: const i64[:],
    pcount: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: const i64[:],
    add_kind: const i64[:],
    target: const i64[:],
    refstr: const i64[:],
    pname: const i64[:],
    pschema: const i64[:],
    preq: const i64[:],
    pdump: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    ptname: const i64[:],
    ptcontent: const i64[:],
    ptdeps: const i64[:],
    mo: i64[:],
    ml: i64[:],
    vo: i64[:],
    vl: i64[:],
    mb: u8[:],
    tb: u8[:],
    ob: u8[:],
    st: i64[:],
    rref: i64[:],
    space_sid: i64) -> i64 {
    st[0] = 0;
    st[1] = 0;
    st[2] = 0;
    st[3] = 0;
    st[4] = 0;
    st[5] = 0;
    st[6] = 0;
    // the converter starts with the space rule
    let spo = sdir[2 * space_sid + 0];
    let spl = sdir[2 * space_sid + 1];
    let spa, spb = add_rule(mo, ml, vo, vl, mb, st, "space", strs[spo .. spo + spl]);
    let e: const u8[:] = "";
    let no, nl = visit(
        root,
        e,
        kind,
        pfirst,
        pcount,
        cfirst,
        ccount,
        children,
        add_kind,
        target,
        refstr,
        pname,
        pschema,
        preq,
        pdump,
        strs,
        sdir,
        ptname,
        ptcontent,
        ptdeps,
        mo,
        ml,
        vo,
        vl,
        mb,
        tb,
        ob,
        st,
        rref);
    return st[0];
}

