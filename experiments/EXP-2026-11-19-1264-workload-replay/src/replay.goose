// The replay-side guards of DeterministicWorkload, in native order.
// token(): a replay may not ask for an index beyond the recorded output.
// finish(): the replay must have consumed every route record and emitted exactly the recorded count.
// rule 1 exhaustion in token(), 2 route cursor at finish(), 3 emitted count at finish().
fn wl_replay_token(index: i64, recorded: i64, marker: i64) -> i64 {
    if marker != 214 {
        return -1;
    }
    if index >= recorded {
        return 1;
    }
    return 0;
}

fn wl_replay_finish(route_cursor: i64, routes: i64, emitted: i64, recorded: i64, marker: i64) -> i64 {
    if marker != 215 {
        return -1;
    }
    if route_cursor != routes {
        return 2;
    }
    if emitted != recorded {
        return 3;
    }
    return 0;
}

fn main() {
    if wl_replay_token(0, 1, 214) < 0 {
        return;
    }
    if wl_replay_finish(1, 1, 2, 2, 215) < 0 {
        return;
    }
}
