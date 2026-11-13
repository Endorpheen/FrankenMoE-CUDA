// error: cannot assign pool while s is still used: a reference into it was stored there
// A holder's own references moved into the part emptied, a store no event
// records, fill it again (§5.1).
struct H { r: (i64&)? }
struct S { a: H, b: (i64&)[>..] }
fn main() {
    var pool: i64[>..] = [1];
    var s = S { a: H {}, b: [] };
    s.b.push(pool[0]);
    s.a = H {};
    s.a = H { r: s.b[0] };
    s.b = [];
    pool = [];
    print(s.b.len);
}
