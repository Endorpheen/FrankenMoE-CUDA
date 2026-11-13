// A value that holds no length and no reference changes no length where it
// is stored, through a reference to an element or into a field included; one
// holding a slice changes the length a reference into it measures.
import std;

struct P { x: f64, y: f64 }
struct Q { s: i64[:] }
struct Holder { p: P, xs: f64[>..] }

fn through_ref(ps: P[:], out: f64[:]) {
    assert(out.len == ps.len);
    for i in ps.len {
        let r = &ps[i];                            // bce:elide
        r = P { x: 1.0 + i, y: out[i] };           // bce:elide
        out[i] = r.x;                              // bce:elide
    }
}

fn through_binder(ps: P[:], out: f64[:]) {
    assert(out.len == ps.len);
    for &p, i in ps {
        p = P { x: p.y, y: 3.0 };
        out[i] += p.x;                             // bce:elide
    }
}

fn into_field(h: Holder&, out: f64[:]) {
    assert(out.len == h.xs.len);
    for i in out.len {
        h.p = P { x: out[i], y: 0.0 };             // bce:elide
        out[i] += h.xs[i];                         // bce:elide
    }
}

fn holding_slice(q: Q&, b: i64[:], k: i64) -> i64 {
    let n = q.s.len;
    q = Q { s: b[0..k] };
    if n > 1 { return q.s[1]; }                    // bce:keep
    return 0;
}

fn main() {
    var ps: P[>..] = [P { x: 0.0, y: 5.0 }, P { x: 0.0, y: 6.0 }];
    var out: f64[>..] = [0.0, 0.0];
    through_ref(ps, out);
    through_binder(ps, out);
    var h = Holder { p: P { x: 0.0, y: 0.0 }, xs: [0.5, 0.25] };
    into_field(h, out);
    print(out, " ", h.p.x);
    var b: i64[>..] = [7, 8, 9];
    var q = Q { s: b[..] };
    print(holding_slice(q, b, b.len - 1));
}
