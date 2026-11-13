// Streaming content-provider decisions in Goose, from server-http.cpp
// process_handler_response and server_http_res::is_stream.
//
// Owned decisions:
//   - a response is a stream exactly when a chunk generator is installed;
//   - a streamed response always carries X-Accel-Buffering: no;
//   - per provider call: a non-empty chunk is written first; a failed write stops
//     the provider without finishing the sink; when the generator says there is
//     no next chunk, the sink is finished and the provider stops; otherwise the
//     provider asks for another call.
//
// The sink, the socket and httplib's chunk framing stay native.
extern "fg469_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn put(out: u64, i: i64, v: i64) {
    st_i64(out, (i * (8 as i64)) as u64, v);
}

export fn is_stream(next_present: i64) -> i64 {
    if next_present != 0 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}
// The streaming branch always sets the nginx buffering hint.

export fn accel_header(streaming: i64) -> i64 {
    if streaming != 0 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}
// One provider invocation.
// steps: 1 abort (write failed), 2 write and continue, 3 write and finish,
//        4 continue without data, 5 finish without data.

export fn step(has_next: i64, chunk_len: i64, write_ok: i64, out: u64) -> i64 {
    var action: i64 = 0 as i64;

    if chunk_len > 0 as i64 && write_ok == 0 as i64 {
        action = 1 as i64;
    } else {
        if has_next != 0 as i64 {
            if chunk_len > 0 as i64 {
                action = 2 as i64;
            } else {
                action = 4 as i64;
            }
        } else {
            if chunk_len > 0 as i64 {
                action = 3 as i64;
            } else {
                action = 5 as i64;
            }
        }
    }
    var done: i64 = 0 as i64;

    if action == 3 as i64 || action == 5 as i64 {
        done = 1 as i64;
    }
    var keep_going: i64 = 0 as i64;

    if action == 2 as i64 || action == 4 as i64 {
        keep_going = 1 as i64;
    }
    put(out, 0 as i64, action);
    put(out, 1 as i64, done);
    put(out, 2 as i64, keep_going);
    return action;
}
// on_complete runs for both branches; for the streaming branch it runs after the
// provider reports completion, for the plain branch immediately after the body.

export fn complete_when(streaming: i64, out: u64) -> i64 {
    put(out, 0 as i64, if streaming != 0 as i64 {
        2 as i64
    } else {
        1 as i64
    }
    );
    return 0 as i64;
}

