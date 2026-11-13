// error: an index type takes no postfix: parenthesize it, as in (u32 in nodes)[
// `u32 in nodes[>..]` would read as an index into `nodes[>..]`. (docs/design/index_types.md)
var nodes: i32[>..] = [];
var ids: u32 in nodes[>..] = [];
fn main() {}
