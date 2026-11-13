// error: expected identifier in field access, found ';'
// error: expected identifier in field access, found 'let'
// error: expected identifier in field access, found '}'
// A `.` with no name after it is reported once and kept as an
// incomplete Dot (docs/design/language_server.md §5 D); the error the rest
// of its statement then raises is a consequence and is not reported.
struct V { x: i64 }
fn f(v: V) -> i64 {
    let a = v.;
    print(v.
    let b = 2;
    a + b
}
fn main() {
    let v = V { x: 1 };
    print(f(v));
    let w = v.
}
