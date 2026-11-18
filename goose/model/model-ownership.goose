// Linux x86_64 little-endian GGML POD ABI: native init params occupy24 bytes.
// The third word encodes bool at byte16 plus seven zero padding bytes. This
// avoids a new native adapter; the ABI gate verifies the actual native layout.
struct QOwnerParams { bytes: u64, buffer: u64, no_alloc_word: u64 }
extern "ggml_init" fn qo_native_init(params: QOwnerParams) -> i64;
extern "ggml_free" fn qo_native_free(c: i64);
extern "ggml_set_zero" fn qo_native_zero(tensor: i64) -> i64;
// Reuse the existing shared host byte-store primitive. No new native body.
extern "qz_st_u8" fn qo_store_u8(address: u64, value: i64);

export fn qo_context_new(bytes: i64, no_alloc: i64) -> i64 {
    if bytes <= 0 || bytes > 2147483648 || (no_alloc != 0 && no_alloc != 1) { return 0; }
    return qo_native_init(QOwnerParams { bytes: bytes as u64, buffer: 0, no_alloc_word: no_alloc as u64 });
}

export fn qo_context_free(c: i64) {
    if c != 0 { qo_native_free(c); }
}

export fn qo_tensor_zero(tensor: i64) -> i64 {
    if tensor == 0 { return 0; }
    return qo_native_zero(tensor);
}
