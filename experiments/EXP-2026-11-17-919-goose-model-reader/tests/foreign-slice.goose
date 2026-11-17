// Capability probe only. No production adapter is defined or called here.
extern "reader_probe_span" fn reader_probe_span() -> const u8[:];
export fn reader_probe_span_check() -> i64 {
    let view = reader_probe_span();
    return view.len;
}
