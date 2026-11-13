// SSE framing in Goose, from tools/server/server-common.cpp (format_oai_sse,
// format_oai_resp_sse, format_anthropic_sse) and server-context.cpp (the
// "[DONE]" terminator and the ":\n\n" keep-alive ping).
//
// Owned decisions: which parts a frame contains, in which order, and the exact
// byte length. The literals ("data: ", "event: ", newlines, "[DONE]", ":") and
// the JSON serialisation stay native; the wrapper expands the step plan.
//
// Flavors: 1 OpenAI chat, 2 OpenAI responses, 3 Anthropic, 4 terminator,
// 5 keep-alive.
// Steps: 1 "data: ", 2 payload, 3 "event: ", 4 event name, 5 newline,
// 7 "[DONE]", 8 ":".
extern "fg455_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn put(out: u64, i: i64, v: i64) {
    st_i64(out, (i * (8 as i64)) as u64, v);
}
// Writes the step count followed by the steps.

export fn frame(flavor: i64, has_event: i64, out: u64) -> i64 {
    var n: i64 = 0 as i64;

    if flavor == 1 as i64 {
        put(out, 1 as i64, 1 as i64);
        put(out, 2 as i64, 2 as i64);
        put(out, 3 as i64, 5 as i64);
        put(out, 4 as i64, 5 as i64);
        n = 4 as i64;
    } else {
        if flavor == 2 as i64 {
            put(out, 1 as i64, 3 as i64);
            put(out, 2 as i64, 4 as i64);
            put(out, 3 as i64, 5 as i64);
            put(out, 4 as i64, 1 as i64);
            put(out, 5 as i64, 2 as i64);
            put(out, 6 as i64, 5 as i64);
            put(out, 7 as i64, 5 as i64);
            n = 7 as i64;
        } else {
            if flavor == 3 as i64 {
                if has_event != 0 as i64 {
                    put(out, 1 as i64, 3 as i64);
                    put(out, 2 as i64, 4 as i64);
                    put(out, 3 as i64, 5 as i64);
                    put(out, 4 as i64, 1 as i64);
                    put(out, 5 as i64, 2 as i64);
                    put(out, 6 as i64, 5 as i64);
                    put(out, 7 as i64, 5 as i64);
                    n = 7 as i64;
                } else {
                    put(out, 1 as i64, 1 as i64);
                    put(out, 2 as i64, 2 as i64);
                    put(out, 3 as i64, 5 as i64);
                    put(out, 4 as i64, 5 as i64);
                    n = 4 as i64;
                }
            } else {
                if flavor == 4 as i64 {
                    put(out, 1 as i64, 1 as i64);
                    put(out, 2 as i64, 7 as i64);
                    put(out, 3 as i64, 5 as i64);
                    put(out, 4 as i64, 5 as i64);
                    n = 4 as i64;
                } else {
                    put(out, 1 as i64, 8 as i64);
                    put(out, 2 as i64, 5 as i64);
                    put(out, 3 as i64, 5 as i64);
                    n = 3 as i64;
                }
            }
        }
    }
    put(out, 0 as i64, n);
    return n;
}
// Total byte length of a frame: literals plus the payload and event-name bytes.

export fn frame_size(steps: i64[:], payload: i64, event: i64, out: u64) -> i64 {
    var total: i64 = 0 as i64;
    var i: i64 = 0 as i64;

    while i < steps.len {
        let s: i64 = steps[i];

        if s == 1 as i64 {
            total = total + 6 as i64;
        }
        // "data: "

        if s == 2 as i64 {
            total = total + payload;
        }

        if s == 3 as i64 {
            total = total + 7 as i64;
        }
        // "event: "

        if s == 4 as i64 {
            total = total + event;
        }

        if s == 5 as i64 {
            total = total + 1 as i64;
        }
        // newline

        if s == 7 as i64 {
            total = total + 6 as i64;
        }
        // "[DONE]"

        if s == 8 as i64 {
            total = total + 1 as i64;
        }
        // ":"
        i = i + 1 as i64;
    }
    put(out, 0 as i64, total);
    return total;
}

