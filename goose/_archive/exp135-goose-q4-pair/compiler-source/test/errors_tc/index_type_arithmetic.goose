// error: expected a value of type u32 in nodes, got u32
// Arithmetic computes with the number an index holds, so its result is a plain integer. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
var ids: NodeId[>..] = [];
fn main() {
    nodes.push(1);
    let w = 0 as NodeId;
    ids.push(w + 0);
}
