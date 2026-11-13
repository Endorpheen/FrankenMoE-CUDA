// error: checkfree fn sum_all calls get, which is not checkfree
// What a checkfree function calls runs inside it, so it must be checkfree too. (docs/design/index_types.md)
var a: i32[>..] = [];
fn get(i: i64) -> i32 { a[i] }
checkfree fn sum_all(n: i64) -> i64 {
    var t = 0;
    for i in n { t += get(i); }
    return t;
}
fn main() {
    a.push(1);
    print(sum_all(1));
}
