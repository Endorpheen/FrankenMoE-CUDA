// error: expected a value of type u32 in edges, got u32 in nodes
// The array is part of the type: an index of one array is not one of another. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
var edges: i32[>..] = [];
fn main() {
    nodes.push(1);
    edges.push(1);
    let w = 0 as NodeId;
    let e: u32 in edges = w;
    print(e);
}
