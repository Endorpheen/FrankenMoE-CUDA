// error: cannot call stamp, which grows b: the value assigned to b is still under construction
// A limited array may lie inside the value being built, as tags does in
// b's: a growth of it is one of the storage under construction, though its
// type is not the resizable array's (§1.3(4), §4.4).
struct Box { tags: u8[..4], items: i64[>..] }
var b = Box { tags: [], items: [] };
fn stamp() -> i64 { b.tags.push(1); return 2; }
fn main() {
    b = Box { tags: [], items: [stamp()] };
    print(b.items.len);
}
