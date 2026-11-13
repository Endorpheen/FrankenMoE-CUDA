// /props payload structure in Goose, from server-context.cpp (get_res_props,
// get_props, post_props).
//
// Owned decisions:
//   - the ordered key list of the payload, and the one conditional key
//     ("connect_code") that appears only when the tunnel has a code;
//   - which source answers GET /props: the cached snapshot while the server
//     sleeps, the live structure otherwise;
//   - POST /props is rejected with not_supported when the endpoint is disabled.
//
// The values themselves (model metadata, templates, sampler params) stay native.
extern "fg457_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn put(out: u64, i: i64, v: i64) {
    st_i64(out, (i * (8 as i64)) as u64, v);
}
// Key ids follow the native initializer order:
// 1 default_generation_settings, 2 total_slots, 3 model_alias, 4 model_ftype,
// 5 model_path, 6 modalities, 7 media_marker, 8 endpoint_slots,
// 9 endpoint_props, 10 endpoint_metrics, 11 ui, 12 ui_settings,
// 13 chat_template, 14 chat_template_caps, 15 bos_token, 16 eos_token,
// 17 build_info, 18 is_sleeping, 19 cors_proxy_enabled, 20 connect_code.

export fn props_keys(has_connect_code: i64, out: u64) -> i64 {
    var i: i64 = 1 as i64;

    while i <= 19 {
        put(out, i - 1 as i64, i);
        i = i + 1 as i64;
    }
    var n: i64 = 19 as i64;

    if has_connect_code != 0 as i64 {
        put(out, 19 as i64, 20 as i64);
        n = 20 as i64;
    }
    return n;
}
// 1 cached snapshot, 2 live structure.

export fn props_mode(is_sleeping: i64, out: u64) -> i64 {
    var mode: i64 = 2 as i64;

    if is_sleeping != 0 as i64 {
        mode = 1 as i64;
    }
    put(out, 0 as i64, mode);
    return mode;
}
// 0 handled, 1 not supported (501) when POST /props is disabled.

export fn post_props_decision(endpoint_props: i64, out: u64) -> i64 {
    var status: i64 = 0 as i64;

    if endpoint_props == 0 as i64 {
        status = 1 as i64;
    }
    put(out, 0 as i64, status);
    return status;
}

