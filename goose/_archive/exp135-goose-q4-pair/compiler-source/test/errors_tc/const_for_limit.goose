// error: const for expansion exceeds 100000 generated nodes
fn main() {
    const for <A> in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] {
        const for <B> in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] {
            const for <C> in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] {
                const for <D> in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] {
                    const for <E> in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] { print(E); }
                }
            }
        }
    }
}
