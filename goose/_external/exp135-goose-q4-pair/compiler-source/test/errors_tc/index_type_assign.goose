// error: cannot assign nodes: an index type names nodes
// A whole assignment rebuilds the array, which may leave it shorter. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    nodes = [];
}
