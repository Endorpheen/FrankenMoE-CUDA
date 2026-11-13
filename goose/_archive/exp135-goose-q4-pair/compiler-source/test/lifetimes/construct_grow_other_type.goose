// Growth of a resizable array of another type while an element is built in
// place (§1.3(4)): a stack holds one resizable, and resizables are never
// elements (§3.4), so the array grown is not the one built in, though its
// root is inexact (§5.1) and may be any array of its own type. Grown through
// a reference parameter, through a struct's tail, through a by-value
// holder's reference, and while the new contents of a whole assignment are
// built.
struct Log { n: i64, items: i64[>..] }
struct H { r: i64[>..]& }
fn mk(scratch: u8[>..]&) -> u8[] { scratch.push('a'); return "x"; }
fn note(l: Log&) -> u8[] { l.items.push(l.n); return "y"; }
fn held(h: H) -> u8[] { h.r.push(5); return "z"; }
fn count(scratch: u8[>..]&) -> i64 { scratch.push('b'); return scratch.len; }
fn push_one(v: u8[][>..]&, scratch: u8[>..]&) { v.push(mk(scratch)); }
fn push_log(v: u8[][>..]&, l: Log&) { v.push(note(l)); }
fn push_held(v: u8[][>..]&, h: H) { v.push(held(h)); }
fn reset(l: Log&, scratch: u8[>..]&) { l = Log { n: count(scratch), items: [] }; }
var first: u8[>..] = [];
var spill: u8[>..] = [];
var la = Log { n: 1, items: [] };
var lb = Log { n: 2, items: [] };
fn main() {
    var v: u8[][>..] = [];
    let c = v.len == 0;
    push_one(v, if c { first } else { spill });
    push_log(v, if c { la } else { lb });
    var xs: i64[>..] = [];
    var ys: i64[>..] = [];
    push_held(v, H { r: if c { xs } else { ys } });
    for s in v { print(s); }
    print(first.len, " ", spill.len, " ", la.items, " ", lb.items, " ", xs, " ", ys);
    reset(la, if c { first } else { spill });
    print(la.n, " ", la.items.len, " ", first.len);
}
