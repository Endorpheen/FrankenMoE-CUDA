// error: const for expansion exceeds 100000 generated nodes
fn main() {
    match 0 {
        const for <A> in types<i64, i64, i64, i64, i64, i64, i64, i64, i64, i64> {
            const for <B> in types<i64, i64, i64, i64, i64, i64, i64, i64, i64, i64> {
                const for <C> in types<i64, i64, i64, i64, i64, i64, i64, i64, i64, i64> {
                    const for <D> in types<i64, i64, i64, i64, i64, i64, i64, i64, i64, i64> {
                        const for <E> in types<i64, i64, i64, i64, i64, i64, i64, i64, i64, i64> {
                            const for <F> in types<i64, i64, i64, i64, i64, i64, i64, i64, i64, i64> {
                                const for <G> in types<> { 0 => if true { print(0); }, }
                            }
                        }
                    }
                }
            }
        },
        _ => 1,
    }
}
