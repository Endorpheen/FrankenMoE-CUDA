// error: an index type is unsigned (u8, u16, u32 or u64), not i32
// An index type is unsigned. (docs/design/index_types.md)
var nodes: i32[>..] = [];
fn main() {
    let w: i32 in nodes = 0 as (u32 in nodes);
    print(w);
}
