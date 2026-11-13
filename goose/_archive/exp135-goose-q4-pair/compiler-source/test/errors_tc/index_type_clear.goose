// error: cannot clear nodes: an index type names nodes
// Clearing is a shrink. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    nodes.clear();
}
