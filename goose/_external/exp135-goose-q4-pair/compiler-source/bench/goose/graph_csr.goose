// The same graph and the same BFS, built as CSR with the two-pass
// count-then-fill instead of one-pass linking. This is the shape a
// performance-conscious C++ programmer writes, and it exists here to separate
// two things the one-pass row conflates: what the language costs, and what
// the data structure costs. Traversal here is a contiguous run per vertex
// rather than a pointer chase, and it needs two passes over the edges plus
// two more arrays to get there.

import rng;
import std;

let V = 1000000;                         // BENCH_N
let DEG = 8;
let SOURCES = 4;

var dist: i32[>..] = [];
type Vertex = u32 in dist;
var out: Vertex[>..] = [];
type EdgeEnd = u32 upto out;

checkfree fn bfs(src: Vertex, start: EdgeEnd[>..]&, q: Vertex[>..<]&) -> i64 {
    assert(start.len > dist.len);
    q.clear();
    dist[src] = 0;
    q.push(src);
    var total = 0;
    var qh = 0;
    while qh < q.len {
        let u = q[qh];
        qh++;
        let d = dist[u];
        total += d;
        for k in start[u]..start[u as i64 + 1] {
            let w = out[k];
            if dist[w] < 0 {
                dist[w] = d + 1;
                q.push(w);
            }
        }
    }
    return total;
}

fn main() {
    let ne = V * DEG;
    for v in V { dist.push(-1); }
    let zero = 0 as Vertex;
    for e in ne { out.push(zero); }
    let no_edges = 0 as EdgeEnd;
    var esrc: i32[>..] = [];
    var edst: Vertex[>..] = [];
    var start: EdgeEnd[>..] = [];
    for v in V + 1 { start.push(no_edges); }
    var r: u64 = 12345;
    for e in ne {
        r = xs_next(r);
        let u = xs_mod(r, V);
        esrc.push(u as i32);
        edst.push(xs_mod(r >> 24, V) as Vertex);
        start[u + 1] = (start[u + 1] + 1) as EdgeEnd;
    }
    // Each prefix is validated once; ranges retain the bound type in BFS.
    for v in V { start[v + 1] = (start[v + 1] + start[v]) as EdgeEnd; }
    var cursor: u32[>..] = [];
    for v in V + 1 { cursor.push(start[v]); }
    for e in ne {
        let s = esrc[e];
        out[cursor[s]] = edst[e];
        cursor[s] = cursor[s] + 1;
    }
    // BFS, identical to graph.goose apart from how the edges are reached.
    var q: Vertex[>..<] = [];
    var total = 0;
    var seed: u64 = 999;
    for s in SOURCES {
        seed = xs_next(seed);
        let src = xs_mod(seed, V);
        fill(dist, -1);
        total += bfs(src as Vertex, start, q);
    }
    print(V + ne);
    print(total);
}
