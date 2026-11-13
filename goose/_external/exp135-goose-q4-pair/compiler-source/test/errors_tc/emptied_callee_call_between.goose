// error: cannot call run, which may shrink pool while s is still used: a reference into it was stored there
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
// A call between emptying the holder and shrinking the pool may fill the
// holder again (§5.1).
fn keep(s: S&) { s.refs.push(s.pool[0]); }
fn run(s: S&) {
    s.refs = [];
    keep(s);
    s.pool = [];
}
fn main() {
    var pool: i64[>..] = [1];
    var s = S { pool: pool, refs: [] };
    run(s);
    print(s.refs.len);
}
