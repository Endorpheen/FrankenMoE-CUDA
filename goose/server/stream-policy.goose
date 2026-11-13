// Stream session buffer policy in Goose, from tools/server/server-stream.cpp
// (stream_session::append and stream_session::read_from).
//
// Owned decisions:
//   1. retention: a session keeps at most `cap` bytes of the newest stream data.
//      A chunk larger than the cap keeps only its tail; an overflow drops the
//      oldest bytes and counts them in the dropped prefix; an empty chunk is a
//      no-op; a finalized session accepts nothing.
//   2. read decision: stop request wins, then an offset below the dropped prefix
//      is OFFSET_LOST, then the available tail is returned as one chunk, then a
//      finalized session ends the read.
//
// Mutexes, condition variables and byte copies stay native; this module owns the
// arithmetic and the status selection.
extern "fg453_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn put(out: u64, i: i64, v: i64) {
    st_i64(out, (i * (8 as i64)) as u64, v);
}
// append(done, buf_len, len, cap) -> accept, new_buf_len, dropped_delta, src_skip

export fn append(done: i64, buf_len: i64, len: i64, cap: i64, out: u64) -> i64 {
    var accept: i64 = 0 as i64;
    var nbuf: i64 = buf_len;
    var delta: i64 = 0 as i64;
    var skip: i64 = 0 as i64;
    // native returns true for an empty chunk before it looks at `done`

    if len == 0 as i64 {
        accept = 1 as i64;
    } else {
        if done == 0 as i64 {
            accept = 1 as i64;

            if len >= cap {
                skip = len - cap;
                delta = buf_len + skip;
                nbuf = cap;
            } else {
                let needed: i64 = buf_len + len;

                if needed > cap {
                    delta = needed - cap;
                    nbuf = cap;
                } else {
                    nbuf = needed;
                }
            }
        }
    }
    put(out, 0 as i64, accept);
    put(out, 1 as i64, nbuf);
    put(out, 2 as i64, delta);
    put(out, 3 as i64, skip);
    return 0 as i64;
}
// read_decision(offset, dropped, buf_len, done, stop)
// -> status (0 wait, 1 chunk, 2 offset lost, 3 end, 4 stopped), bytes available

export fn read_decision(offset: i64, dropped: i64, buf_len: i64, done: i64, stop: i64, out: u64) -> i64 {
    var status: i64 = 0 as i64;
    var n: i64 = 0 as i64;

    if stop != 0 as i64 {
        status = 4 as i64;
    } else {
        if offset < dropped {
            status = 2 as i64;
        } else {
            let logical_end: i64 = dropped + buf_len;

            if offset < logical_end {
                status = 1 as i64;
                n = logical_end - offset;
            } else {
                if done != 0 as i64 {
                    status = 3 as i64;
                }
            }
        }
    }
    put(out, 0 as i64, status);
    put(out, 1 as i64, n);
    return 0 as i64;
}

