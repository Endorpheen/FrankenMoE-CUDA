// Transport limits and setup decisions in Goose, from server-http.cpp
// (set_read_timeout, set_write_timeout, socket options, the error handler).
//
// Owned decisions:
//   - read and write timeouts come from the parameters, with the write timeout
//     defaulting to the read value;
//   - SO_REUSEADDR is always requested, SO_REUSEPORT only when configured and
//     available, otherwise a warning is reported;
//   - a 404 gets the JSON not_found body; other codes pass through untouched;
//   - no payload maximum is configured, so request size is bounded by the
//     consumer's own checks (route-level field limits and the model's context).
// out is an i64 slice, not an address: a Goose caller writes through a slice it owns,
// and cannot take the address of a local array. The C-host form needed an extern and a
// host binding for a three-word store; a slice needs neither, which is three fewer
// lines of adapter debt.
fn lm_put(out: i64[:], i: i64, v: i64) {
    if i >= 0 && i < out.len {
        out[i] = v;
    }
}
// effective timeout: explicit value, otherwise the default 3600 seconds

export fn timeout_value(explicit: i64, value: i64, out: i64[:]) -> i64 {
    var v: i64 = value;

    if explicit == 0 as i64 {
        v = 3600 as i64;
    }

    if v <= 0 as i64 {
        v = 3600 as i64;
    }
    lm_put(out, 0 as i64, v);
    return v;
}
// socket options: reuseaddr always, reuseport only when asked and supported.
// Writes reuseaddr, reuseport, warn.

export fn socket_opts(reuse_port: i64, supported: i64, out: i64[:]) -> i64 {
    var rp: i64 = 0 as i64;
    var warn: i64 = 0 as i64;

    if reuse_port != 0 as i64 {
        if supported != 0 as i64 {
            rp = 1 as i64;
        } else {
            warn = 1 as i64;
        }
    }
    lm_put(out, 0 as i64, 1 as i64);
    lm_put(out, 1 as i64, rp);
    lm_put(out, 2 as i64, warn);
    return 0 as i64;
}
// error handler: 1 means write the JSON not_found body, 0 means leave the
// response as the handler produced it.

export fn error_404(action_status: i64, out: i64[:]) -> i64 {
    var own: i64 = 0 as i64;

    if action_status == 404 as i64 {
        own = 1 as i64;
    }
    lm_put(out, 0 as i64, own);
    return own;
}

