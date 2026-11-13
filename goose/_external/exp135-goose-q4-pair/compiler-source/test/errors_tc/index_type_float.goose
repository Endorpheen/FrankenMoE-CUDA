// error: only an integer converts to u32 in nodes, not f64
// An index is converted from an integer, never from a float. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    let w = 0.0 as NodeId;
    print(w);
}
