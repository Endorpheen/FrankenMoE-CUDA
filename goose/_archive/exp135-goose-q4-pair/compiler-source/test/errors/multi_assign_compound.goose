// error: several targets take a call's results with `=` alone, not `+=`
fn two() -> i64, i64 { return 1, 2; }
fn main() {
    var a = 0;
    var b = 0;
    a, b += two();
}
