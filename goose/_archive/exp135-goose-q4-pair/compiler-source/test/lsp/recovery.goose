// lsp: fixture for test/lsp_protocol.py: errors in two functions, both
// published, since the checker goes on past an error to the next root
// (docs/design/language_server.md §5 D), and queries that still answer
// from what was checked. Lines and columns are 1-based.
fn first(x: i64) -> i64 {
    let s: i64 = "one";
    x + 1
}

fn second(a: i64) -> i64 {
    let b = a * 2;
    let c: bool = b;
    b
}

fn main() {
    let v = first(3);
    let w = second(v);
    print(v + w);
}

// lsp: error 6
// lsp: error 12
// lsp: hover 11:9 "let b: i64"
// lsp: hover 11:13 "parameter a: i64"
// lsp: hover 17:13 "fn first(x: i64) -> i64"
// lsp: definition 18:13 -> 10:4
// lsp: symbol second function
