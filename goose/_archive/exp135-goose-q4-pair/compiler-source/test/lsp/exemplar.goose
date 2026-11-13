// lsp: fixture for test/lsp_protocol.py: exemplar instantiation
// (docs/design/language_server.md §11). Nothing calls the generic
// functions below, so the server checks each once with its type
// parameters bound to opaque types, and answers from that check. Lines
// and columns are 1-based.
import std;

struct Point {
    x: i64,
    y: i64,
}

fn scale(x: f64, by: f64) -> f64 { x * by }

// Scales a length.
fn scale(n: i64, by: i64) -> i64 { n * by }

// Counts the items equal to key, and adds up the rows' scaled lengths.
fn tally<T>(items: T[:], rows: T[][:], key: T) -> i64 {
    var n = 0;
    for item in items {
        if item == key { n += 1; }
    }
    for row in rows {
        n += scale(row.len, 2);
    }
    let twice = key + key;
    n
}

// Untyped parameters: each its own opaque type. An i64 is not one, so the
// exemplar ends at `size`.
fn pick(a, b) -> i64 {
    let same = a == b;
    let size: i64 = a;
    let after = size + 1;
    after
}

fn takes_any<V>(v: V) -> i64 { 1 }

fn lengths<T>(rows: T[][:]) {
    for item in rows {
        let w = item.
    }
}

fn members<U>(value: U) {
    let v = value.
}

fn main() {
    let n: i64 = 2;
    print(scale(n, 3));
}

// lsp: error 45
// lsp: error 50
// lsp: hover 22:12 "let item: T"
// lsp: hover 22:12 "In an exemplar instantiation"
// lsp: hover 22:20 "parameter key: T"
// lsp: hover 25:20 "let row: T[]"
// lsp: hover 25:24 "builtin .len: i64"
// lsp: hover 27:9 "let twice: T"
// lsp: hover 28:5 "var n: i64"
// lsp: hover 34:9 "let same: bool"
// lsp: hover 33:9 "a: type of a"
// lsp: hover 34:16 "parameter a: type of a"
// lsp: hover 36:17 "Not checked: the exemplar instantiation ends before this code."
// lsp: definition 22:20 -> 19:40
// lsp: definition 22:12 -> 21:9
// lsp: definition 25:14 -> 16:4
// lsp: signature 25:29 "fn scale(n: i64, by: i64) -> i64" 1
// lsp: complete 44:22 has len
// lsp: complete 44:22 has takes_any
// lsp: complete 44:22 lacks x
// lsp: complete 44:22 lacks scale
// lsp: complete 49:19 has takes_any
// lsp: complete 49:19 lacks len
// lsp: complete 49:19 lacks x
