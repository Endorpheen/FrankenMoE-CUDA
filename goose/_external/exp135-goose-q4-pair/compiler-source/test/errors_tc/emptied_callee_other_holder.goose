// error: cannot call run, which may shrink pool while s1 is still used: a reference into it was stored there
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
// A callee emptying one struct parameter's references empties no other
// holder referring to the same pool (§5.1).
fn run(a: S&, b: S&) {
    a.refs = [];
    b.pool = [];
}
fn main() {
    var pool: i64[>..] = [];
    var s1 = S { pool: pool, refs: [] };
    var s2 = S { pool: pool, refs: [] };
    s1.refs.push(pool.push(1));
    run(s2, s1);
    print(s2.refs.len + s1.refs.len);
}
