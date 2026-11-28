struct VkGgufSlice { address: u64, len: i64 }

extern fn gr_find_kv(data: VkGgufSlice, state: VkGgufSlice, metadata: VkGgufSlice,
    slots: VkGgufSlice, name: VkGgufSlice) -> i64;
extern fn gr_val_span(state: VkGgufSlice, metadata: VkGgufSlice, index: i64, out: VkGgufSlice) -> i64;
extern fn gr_parse(data: VkGgufSlice, size: i64, state: VkGgufSlice, metadata: VkGgufSlice,
    tensors: VkGgufSlice, meta_slots: VkGgufSlice, tensor_slots: VkGgufSlice) -> i64;
extern fn fg_vk_upload_write(upload: u64, dest: u64, offset: u64, chunk: u64, bytes: u64) -> i32;

extern fn fg_vk_crt_seek(file: u64, offset: i64, whence: i32) -> i32;
extern fn fg_vk_crt_tell(file: u64) -> i64;
extern fn fg_vk_crt_read(buffer: u64, size: u64, count: u64, file: u64) -> u64;
extern fn fg_vk_crt_strlen(text: u64) -> u64;
extern fn fg_vk_crt_memcmp(a: u64, b: u64, count: u64) -> i32;
extern "memset" fn vk_gguf_clear(buffer: c_ptr<void>, value: i32, count: u64) -> c_ptr<void>;
extern "memcpy" fn vk_gguf_copy(buffer: c_ptr<void>, source: c_ptr<const void>, count: u64) -> c_ptr<void>;
extern "malloc" fn vk_gguf_alloc(count: u64) -> c_ptr<void>;

const VK_GGUF_STATE: u64 = 16;
const VK_GGUF_METADATA: u64 = 80;
const VK_GGUF_TENSORS: u64 = 16464;
const VK_GGUF_META_SLOTS: u64 = 114768;
const VK_GGUF_TENSOR_SLOTS: u64 = 118864;
const VK_GGUF_STAGING_SIZE: u64 = 32;
const VK_GGUF_MAX_PREFIX: i64 = 67108864;

fn vk_gguf_read_byte(address: u64) -> u8 {
    var value: u8 = 0;
    vk_gguf_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 1);
    return value;
}

fn vk_gguf_read_word(address: u64) -> u64 {
    var value: u64 = 0;
    vk_gguf_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 8);
    return value;
}

fn vk_gguf_write_word(address: u64, value: u64) {
    vk_gguf_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 8);
}

fn vk_gguf_data(header: u64) -> VkGgufSlice {
    return VkGgufSlice { address: vk_gguf_read_word(header), len: vk_gguf_read_word(header + 8) as! i64 };
}

fn vk_gguf_state(header: u64) -> VkGgufSlice { return VkGgufSlice { address: header + VK_GGUF_STATE, len: 8 }; }
fn vk_gguf_metadata(header: u64) -> VkGgufSlice { return VkGgufSlice { address: header + VK_GGUF_METADATA, len: 2048 }; }
fn vk_gguf_meta_slots(header: u64) -> VkGgufSlice { return VkGgufSlice { address: header + VK_GGUF_META_SLOTS, len: 512 }; }

export fn fg_gguf_metadata(header: u64, name: u64) -> i64 {
    let key = VkGgufSlice { address: name, len: fg_vk_crt_strlen(name) as! i64 };
    return gr_find_kv(vk_gguf_data(header), vk_gguf_state(header), vk_gguf_metadata(header),
        vk_gguf_meta_slots(header), key);
}

export fn fg_gguf_metadata_is(header: u64, name: u64, value: u64) -> i32 {
    var span: i64[4];
    let pointer = unsafe_c_address<c_ptr<const void>>(span[0]);
    var span_address: u64 = 0;
    vk_gguf_copy(unsafe_c_address<c_ptr<void>>(span_address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    let index = fg_gguf_metadata(header, name);
    let out = VkGgufSlice { address: span_address, len: 4 };
    if gr_val_span(vk_gguf_state(header), vk_gguf_metadata(header), index, out) != 0 || span[0] != 8 ||
        (span[2] as! u64) != fg_vk_crt_strlen(value) { return 0; }
    return if fg_vk_crt_memcmp(vk_gguf_read_word(header) + (span[1] as! u64), value, span[2] as! u64) == 0 { 1 } else { 0 };
}

export fn fg_gguf_read_header(header: u64, file: u64) -> i64 {
    if fg_vk_crt_seek(file, 0, 2) != 0 { return -1; }
    let size = fg_vk_crt_tell(file);
    if size < 24 { return -1; }
    let prefix = if size < VK_GGUF_MAX_PREFIX { size } else { VK_GGUF_MAX_PREFIX };
    let buffer = vk_gguf_alloc(prefix as! u64);
    var address: u64 = 0;
    vk_gguf_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(buffer), 8);
    vk_gguf_write_word(header, address);
    vk_gguf_write_word(header + 8, prefix as! u64);
    if address == 0 || fg_vk_crt_seek(file, 0, 0) != 0 ||
        fg_vk_crt_read(address, 1, prefix as! u64, file) != (prefix as! u64) { return -1; }
    let tensors = VkGgufSlice { address: header + VK_GGUF_TENSORS, len: 12288 };
    let slots = VkGgufSlice { address: header + VK_GGUF_TENSOR_SLOTS, len: 2048 };
    let status = gr_parse(vk_gguf_data(header), size, vk_gguf_state(header), vk_gguf_metadata(header),
        tensors, vk_gguf_meta_slots(header), slots);
    return if status == 0 { size } else { -1 };
}

export fn fg_gguf_upload(file: u64, upload: u64, dest: u64, offset: u64, file_offset: u64, bytes: u64, chunk: u64) -> i32 {
    let capacity = vk_gguf_read_word(upload + VK_GGUF_STAGING_SIZE);
    if capacity == 0 || (capacity & 3) != 0 || file_offset > 0x7fffffffffffffff ||
        fg_vk_crt_seek(file, file_offset as! i64, 0) != 0 { return 0; }
    var at: u64 = 0;
    while at < bytes {
        let count = if bytes - at < capacity { bytes - at } else { capacity };
        let padded = (count + 3) & 0xfffffffffffffffc;
        if fg_vk_crt_read(chunk, 1, count, file) != count { return 0; }
        vk_gguf_clear(unsafe_c_ptr<c_ptr<void>>(chunk + count), 0, padded - count);
        if fg_vk_upload_write(upload, dest, offset + at, chunk, padded) == 0 { return 0; }
        at += count;
    }
    return 1;
}
