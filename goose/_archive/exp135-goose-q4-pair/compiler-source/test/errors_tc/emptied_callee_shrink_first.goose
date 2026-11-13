// error: cannot assign s while s is still used: what it refers to may hold a reference or slice into it
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
// A pool shrunk before the holder is emptied in a loop's body frees what
// the previous iteration's references point at (§5.1).
fn run(s: S&) {
    for i in 3 {
        s.pool = [];
        s.refs = [];
        s.refs.push(s.pool.push(i));
    }
}
fn main() {
    var pool: i64[>..] = [];
    var s = S { pool: pool, refs: [] };
    run(s);
    print(s.refs.len);
}
