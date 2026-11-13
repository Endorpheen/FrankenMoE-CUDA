// error: cannot call prepare, which may shrink pool while s is still used: a reference into it was stored there
// A callee handed a block may run it, and the block may write into the part
// emptied before the call (§5.1).
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
fn build(t: S&) {
    t.refs = [];
    t.pool = [];
    t.refs.push(t.pool.push(1));
}
fn twice<F>(t: S&) { F(); }
fn prepare(s: S&) {
    var t = S { pool: s.pool, refs: [] };
    s.refs = [];
    twice(t) { s.refs.push(s.pool[0]); }
    build(t);
}
fn main() {
    var pool: i64[>..] = [1];
    var s = S { pool: pool, refs: [] };
    prepare(s);
    print(s.refs.len);
}
