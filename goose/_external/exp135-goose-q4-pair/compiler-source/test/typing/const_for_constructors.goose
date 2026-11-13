struct Point { x: i64, y: i64 }
struct Quad { a: i64, b: i64, c: i64, d: i64 }
struct Empty {}
struct Defaults { x: i64, y: i64 = next(), z: i64 }
struct Packed { n: varint, text: u8[], values: i64[] }
enum Result { Data { x: i64, y: i64 } }
var serial = 0;

fn next() -> i64 { serial++; serial }
fn transform<T, F>(v: T&) -> T {
    T {
        const for <N, A> in fields<T> {
            A: F(N, v.A),
        }
    }
}
fn main() {
    var p = Point { 2, 3 };
    let q = transform(p) { n, v => print(n); v * 10 };
    print(q);
    print(Empty { const for <N, A> in fields<Empty> { A: invalid(N), } });
    print(Point { const for <C> in [5, 6] { C, } });
    print(Quad {
        const for <C> in [1, 2] {
            const for <D> in [C, C + 10] { D, },
        },
    });
    print(Quad {
        8,
        const for <U> in types<i8, i16> { default<U>(), },
        9,
    });
    print(Point { const for <C> in [] { C, }, x: 7, .. });
    print(Defaults { const for <C> in [5] { x: C, }, .. });
    print(serial);
    print(Result.Data { const for <C> in [4, 8] { C, } });
    // Each variable-size field is constructed directly at its destination.
    let packed = Packed {
        n: 300,
        const for <C> in ["abc"] { text: C, },
        values: [1, 2, 3],
    };
    print(packed);
    let effects = Point { const for <N, A> in fields<Point> { A: next(), } };
    print(effects);
}
