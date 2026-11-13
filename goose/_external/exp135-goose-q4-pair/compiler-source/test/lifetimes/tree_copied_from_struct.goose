// A tree built by a struct kept in another one (§5.1): prepare makes a
// parser's struct out of its own struct's reference fields, empties its own
// tree, runs the parser's whole run -- which empties the parser's tree and
// the pool both structs refer to, then rebuilds them -- and keeps a copy.
// The call cannot write into the emptied tree, the parser's struct leading
// to no storage of its type, so the tree stays empty across it; the text and
// the pool the run replaces are what the caller's struct refers to, no
// global array of the type.
import std;

enum Node {
    Number { value: i64 },
    Variable { name: u8[] },
    BinaryOp { op: u8, left: Node..&<u32>, right: Node..&<u32> },
    Assignment { name: u8[], expr: Node..&<u32> },
}

struct Ast { n: i64, text: u8[>..]&, nodes: Node..[>..]&, expressions: (Node..&)[>..] }

struct Parser { input: const u8[:], pos: i64 }

fn new_parser(input: const u8[:]) -> Parser { Parser { input: input, pos: 0 } }

fn peek(p: Parser&) -> u8 { if p.pos < p.input.len { p.input[p.pos] } else { 0 } }

fn skip(p: Parser&) { while p.peek() == ' ' { p.pos++; } }

fn parse(p: Parser&, nodes: Node..[>..]&) -> (Node..&)[>..] {
    var e: (Node..&)[>..] = [];
    loop {
        p.skip();
        while p.peek() == '\n' { p.pos++; p.skip(); }
        if p.peek() == 0 { break; }
        e.push(p.parse_line(nodes));
    }
    return e;
}

fn parse_line(p: Parser&, nodes: Node..[>..]&) -> Node..& {
    let start = p.pos;
    while p.peek() >= 'a' && p.peek() <= 'z' { p.pos++; }
    let name = p.input[start..p.pos];
    p.skip();
    p.pos++;   // '='
    let expr: Node..& = p.parse_sum(nodes);
    return nodes.push(Node.Assignment { name: name, expr: expr });
}

recursive fn parse_sum(p: Parser&, nodes: Node..[>..]&) -> Node..& {
    var left: Node..& = p.parse_atom(nodes);
    loop {
        p.skip();
        if p.peek() != '+' && p.peek() != '*' { break; }
        let op = p.peek();
        p.pos++;
        let right: Node..& = p.parse_atom(nodes);
        left .= nodes.push(Node.BinaryOp { op: op, left: left, right: right });
    }
    return left;
}

recursive fn parse_atom(p: Parser&, nodes: Node..[>..]&) -> Node..& {
    p.skip();
    if p.peek() == '(' {
        p.pos++;
        let inner: Node..& = p.parse_sum(nodes);
        p.skip();
        p.pos++;   // ')'
        return inner;
    }
    if p.peek() >= 'a' && p.peek() <= 'z' {
        let start = p.pos;
        while p.peek() >= 'a' && p.peek() <= 'z' { p.pos++; }
        return nodes.push(Node.Variable { name: p.input[start..p.pos] });
    }
    var v = 0;
    while p.peek() >= '0' && p.peek() <= '9' {
        v = v * 10 + (p.peek() - '0');
        p.pos++;
    }
    return nodes.push(Node.Number { value: v });
}

recursive fn sum_numbers(n: Node..&) -> i64 {
    match n {
        Number &x => x.value,
        Variable &_ => 0,
        BinaryOp &b => sum_numbers(b.left) + sum_numbers(b.right),
        Assignment &a => sum_numbers(a.expr),
    }
}

fn new_ast(n: i64, text: u8[>..]&, nodes: Node..[>..]&) -> Ast {
    Ast { n: n, text: text, nodes: nodes, expressions: [] }
}

fn prepare(s: Ast&) {
    var t: u8[>..] = [];
    for i in s.n { format(t, "v = ", i, " + (a * ", i + 1, ")\n"); }
    s.text = copy(t);
}

fn run(s: Ast&, i: i64) {
    s.expressions = [];
    s.nodes = [];
    var p = new_parser(s.text);
    s.expressions = p.parse(s.nodes);
    s.n += i;
}

fn checksum(s: Ast&) -> i64 {
    var total = 0;
    for e in s.expressions { total += sum_numbers(e); }
    return total * 1000 + s.expressions.len * 10 + s.nodes.len;
}

struct Interp { n: i64, text: u8[>..]&, nodes: Node..[>..]&, ast: (Node..&)[>..] }

fn iprepare(s: Interp&) {
    var a = new_ast(s.n, s.text, s.nodes);
    prepare(a);
    s.ast = [];
    run(a, 0);
    s.ast = copy(a.expressions);
}

fn irun(s: Interp&, i: i64) {
    var total = 0;
    for e in s.ast { total += sum_numbers(e); }
    s.n = total;
}

fn execute<P, R, B>(s: B&) {
    fn warmup(t: B&, k: i64) { for i in k { R(t, i); } }
    P(s);
    warmup(s, 2);
}

var banner: u8[>..] = ['o', 'k'];

fn main() {
    let view: const u8[:] = banner[..];
    var text: u8[>..] = [];
    var nodes: Node..[>..] = [];
    var s = Interp { n: 4, text: text, nodes: nodes, ast: [] };
    execute(s, iprepare, irun);
    print(s.n, " ", s.ast.len, " ", nodes.len, " ", text.len, " ", view);
}
