// error: expected a value of type u32 in nodes, got u32
// A branch of an index and a constant is a plain integer. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
var ids: NodeId[>..] = [];
fn main() {
    nodes.push(1);
    let w = 0 as NodeId;
    let z = if nodes.len > 3 { w } else { 7 };
    ids.push(z);
}
