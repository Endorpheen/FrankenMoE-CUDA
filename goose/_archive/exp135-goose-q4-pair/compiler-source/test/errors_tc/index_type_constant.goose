// error: expected a value of type u32 in nodes, got i64 constant
// No constant is an index: the array may be empty. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
var ids: NodeId[>..] = [];
fn main() {
    nodes.push(1);
    ids.push(0);
}
