// error: const for variant A belongs to Item, not i64
enum Item { A }
const A = 7;
fn main() {
    match 7 {
        const for <N, V> in variants<Item> { V => print(N), },
        _ => print("other"),
    }
}
