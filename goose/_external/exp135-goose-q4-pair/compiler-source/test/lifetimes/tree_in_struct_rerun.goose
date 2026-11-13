// A parsed tree kept in a struct and replaced on every run (§5.1): the
// struct refers to the program text and the node pool its caller owns, and
// keeps the expressions in its last field. A run empties the expressions
// before it empties the pool, so nothing it still holds points into what the
// pool frees, and it may run again and again, from a generic driver, a
// warmup taking the struct by reference and the caller itself; prepare
// replaces the text through the reference field, which none of the struct's
// references can point into, and the parser stores links within the pool,
// rooted there rather than at the text.
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

fn warmup(s: Ast&, k: i64) { for i in k { run(s, i); } }

fn execute_with_warmup<P, W, R, C, B>(s: B&, iterations: i64) -> i64 {
    P(s);
    W(s, 2);
    for i in iterations { R(s, i); }
    return C(s);
}

fn main() {
    var text: u8[>..] = [];
    var nodes: Node..[>..] = [];
    var s = new_ast(4, text, nodes);
    print(execute_with_warmup(s, 3, prepare, warmup, run, checksum));
    print(text.len, " ", nodes.len, " ", s.expressions.len);
    run(s, 0);
    print(checksum(s));
}
