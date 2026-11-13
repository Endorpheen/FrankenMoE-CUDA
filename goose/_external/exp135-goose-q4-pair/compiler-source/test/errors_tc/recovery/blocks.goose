// error: expected ';' in variable declaration, found '}'
// error: expression expected, found ';'
// error: expected '}' in struct literal, found ';'
// error: expression expected, found 'end of file'
// Recovery inside nested blocks: a statement's error ends at the `}`
// that closes its block where no `;` comes first, braces balanced, and
// a block the file ends in ends there (docs/design/language_server.md §5 D).
struct P { x: i64, y: i64 }
fn f(c: bool) -> i64 {
    var n = 0;
    if c {
        let x = 1
    } else {
        n = ;
    }
    let p = P { x: 1, y: { let q = 2; q } ;
    n
}
fn main() {
    print(f(true));
    while true {
        break;
