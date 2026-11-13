// A recursive walk's match binding, copied out of the elements of its
// sequence parameter, holds what the caller's pool holds, which the tape the
// recursive call may shrink (§5.1) is not part of, though pool and tape are
// locals of the same caller: the binding stays usable across the call.
enum Op { Inc, Loop { body: Op[:] } }

struct Tape { pos: i64, cells: i32[>..] }

fn grow(t: Tape&) { t.cells.resize(t.cells.len * 2, 0); }

recursive fn run(program: Op[:], tape: Tape&) {
    for op in program {
        match op {
            Inc => { tape.cells[0] += 1; tape.grow(); },
            Loop l => { while tape.cells[0] < 3 { run(l.body, tape); } },
        }
    }
}

fn run_program() -> i64 {
    var pool: Op[>..] = [];
    pool.push(Op.Inc);
    pool.push(Op.Loop { body: pool[0..1] });
    let ops = pool[1..2];
    var tape = Tape { pos: 0, cells: [0] };
    run(ops, tape);
    return tape.cells[0] * 100 + tape.cells.len;
}

fn main() {
    print(run_program());
}
