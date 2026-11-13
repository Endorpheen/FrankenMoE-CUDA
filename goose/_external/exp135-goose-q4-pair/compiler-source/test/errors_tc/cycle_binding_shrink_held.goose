// error: cannot call run, which shrinks tape while held is still used: a reference into it was stored there
// A recursive walk's local holder that does hold a slice into the tape stays
// in the way of the recursive call that may shrink the tape (§5.1), though a
// match binding copied out of the sequence parameter beside it does not.
enum Op { Inc, Loop { body: Op[:] } }

struct Tape { pos: i64, ops: Op[>..] }

fn grow(t: Tape&) { t.ops.resize(t.ops.len * 2, Op.Inc); }

recursive fn run(program: Op[:], tape: Tape&) {
    for op in program {
        match op {
            Inc => { tape.grow(); },
            Loop l => {
                let held = Op.Loop { body: tape.ops[0..1] };
                while tape.ops.len < 8 { run(l.body, tape); }
                print(held.body.len);
            },
        }
    }
}

fn main() {
    var tape = Tape { pos: 0, ops: [Op.Inc] };
    var pool: Op[>..] = [];
    pool.push(Op.Inc);
    run(pool[0..1], tape);
    print(tape.ops.len);
}
