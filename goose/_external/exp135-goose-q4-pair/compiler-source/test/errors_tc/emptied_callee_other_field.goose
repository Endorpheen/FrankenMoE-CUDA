// error: cannot call run, which may shrink pool while s is still used: a reference into it was stored there
// A callee emptying one field of its struct parameter before the pool it
// refers to leaves what the caller stored into another (§5.1).
struct H { r: (i64&)? }
struct S { pool: i64[>..]&, h: H, refs: (i64&)[>..] }
fn run(s: S&) {
    s.refs = [];
    s.pool = [];
}
fn main() {
    var pool: i64[>..] = [1];
    var s = S { pool: pool, h: H {}, refs: [] };
    s.h = H { r: pool[0] };
    run(s);
    print(s.refs.len);
}
