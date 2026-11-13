// error: ++ cannot update a u32 in nodes
// An increment is an update too. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    nodes.push(2);
    var w = 0 as NodeId;
    w++;
    print(w);
}
