// error: += cannot update a u32 in nodes
// An update computes a plain integer, which the index type does not take back. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    var w = 0 as NodeId;
    w += 1;
    print(w);
}
