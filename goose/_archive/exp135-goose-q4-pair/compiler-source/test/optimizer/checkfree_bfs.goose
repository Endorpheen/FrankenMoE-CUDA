// checkfree (§7.14) with index and bound types (§3.11): a CSR graph whose
// edges are validated once, where they enter the program, and a
// breadth-first search the compiler must leave with no bounds check at all.

import std;

struct Node { lo: u32 upto edges, hi: u32 upto edges }

var nodes: Node[>..] = [];
type NodeId = u32 in nodes;
var edges: NodeId[>..] = [];

// Builds the graph from an edge list nothing has validated.
fn build(nv: i64, src: i32[:], dst: i32[:]) {
    let empty = 0 as (u32 upto edges);
    for v in nv { nodes.push(Node { lo: empty, hi: empty }); }
    var first: i64[>..] = [];
    for v in nv + 1 { first.push(0); }
    for e in src.len { first[src[e] + 1]++; }
    for v in nv { first[v + 1] += first[v]; }
    var next: i64[>..] = [];
    for v in nv { next.push(first[v]); }
    let some = 0 as NodeId;
    for e in dst.len { edges.push(some); }
    for e in src.len {
        let s = src[e];
        edges[next[s]] = dst[e] as NodeId;
        next[s]++;
    }
    for v in nv {
        nodes[v].lo = first[v] as (u32 upto edges);
        nodes[v].hi = first[v + 1] as (u32 upto edges);
    }
}

// One step of the walk, inlined into the loop that makes it.
checkfree inline fn visit(dist: i32[>..]&, q: NodeId[>..<]&, w: NodeId, d: i32) {
    if dist[w] < 0 {
        dist[w] = d + 1;
        q.push(w);
    }
}

// The sum of distances from src; dist holds an entry per node.
checkfree fn bfs(dist: i32[>..]&, q: NodeId[>..<]&, src: NodeId) -> i64 {
    assert(dist.len >= nodes.len);
    var total = 0;
    q.clear();
    dist[src] = 0;
    q.push(src);
    var qh = 0;
    while qh < q.len {
        let u = q[qh];
        qh++;
        let d = dist[u];
        total += d;
        let n = nodes[u];
        for k in n.lo..n.hi { visit(dist, q, edges[k], d); }
    }
    return total;
}

fn main() {
    let V = 1000;
    var src: i32[>..] = [];
    var dst: i32[>..] = [];
    var r = 12345;
    for e in V * 8 {
        r = (r * 1103515245 + 12345) % 2147483648;
        src.push(((r >> 16) % V) as i32);
        r = (r * 1103515245 + 12345) % 2147483648;
        dst.push(((r >> 16) % V) as i32);
    }
    build(V, src, dst);
    var dist: i32[>..] = [];
    for v in nodes.len { dist.push(-1); }
    var q: NodeId[>..<] = [];
    var total = 0;
    for s in 4 {
        fill(dist, -1);
        total += bfs(dist, q, (s * 7) as NodeId);
    }
    print(total);
}
