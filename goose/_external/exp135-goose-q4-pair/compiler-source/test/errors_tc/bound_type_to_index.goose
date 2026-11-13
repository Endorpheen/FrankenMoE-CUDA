// error: expected a value of type u32 in nodes, got u32 upto nodes
// A bound may be the length itself, which is no index: converting one checks. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    let b = 1 as (u32 upto nodes);
    let w: NodeId = b;
    print(w);
}
