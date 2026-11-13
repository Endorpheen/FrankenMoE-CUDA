// error: returning a reference rooted in arr, which dies with this function
// keep binds r to its parameter, which keep's call makes a reference into
// arr: r points into arr, which get's return outlives (§9.2).
var K: i64 = 7;
fn get() -> i64& {
    var arr: i64[>..] = [];
    arr.push(1);
    var r: i64&? = null;
    fn keep(p: i64&) { r .= p; }
    keep(arr[0]);
    guard r else { return K; }
    return r;
}
fn main() { print(get()); }
