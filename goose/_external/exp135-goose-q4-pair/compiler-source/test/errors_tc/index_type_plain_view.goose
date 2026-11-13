// error: expected a value of type u32[:], got (u32 in nodes)[>..]
// A slice of plain integers could have any value written through it. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    var ids: NodeId[>..] = [];
    ids.push(0 as NodeId);
    let s: u32[:] = ids;
    s[0] = 7;
}
