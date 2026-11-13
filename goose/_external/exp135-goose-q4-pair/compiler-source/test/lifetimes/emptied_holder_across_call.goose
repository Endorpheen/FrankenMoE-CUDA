// A holder emptied before a call stays empty across it where the callee
// cannot write into the part emptied (§5.1): it is handed only a local
// struct built from the holder's reference fields, whose own references
// lead to no storage of that part's type. So a function may empty its
// struct parameter's references, have a callee rebuild a tree in the pool
// they pointed into, and keep a copy of the result, again and again.
struct S { pool: i64[>..]&, refs: (i64&)[>..] }

fn build(t: S&, i: i64) {
    t.refs = [];
    t.pool = [];
    t.refs.push(t.pool.push(i));
    t.refs.push(t.pool.push(i * 2));
}

fn prepare(s: S&, i: i64) {
    var t = S { pool: s.pool, refs: [] };
    s.refs = [];
    build(t, i);
    s.refs = copy(t.refs);
}

fn main() {
    var pool: i64[>..] = [];
    var s = S { pool: pool, refs: [] };
    for i in 3 { prepare(s, i + 1); }
    print(s.refs.len, " ", s.refs[0], " ", s.refs[1], " ", pool.len);
}
