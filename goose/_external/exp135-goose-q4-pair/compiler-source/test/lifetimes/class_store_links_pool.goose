// A callee storing references into a pool its struct parameter refers to,
// linked to each other within the pool, stores references rooted at that
// pool and at nothing else the struct refers to (§9.2): a `Link&` can lie
// in no `u8[>..]`, so the program text the struct also refers to, a local
// of the caller, need not outlive the pool.
struct Link { next: Link&<u32>?, value: i64 }
struct Ast { text: u8[>..]&, nodes: Link[>..]&, expressions: (Link&)[>..] }

fn parse(input: const u8[:], nodes: Link[>..]&) -> (Link&)[>..] {
    var e: (Link&)[>..] = [];
    let a: Link& = nodes.push(Link { value: input.len });
    e.push(nodes.push(Link { next: a, value: 1 }));
    return e;
}

fn run(s: Ast&) { s.expressions = parse(s.text, s.nodes); }

fn prepare(nodes: Link[>..]&) -> i64 {
    var text: u8[>..] = ['a', 'b', 'c'];
    var ast = Ast { text: text, nodes: nodes, expressions: [] };
    run(ast);
    var total = 0;
    for x in ast.expressions {
        total += x.value;
        let n = x.next;
        if n { total += n.value; }
    }
    return total;
}

fn main() {
    var nodes: Link[>..] = [];
    print(prepare(nodes));
    print(nodes.len);
}
