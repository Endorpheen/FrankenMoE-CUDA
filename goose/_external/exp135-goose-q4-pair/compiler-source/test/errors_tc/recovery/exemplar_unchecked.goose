// error: expected a value of type i64, got bool
// A generic function nothing calls is not checked, so a body that no
// binding of its parameters' types could make right is no error of the
// program's: only main's is reported. The language server checks such a
// function once with opaque bindings (docs/design/language_server.md §11),
// silently, and reports nothing of it either (test/lsp_protocol.py opens
// this file); its hover answers from what that check annotated.
fn never<T>(value: T) -> i64 {
    let kept = value;
    let n: i64 = "text";
    n + kept
}

fn never_untyped(a, b) {
    let s = a + b;
    s.missing(nothere);
}

fn main() {
    let x: i64 = true;
    print(x);
}

// lsp: error 20
// lsp: hover 9:9 "let kept: T"
// lsp: hover 15:9 "let s: type of a"
