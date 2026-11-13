enum Choice { A, B }
enum Empty {}
enum Record { Value { n: i64 }, None }
struct EmptyFields {}

fn show<E>(value: E) {
    match value { const for <N, V> in variants<E> { V => if true { print(N); }, } }
}

fn wrapped<E>(value: E) {
    block { {
        let local = value;
        match local {
            const for <T> in types<E> {
                const for <N, V> in variants<T> { V => if true { print(N); }, }
            }
        }
    } }
}

fn mixed(value: Choice) {
    match value { A => 7, const for <T> in types<i64> { B => if true { print("mixed"); }, } }
}

fn run<F>() { F(); }

// Empty fragments cannot turn a value-producing match into a statement.
fn empty_sources<E, S>(n: i64) {
    match n {
        const for <C> in [] { C => if true { print("unreachable"); }, },
        const for <T> in types<> { 0 => if true { print("unreachable"); }, },
        const for <N, V> in variants<E> { V => if true { print("unreachable"); }, },
        const for <N, A> in fields<S> { 0 => if true { print("unreachable"); }, },
        _ => 42,
    }
}

fn nested_fields<E>(n: i64) {
    match n {
        const for <N, V> in variants<E> {
            const for <F, A> in fields<V> { 0 => if true { print(F); }, }
        },
        _ => 7,
    }
}

fn named_fields<E>(n: i64) {
    match n {
        const for <F, A> in fields<E.Value> { 0 => if true { print(F); }, },
        _ => 7,
    }
}

fn valued<E>(value: E) {
    match value { const for <N, V> in variants<E> { V => N, } }
}

// A tail that normal checking never reaches performs no source checks.
fn unreachable_tail() {
    block {
        return;
        match 0 { const for <C> in [unknown] { C => if true { print(C); }, }, _ => 1 }
    }
}

fn main() {
    show(Choice.A);
    wrapped(Choice.B);
    mixed(Choice.B);
    run() { match Choice.A { const for <N, V> in variants<Choice> { V => if true { print(N); }, } } };
    print(empty_sources<Empty, EmptyFields>(0));
    print(nested_fields<Choice>(0));
    nested_fields<Record>(0);
    named_fields<Record>(0);
    print(valued(Choice.B));
    unreachable_tail();
}
