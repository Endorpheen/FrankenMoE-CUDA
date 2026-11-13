// error: cannot call run: it shrinks tape while op is still used, and op may refer into tape
// A recursive walk's match binding holds what the caller's pool holds: here
// a slice into the tape, which the recursive call may shrink (§5.1) while the
// binding is still used. Only the caller knows, and its call is rejected.
enum Op { Inc, Loop { body: Op[:] } }

struct Tape { pos: i64, ops: Op[>..] }

fn grow(t: Tape&) { t.ops.resize(t.ops.len * 2, Op.Inc); }

recursive fn run(program: Op[:], tape: Tape&) {
    for op in program {
        match op {
            Inc => { tape.grow(); },
            Loop l => { while tape.ops.len < 8 { run(l.body, tape); } },
        }
    }
}

fn run_program() -> i64 {
    var tape = Tape { pos: 0, ops: [Op.Inc] };
    var pool: Op[>..] = [];
    pool.push(Op.Loop { body: tape.ops[0..1] });
    let ops = pool[0..1];
    run(ops, tape);
    return tape.ops.len;
}

fn main() {
    print(run_program());
}
