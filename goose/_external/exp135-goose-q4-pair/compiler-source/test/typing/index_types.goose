// Index and bound types (§3.11): a value of `u32 in nodes` is below
// nodes.len wherever it can be named, so indexing nodes with it needs no
// check, and it passes through declarations, calls, branches, storage and
// generic code as its type.

import std;

struct Node { lo: u32 upto edges, hi: u32 upto edges, weight: i32 }

var nodes: Node[>..] = [];
type NodeId = u32 in nodes;
var edges: NodeId[>..] = [];

enum Link { None, To { n: NodeId } }

fn add_node(w: i32) -> NodeId {
    let empty = 0 as (u32 upto edges);
    nodes.push(Node { lo: empty, hi: empty, weight: w });
    return (nodes.len - 1) as NodeId;            // bce:elide
}

fn heavier(a: NodeId, b: NodeId) -> NodeId {
    if nodes[a].weight >= nodes[b].weight { a } else { b }   // bce:elide
}

fn main() {
    for v in 6 { add_node((v * 7 % 5) as i32); }
    // Edges as a CSR run per node: node v links to v + 1 and v + 2.
    var first: i64[>..] = [];
    for v in nodes.len {
        first.push(edges.len);
        for d in 1..3 {
            if v + d < nodes.len { edges.push((v + d) as NodeId); }
        }
    }
    for v in nodes.len {
        nodes[v].lo = first[v] as (u32 upto edges);
        nodes[v].hi = (if v + 1 < nodes.len { first[v + 1] } else { edges.len }) as (u32 upto edges);
    }
    // A range of bound types gives index-typed binders.
    var total = 0;
    for v in nodes.len {
        let n = nodes[v];
        for k in n.lo..n.hi {
            let w = edges[k];                    // bce:elide
            total += nodes[w].weight;            // bce:elide
        }
    }
    print("total ", total);
    // As a number: comparisons, arithmetic and plain integers.
    let w = edges[2];
    let plain: u32 = w;
    let wide: i64 = w;
    print(w, " ", plain + 1, " ", wide * 2, " ", w == plain);
    // Branches of one array's types keep it; anything else is a number.
    let c = total > 3;
    let j = if c { w } else { edges[0] };
    let mixed = if c { w } else { 0 };
    print(nodes[j].weight, " ", mixed + 1);      // bce:elide
    // Widths of one array join at the wider one.
    let narrow = 3 as (u16 in nodes);
    let joined = if c { narrow } else { w };
    print(nodes[joined].weight);                 // bce:elide
    // Calls, ADT payloads and generic code.
    let h = heavier(w, narrow);
    let link: Link = Link.To { n: h };
    match link {
        None => print("none"),
        To t => print("to ", t.n, " weighs ", nodes[t.n].weight),   // bce:elide
    }
    var order: NodeId[>..] = [];
    for v in nodes.len { order.push(v as NodeId); }   // bce:elide
    sort(order) { a, b => nodes[a].weight < nodes[b].weight };
    for x in order { print(x, " ", nodes[x].weight); }   // bce:elide
    swap_at(order, 0, 5);
    print(order[0], " ", max(order[1], order[2]));
    // A binder's written index type takes parentheses before the loop's `in`.
    var firsts = 0;
    for v: (u32 in nodes) in 3 as (u32 in nodes) { firsts += nodes[v].weight; }
    print("firsts ", firsts);
    // An image of indices is verified against the array when it is read.
    let back, ok = from_bytes<NodeId[>..]>(to_bytes(order));
    print(ok, " ", back.len, " ", back[3]);
    var big: u32[>..] = [];
    big.push(2);
    big.push(9);
    let bad, ok2 = from_bytes<NodeId[>..]>(to_bytes(big));
    print(ok2, " ", bad.len);
}
