// error: the array of an index type must be a grow-only resizable global var
// Only a grow-only array can promise its length never goes down. (docs/design/index_types.md)
var nodes: i32[>..<] = [];
fn main() {
    nodes.push(1);
    let w = 0 as (u32 in nodes);
    print(w);
}
