// `a, b = f();` assigns a call's results to places that exist (§7.1): the
// call runs first, then each target is assigned in order, as `a = f()`
// would assign a single result. A non-fixed result is built at its target.
import std;

fn two() -> i64, bool { return 1, true; }
fn parts(n: i64) -> u8[>..], i64 {
    var s: u8[>..] = [];
    for i in n { s.push('a' + i as u8); }
    return s, n * 10;
}
fn swap(a: i64, b: i64) -> i64, i64 { return b, a; }
fn halves(s: const u8[:]) -> const u8[:], const u8[:] { return s[..1], s[1..]; }

struct P { x: i64, name: u8[>..] = [] }

// Through reference parameters, in a generic function.
fn both<T>(v: T) -> T, T { return v, v; }
fn fill<T>(a: T&, b: T&, v: T) { a, b = both(v); }

fn main() {
    var a = 0;
    var ok = false;
    a, ok = two();
    print(a, " ", ok);                      // 1 true

    // A resizable target is rebuilt from the result.
    var s: u8[>..] = [];
    s.append("old");
    var n = 0;
    s, n = parts(3);
    print(s, " ", n);                       // abc 30

    // Elements and fields; the targets' indices are read after the call.
    var xs = [0, 0, 0];
    var i = 1;
    xs[i], xs[i + 1] = swap(5, 6);
    print(xs);                              // [0, 6, 5]
    var p = P { x: 0, .. };
    p.name, p.x = parts(2);
    print(p.name, " ", p.x);                // ab 20

    // The arguments are read before any target is written.
    a, i = swap(a, i + 1);
    print(a, " ", i);                       // 2 1

    // Slices.
    let text = "hello";
    var h = text[..0];
    var t = text[..0];
    h, t = halves(text);
    print(h, " ", t);                       // h ello

    var f1 = 0.0;
    var f2 = 0.0;
    fill(f1, f2, 2.5);
    print(f1 + f2);                         // 5.0

    // In a loop, whose body the checker passes over more than once.
    var acc: u8[>..] = [];
    var total = 0;
    for k in 4 {
        var m = 0;
        acc, m = parts(k);
        total += m;
    }
    print(acc, " ", total);                 // abc 60
}
