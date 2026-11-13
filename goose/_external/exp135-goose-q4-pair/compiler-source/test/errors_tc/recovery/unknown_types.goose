// error: unknown type: Missing
// error: unknown type: lib::Gone
// error: unknown type: Nowhere
// error: type alias cycle involving: Pong
// error: type alias Alias takes no type arguments
// error: struct S has no variable mode
// error: enum Shape has no variant named Hexagon
// error: unknown type: ns::Thing
// A type name that names nothing is reported where it is written, and the
// use stands for a type of its own, so that resolution goes on past it
// (docs/design/language_server.md §5 D): a qualified name anywhere, and
// in a declaration any name that is not one of its type parameters.
// Each is reported once, however many copies of its node there are.
struct S { a: i64, b: Missing, c: const lib::Gone }
enum Shape { Circle { r: i64 }, Square { s: Nowhere[] } }
type Ping = Pong;
type Pong = Ping[];
type Alias = i64;
fn f(x: Alias<i64>, s: S.., h: Shape.Hexagon) -> i64 { 1 }
fn main() {
    let t: ns::Thing = 1;
    print(f(1));
}
