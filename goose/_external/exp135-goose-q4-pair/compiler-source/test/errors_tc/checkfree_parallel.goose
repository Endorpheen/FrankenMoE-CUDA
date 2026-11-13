// error: so `assert(dist.len >= nodes.len)`
// Against another array an index needs the length relation, which the error names. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
checkfree fn read(dist: i32[:], w: NodeId) -> i32 { dist[w] }
fn main() {
    nodes.push(1);
    nodes.push(2);
    var d: i32[>..] = [];
    d.push(1);
    print(read(d, 1 as NodeId));
}
