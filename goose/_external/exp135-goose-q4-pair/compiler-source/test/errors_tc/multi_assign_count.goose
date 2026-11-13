// error: 2 targets need that many values, and the call gives 3
fn three() -> i64, i64, i64 { return 1, 2, 3; }
fn main() {
    var a = 0;
    var b = 0;
    a, b = three();
    print(a, b);
}
