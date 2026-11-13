// error: cannot assign pool while s is still used: a reference into it was stored there
// Emptying one field of a holder leaves what its other fields hold (§5.1).
struct H { r: (i64&)? }
struct S { a: H, b: (i64&)[>..] }
fn main() {
    var pool: i64[>..] = [1];
    var s = S { a: H {}, b: [] };
    s.b.push(pool[0]);
    s.a = H {};
    pool = [];
    print(s.b.len);
}
