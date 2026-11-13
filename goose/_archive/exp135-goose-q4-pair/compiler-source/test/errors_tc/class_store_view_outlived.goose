// error: call run stores a reference rooted at text into what its parameter s leads to, which may be words, which text does not outlive
// What a callee stores through the arrays its struct parameter refers to,
// derived from another array it refers to, is rooted at each array of the
// caller's that struct refers to that can hold the stored reference's
// pointee (§9.2): a `u8&` may come out of the local text, which the
// caller's caller's array outlives. The `Link[>..]` beside it cannot hold
// one.
struct Link { next: Link&<u32>?, value: i64 }
struct Ast { text: u8[>..]&, nodes: Link[>..]&, words: (u8&)[>..]& }

fn first(t: u8[>..]&) -> u8& { return t[0]; }

fn run(s: Ast&) { s.words.push(first(s.text)); }

fn prepare(nodes: Link[>..]&, words: (u8&)[>..]&) -> i64 {
    var text: u8[>..] = ['a'];
    var ast = Ast { text: text, nodes: nodes, words: words };
    run(ast);
    return words.len;
}

fn main() {
    var nodes: Link[>..] = [];
    var words: (u8&)[>..] = [];
    print(prepare(nodes, words));
}
