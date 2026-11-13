// error: const for variant Some belongs to Item<i64>, not Item<f64>
enum Item<T> { Some { value: T } }
fn main() {
    let item: Item<f64> = Item<f64>.Some { value: 3.5 };
    match item {
        const for <N, V> in variants<Item<i64>> { V payload => print(payload.value), }
    }
}
