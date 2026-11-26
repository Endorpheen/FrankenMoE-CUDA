export fn giga_mtp_align(target: const f32[:], previous: f32[:], aligned: f32[:],
                          tokens: i64) -> i64 {
    let width: i64 = 2560;
    if tokens < 1 || tokens > 4096 || previous.len != width ||
       target.len != tokens * width || aligned.len != target.len { return -1; }
    for i in width { aligned[i] = previous[i]; }
    for i in (tokens - 1) * width { aligned[width + i] = target[i]; }
    let last = (tokens - 1) * width;
    for i in width { previous[i] = target[last + i]; }
    return 0;
}
