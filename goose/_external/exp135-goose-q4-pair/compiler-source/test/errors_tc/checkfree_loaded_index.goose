// error: checkfree fn relax keeps a bounds check: adj[k] is read out of an element
// An index read out of an array keeps its check; the error says how to carry it. (docs/design/index_types.md)
var dist: i32[>..] = [];
checkfree fn relax(adj: i32[:], d: i32) {
    for k in adj.len {
        if dist[adj[k]] < 0 { dist[adj[k]] = d; }
    }
}
fn main() {
    dist.push(-1);
    var adj: i32[>..] = [];
    adj.push(0);
    relax(adj, 1);
    print(dist[0]);
}
