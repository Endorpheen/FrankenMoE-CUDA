// error: cannot call reset, which shrinks nodes: an index type names nodes
// A shrink through a reference parameter reaches the array at the call. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn reset(a: i32[>..]&) { a.clear(); }
fn main() {
    nodes.push(1);
    reset(nodes);
}
