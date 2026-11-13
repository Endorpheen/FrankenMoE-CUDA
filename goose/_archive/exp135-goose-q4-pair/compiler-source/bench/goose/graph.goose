// Building an adjacency structure incrementally, then traversing it. Goose
// grows one edge pool an edge at a time and links each vertex's edges with
// self-relative 4-byte offsets, because growth never moves anything: one
// pass, 8 bytes per edge, no per-vertex container. The first V pool entries
// are the per-vertex list heads. That is the same-root rule itself (spec
// 3.9), not a rooting workaround: a head *is* a relative reference, and a
// relative reference and the value it points at must lie in one array, so a
// separate head array could only hold plain 8-byte ones.
//
// C++ either uses vector<vector<int>> (an allocation and a 24-byte header per
// vertex), or does the two-pass count-then-fill CSR build, which is what a
// performance-conscious programmer actually writes and is a good deal more
// code for the same result.

import rng;
import std;

let V = 1000000;                         // BENCH_N
let DEG = 8;
let SOURCES = 4;

// Validate destinations on insertion; edges and the queue retain that proof.
var dist: i32[>..] = [];
type Vertex = u32 in dist;
struct Edge { to: Vertex, next: Edge&<u32>? }

checkfree fn bfs(src: Vertex, pool: Edge[>..]&, q: Vertex[>..<]&) -> i64 {
    assert(pool.len >= dist.len);
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
        var cur: Edge? = pool[u].next;
        loop {
            guard cur else { break; }
            let w = cur.to;
            if dist[w] < 0 {
                dist[w] = d + 1;
                q.push(w);
            }
            cur .= cur.next;
        }
    }
    return total;
}

fn main() {
    for v in V { dist.push(-1); }
    let zero = 0 as Vertex;
    // pool[0..V) are the list heads, pool[V..) the edges themselves.
    var pool: Edge[>..] = [];
    for v in V { pool.push(Edge { to: zero }); }
    var r: u64 = 12345;
    for e in V * DEG {
        r = xs_next(r);
        let u = xs_mod(r, V);
        let w = xs_mod(r >> 24, V);
        let ne: Edge& = pool.push(Edge { to: w as Vertex, next: pool[u].next });
        pool[u].next .= ne;
    }
    // BFS from a few sources, walking each vertex's edge chain.
    var q: Vertex[>..<] = [];
    var total = 0;
    var seed: u64 = 999;
    for s in SOURCES {
        seed = xs_next(seed);
        let src = xs_mod(seed, V);
        fill(dist, -1);
        total += bfs(src as Vertex, pool, q);
    }
    print(pool.len);
    print(total);
}
