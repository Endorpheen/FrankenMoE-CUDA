// Google Cloud AIP compatibility decisions in Goose, from server-http.cpp
// (gcp_params, path_to_gcp_format, register_gcp_compat, the predict handler).
//
// Owned decisions:
//   - enabled only when the environment value is exactly "PREDICTION";
//   - an empty or missing route environment value falls back to the default,
//     and a non-empty value without a leading slash gets one;
//   - the AIP port is used instead of the server port when compat is enabled,
//     and that override is reported;
//   - the camelCase alias drops a leading "/v1", drops one more leading slash,
//     stops at ':', and upper-cases the character after '/', '-' or '_';
//   - the predict body must be a JSON object with an "instances" array of at
//     most 128 entries, and every instance must be an object with a string
//     "@requestFormat".
//
// String storage, getenv, stoi and the JSON library stay native.
extern "fg456_ld_u8" fn ld_u8(p: u64, o: u64) -> u32;
extern "fg456_st_u8" fn st_u8(p: u64, o: u64, v: u32);
extern "fg456_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn put(out: u64, i: i64, v: i64) {
    st_i64(out, (i * (8 as i64)) as u64, v);
}
// enabled: 1 only for the exact string "PREDICTION".

export fn mode_enabled(p: u64, n: i64, out: u64) -> i64 {
    var on: i64 = 0 as i64;

    if n == 10 as i64 {
        if (ld_u8(p, 0 as u64) as i64) == 80 as i64 {
            if (ld_u8(p, 1 as u64) as i64) == 82 as i64 {
                if (ld_u8(p, 2 as u64) as i64) == 69 as i64 {
                    if (ld_u8(p, 3 as u64) as i64) == 68 as i64 {
                        if (ld_u8(p, 4 as u64) as i64) == 73 as i64 {
                            if (ld_u8(p, 5 as u64) as i64) == 67 as i64 {
                                if (ld_u8(p, 6 as u64) as i64) == 84 as i64 {
                                    if (ld_u8(p, 7 as u64) as i64) == 73 as i64 {
                                        if (ld_u8(p, 8 as u64) as i64) == 79 as i64 {
                                            if (ld_u8(p, 9 as u64) as i64) == 78 as i64 {
                                                on = 1 as i64;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    put(out, 0 as i64, on);
    return on;
}
// Route resolution: an empty value takes the default, and a non-empty value is
// copied with a leading slash added when missing. Writes the chosen bytes into
// out and returns their length.

export fn route(has_value: i64, val: u64, val_len: i64, dflt: u64, dflt_len: i64, out: u64, cap: i64) -> i64 {
    var src: u64 = dflt;
    var n: i64 = dflt_len;

    if has_value != 0 as i64 {
        if val_len > 0 as i64 {
            src = val;
            n = val_len;
        }
    }
    var o: i64 = 0 as i64;

    if n > 0 as i64 {
        if (ld_u8(src, 0 as u64) as i64) != 47 as i64 {
            if o < cap {
                st_u8(out, 0 as u64, 47 as u32);
            }
            o = o + 1 as i64;
        }
    }
    var i: i64 = 0 as i64;

    while i < n {
        if o < cap {
            st_u8(out, o as u64, ld_u8(src, i as u64));
        }
        o = o + 1 as i64;
        i = i + 1 as i64;
    }
    return o;
}
// Port selection: with compat enabled the AIP port wins and `overridden` says
// whether that changed the server port.

export fn port_select(enabled: i64, server_port: i64, aip_port: i64, out: u64) -> i64 {
    var port: i64 = server_port;
    var overridden: i64 = 0 as i64;

    if enabled != 0 as i64 {
        port = aip_port;

        if server_port != aip_port {
            overridden = 1 as i64;
        }
    }
    put(out, 0 as i64, port);
    put(out, 1 as i64, overridden);
    return port;
}
// path_to_gcp_format: "/v1/chat/completions" -> "chatCompletions".

export fn gcp_alias(p: u64, n: i64, out: u64, cap: i64) -> i64 {
    var start: i64 = 0 as i64;

    if n > 3 as i64 {
        if (ld_u8(p, 0 as u64) as i64) == 47 as i64 {
            if (ld_u8(p, 1 as u64) as i64) == 118 as i64 {
                if (ld_u8(p, 2 as u64) as i64) == 49 as i64 {
                    start = 3 as i64;
                }
            }
        }
    }

    if start < n {
        if (ld_u8(p, start as u64) as i64) == 47 as i64 {
            start = start + 1 as i64;
        }
    }
    var o: i64 = 0 as i64;
    var cap_next: i64 = 0 as i64;
    var stop: i64 = 0 as i64;
    var i: i64 = start;

    while i < n && stop == 0 as i64 {
        let c: i64 = (ld_u8(p, i as u64) as i64);

        if c == 58 as i64 {
            stop = 1 as i64;
        } else {
            if c == 47 as i64 || c == 45 as i64 || c == 95 as i64 {
                cap_next = 1 as i64;
            } else {
                var e: i64 = c;

                if cap_next != 0 as i64 && c >= 97 as i64 && c <= 122 as i64 {
                    e = c - 32 as i64;
                }

                if o < cap {
                    st_u8(out, o as u64, e as u32);
                }
                o = o + 1 as i64;
                cap_next = 0 as i64;
            }
        }
        i = i + 1 as i64;
    }
    return o;
}
// Predict request shape: 0 ok, 1 not an object, 2 missing/!array instances,
// 3 more than 128 instances.

export fn predict_shape(is_object: i64, has_instances_array: i64, count: i64, out: u64) -> i64 {
    var status: i64 = 0 as i64;

    if is_object == 0 as i64 {
        status = 1 as i64;
    } else {
        if has_instances_array == 0 as i64 {
            status = 2 as i64;
        } else {
            if count > 128 as i64 {
                status = 3 as i64;
            }
        }
    }
    put(out, 0 as i64, status);
    return status;
}
// Instance shape: 0 ok, 4 not an object, 5 missing/non-string @requestFormat.

export fn instance_shape(is_object: i64, has_format_string: i64, out: u64) -> i64 {
    var status: i64 = 0 as i64;

    if is_object == 0 as i64 {
        status = 4 as i64;
    } else {
        if has_format_string == 0 as i64 {
            status = 5 as i64;
        }
    }
    put(out, 0 as i64, status);
    return status;
}

