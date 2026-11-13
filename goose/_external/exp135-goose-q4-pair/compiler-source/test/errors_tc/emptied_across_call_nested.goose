// error: cannot call prepare, which may shrink pool while s is still used: a reference into it was stored there
// A nested function names its parent's variables, so a call of one may write
// into the part emptied before it, whatever it is handed (§5.1).
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
fn build(t: S&) {
    t.refs = [];
    t.pool = [];
    t.refs.push(t.pool.push(1));
}
fn prepare(s: S&) {
    var t = S { pool: s.pool, refs: [] };
    fn keep() { s.refs.push(s.pool[0]); }
    s.refs = [];
    keep();
    build(t);
}
fn main() {
    var pool: i64[>..] = [1];
    var s = S { pool: pool, refs: [] };
    prepare(s);
    print(s.refs.len);
}
