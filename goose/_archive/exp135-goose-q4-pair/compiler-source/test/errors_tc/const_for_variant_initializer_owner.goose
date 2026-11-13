// error: const for accessor belongs to Item<i64>.Some, not Item<f64>.Some
enum Item<T> { Some { value: T } }
fn main() {
    let item = Item<f64>.Some {
        const for <N, A> in fields<Item<i64>.Some> { A: 3.5, }
    };
    print(item.value);
}
