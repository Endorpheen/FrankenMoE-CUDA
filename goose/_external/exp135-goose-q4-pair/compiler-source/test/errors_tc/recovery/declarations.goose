// error: expected ';' in import, found 'struct'
// error: expected '}' in struct declaration, found 'y'
// error: type expected, found ';'
// error: top-level declaration expected, found ')'
// error: type expected, found 'let'
// error: expected ';' in global declaration, found 'fn'
// error: expression expected, found ';'
// A declaration that raises an error is reported and dropped, and the
// parse goes on at the next one, which it finds by the next `;` or the
// matching `}` of a body, or at a keyword only a declaration can start
// with (docs/design/language_server.md §5 D).
import std
struct P { x: i64 y: i64 }
type T = ;
)
fn f(a: i64, b:
let g = 1
fn main() {
    let z = ;
    print(1);
}
