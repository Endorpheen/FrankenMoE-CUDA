// error: expected '}' in struct declaration, found 'y'
// error: expected '}' in enum variant, found 'w'
// error: expected identifier in parameter list, found ':'
// error: type expected, found '{'
// error: expected identifier in struct body, found 'fn'
// error: expected ')' in function declaration, found 'i64'
// error: expected ';' in variable declaration, found '}'
// A declaration with an error past its name stands with what was parsed
// before the error (docs/design/language_server.md §5 D): a struct or an
// enum skips to the `}` closing its body, braces balanced, or to a
// keyword only a declaration starts with where the body lacks its `}`,
// and a function keeps its name with no body. Nothing after them is lost.
struct P { x: i64 y: i64 }
enum E { A, B { v: i64 w }, C }
fn f(a: i64, : i64) -> i64 { a }
fn g(a: i64) -> { a }
struct Q { a: i64,
fn h() -> i64 { 1 }
extern fn ext(a: i64, b: i64 i64;
fn main() {
    let p = P { x: 1 }
}
