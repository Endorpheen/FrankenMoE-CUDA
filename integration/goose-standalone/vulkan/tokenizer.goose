const TK_STATE_BYTES: u64 = 184;
const TK_HEADER_BYTES: u64 = 135248;
const TK_OFFSETS: u64 = 16;
const TK_LENGTHS: u64 = 32;
const TK_TOKENS: u64 = 48;
const TK_MERGE_OFFSETS: u64 = 64;
const TK_MERGE_LENGTHS: u64 = 80;
const TK_MERGES: u64 = 96;
const TK_ATTR: u64 = 112;
const TK_ORDER: u64 = 128;
const TK_EOG: u64 = 144;
const TK_SPECIAL_COUNT: u64 = 160;
const TK_EOG_COUNT: u64 = 168;
const TK_MODE: u64 = 176;

extern fn fg_vk_tok_fopen(path: u64, mode: u64) -> u64;
extern fn fg_vk_tok_fclose(file: u64) -> i32;
extern fn fg_vk_tok_perror(path: u64);
extern "calloc" fn vk_tok_alloc(count: u64, bytes: u64) -> c_ptr<void>;
extern "free" fn vk_tok_free(pointer: c_ptr<void>);
extern fn gr_meta_int(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice, id: i64, out: VkGgufSlice) -> i64;
extern fn gr_arr_n(state: VkGgufSlice, meta: VkGgufSlice, id: i64) -> i64;
extern fn gr_array_int(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice, id: i64, index: i64, out: VkGgufSlice) -> i64;
extern fn bt_prepare_need(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice, tokens: i64, merges: i64, need: VkGgufSlice) -> i64;
extern fn bt_prepare_from_gguf(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice, tokens: i64, merges: i64,
    blob: VkGgufSlice, offsets: VkGgufSlice, lens: VkGgufSlice, tok: VkGgufSlice,
    merge_offsets: VkGgufSlice, merge_lens: VkGgufSlice, merge: VkGgufSlice, need: VkGgufSlice) -> i64;
extern fn bt_build(blob: VkGgufSlice, offsets: VkGgufSlice, lens: VkGgufSlice, tok: VkGgufSlice,
    merge_offsets: VkGgufSlice, merge_lens: VkGgufSlice, merges: VkGgufSlice, stats: VkGgufSlice) -> i64;
extern fn bt_attrs(blob: VkGgufSlice, offsets: VkGgufSlice, lens: VkGgufSlice, kinds: VkGgufSlice, attrs: VkGgufSlice) -> i64;
extern fn bt_special_order(blob: VkGgufSlice, offsets: VkGgufSlice, lens: VkGgufSlice, attrs: VkGgufSlice, order: VkGgufSlice) -> i64;
extern fn bt_eog_ids(blob: VkGgufSlice, offsets: VkGgufSlice, lens: VkGgufSlice, eos: i64, eom: i64, eot: i64, ids: VkGgufSlice) -> i64;
extern fn bt_is_eog(ids: VkGgufSlice, count: i64, token: i64) -> i64;
extern fn bt_token_to_piece(blob: VkGgufSlice, offsets: VkGgufSlice, lens: VkGgufSlice, attrs: VkGgufSlice,
    token: i64, out: VkGgufSlice, at: i64, trim: i64, special: i64, attr: i64) -> i64;
extern fn bt_tokenize(blob: VkGgufSlice, offsets: VkGgufSlice, lens: VkGgufSlice, tok: VkGgufSlice,
    merge_offsets: VkGgufSlice, merge_lens: VkGgufSlice, merge: VkGgufSlice, order: VkGgufSlice,
    specials: i64, text: VkGgufSlice, ids: VkGgufSlice, special: i64, cpts: VkGgufSlice,
    seg: VkGgufSlice, enc: VkGgufSlice, sym: VkGgufSlice, heap: VkGgufSlice,
    scratch: VkGgufSlice, info: VkGgufSlice, mode: i64) -> i64;

fn vk_tok_address(pointer: c_ptr<const void>) -> u64 {
    var value: u64 = 0;
    vk_gguf_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return value;
}

fn vk_tok_slice(address: u64, len: i64) -> VkGgufSlice {
    return VkGgufSlice { address: address, len: len };
}

fn vk_tok_table(count: i64, size: u64) -> VkGgufSlice {
    let pointer = vk_tok_alloc(count as! u64, size);
    var address: u64 = 0;
    vk_gguf_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return vk_tok_slice(address, count);
}

fn vk_tok_put_slice(at: u64, slice: VkGgufSlice) {
    vk_gguf_write_word(at, slice.address);
    vk_gguf_write_word(at + 8, slice.len as! u64);
}

fn vk_tok_release(address: u64) {
    vk_tok_free(unsafe_c_ptr<c_ptr<void>>(address));
}

fn vk_tok_cstring(text: const u8[:], out: u8[:]) -> u64 {
    for i in text.len { out[i] = text[i]; }
    out[text.len] = 0;
    return vk_tok_address(unsafe_c_address<c_ptr<const void>>(out[0]));
}

fn vk_tok_meta(header: u64, name: const u8[:]) -> i64 {
    var key: u8[96];
    return fg_gguf_metadata(header, vk_tok_cstring(name, key));
}

fn vk_tok_meta_is(header: u64, name: const u8[:], value: const u8[:]) -> bool {
    var key: u8[96];
    var val: u8[32];
    return fg_gguf_metadata_is(header, vk_tok_cstring(name, key), vk_tok_cstring(value, val)) != 0;
}

export fn fg_vk_tokenizer_close(tokenizer: u64) {
    if tokenizer == 0 { return; }
    for i in 10 { vk_tok_release(vk_gguf_read_word(tokenizer + (i as! u64) * 16)); }
    vk_tok_release(tokenizer);
}

fn vk_tok_fill(t: u64, header: u64, tokens: i64, merges: i64, kinds: i64,
        need_values: i64[:], need: VkGgufSlice, eos: i64) -> bool {
    let data = vk_gguf_data(header);
    let state = vk_gguf_state(header);
    let meta = vk_gguf_metadata(header);
    let vocab = need_values[1];
    let blend = need_values[2];
    let blob = vk_tok_table(need_values[0], 1);
    let offsets = vk_tok_table(vocab, 8);
    let lengths = vk_tok_table(vocab, 8);
    let token_table = vk_tok_table(need_values[3], 8);
    let merge_offsets = vk_tok_table(blend, 8);
    let merge_lengths = vk_tok_table(blend, 8);
    let merge_table = vk_tok_table(need_values[4], 8);
    let attributes = vk_tok_table(vocab, 8);
    let order = vk_tok_table(vocab, 8);
    let eog = vk_tok_table(vocab, 8);
    vk_tok_put_slice(t, blob);
    vk_tok_put_slice(t + TK_OFFSETS, offsets);
    vk_tok_put_slice(t + TK_LENGTHS, lengths);
    vk_tok_put_slice(t + TK_TOKENS, token_table);
    vk_tok_put_slice(t + TK_MERGE_OFFSETS, merge_offsets);
    vk_tok_put_slice(t + TK_MERGE_LENGTHS, merge_lengths);
    vk_tok_put_slice(t + TK_MERGES, merge_table);
    vk_tok_put_slice(t + TK_ATTR, attributes);
    vk_tok_put_slice(t + TK_ORDER, order);
    vk_tok_put_slice(t + TK_EOG, eog);
    let types = vk_tok_table(vocab, 8);
    var ok = blob.address != 0 && offsets.address != 0 && lengths.address != 0 && token_table.address != 0 &&
        merge_offsets.address != 0 && merge_lengths.address != 0 && merge_table.address != 0 &&
        attributes.address != 0 && order.address != 0 && eog.address != 0 && types.address != 0;
    if ok {
        ok = bt_prepare_from_gguf(data, state, meta, tokens, merges, blob, offsets, lengths,
            token_table, merge_offsets, merge_lengths, merge_table, need) == 0;
    }
    if ok {
        for i in vocab {
            if gr_array_int(data, state, meta, kinds, i, vk_tok_slice(types.address + (i as! u64) * 8, 1)) != 0 {
                ok = false;
                break;
            }
        }
    }
    if ok {
        var stats: i64[4];
        ok = bt_attrs(blob, offsets, lengths, types, attributes) == 0 &&
            bt_build(blob, offsets, lengths, token_table, merge_offsets, merge_lengths, merge_table,
                vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(stats[0])), 4)) == 0;
    }
    vk_tok_release(types.address);
    if !ok { return false; }
    let specials = bt_special_order(blob, offsets, lengths, attributes, order);
    let stops = bt_eog_ids(blob, offsets, lengths, eos, -1, -1, eog);
    vk_gguf_write_word(t + TK_SPECIAL_COUNT, specials as! u64);
    vk_gguf_write_word(t + TK_EOG_COUNT, stops as! u64);
    return specials >= 0 && stops > 0;
}

fn vk_tok_from_header(header: u64, kind: u32) -> u64 {
    let inkling = kind == 3;
    let qwen35 = kind == 2 || kind == 1;
    let architecture = if inkling { "inkling2_loop" } else {
        if kind == 2 { "qwen35moe" } else { if qwen35 { "qwen35" } else { "qwen2" } }
    };
    let pre = if inkling { "inkling2" } else { if qwen35 { "qwen35" } else { "qwen2" } };
    let vocab: i64 = if inkling { 200058 } else { if qwen35 { 248320 } else { 151936 } };
    if !vk_tok_meta_is(header, "general.architecture", architecture) ||
        !vk_tok_meta_is(header, "tokenizer.ggml.model", "gpt2") ||
        !vk_tok_meta_is(header, "tokenizer.ggml.pre", pre) { return 0; }
    let data = vk_gguf_data(header);
    let state = vk_gguf_state(header);
    let meta = vk_gguf_metadata(header);
    var eos: i64 = 0;
    if gr_meta_int(data, state, meta, vk_tok_meta(header, "tokenizer.ggml.eos_token_id"),
        vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(eos)), 1)) != 0 ||
        eos < 0 || eos >= vocab { return 0; }
    let tokens = vk_tok_meta(header, "tokenizer.ggml.tokens");
    let merges = vk_tok_meta(header, "tokenizer.ggml.merges");
    let kinds = vk_tok_meta(header, "tokenizer.ggml.token_type");
    var sizes: i64[5];
    let need = vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(sizes[0])), 5);
    if bt_prepare_need(data, state, meta, tokens, merges, need) != 0 || sizes[0] <= 0 || sizes[0] > data.len ||
        sizes[1] != vocab || sizes[2] <= 0 || sizes[2] > 1048576 || gr_arr_n(state, meta, kinds) != vocab { return 0; }

    let tokenizer = vk_tok_table(1, TK_STATE_BYTES).address;
    if tokenizer == 0 { return 0; }
    let mode: u64 = if inkling { 2 } else { if qwen35 { 1 } else { 0 } };
    vk_gguf_write_word(tokenizer + TK_MODE, mode);
    if vk_tok_fill(tokenizer, header, tokens, merges, kinds, sizes, need, eos) { return tokenizer; }
    fg_vk_tokenizer_close(tokenizer);
    return 0;
}

export fn fg_vk_model_tokenizer_open(path: u64, kind: u32) -> u64 {
    let mode: u8[3] = [114, 98, 0];
    let file = fg_vk_tok_fopen(path, vk_tok_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if file == 0 { fg_vk_tok_perror(path); return 0; }
    let header = vk_tok_table(1, TK_HEADER_BYTES).address;
    var tokenizer: u64 = 0;
    if header != 0 {
        if fg_gguf_read_header(header, file) >= 0 { tokenizer = vk_tok_from_header(header, kind); }
        vk_tok_release(vk_gguf_read_word(header));
        vk_tok_release(header);
    }
    fg_vk_tok_fclose(file);
    return tokenizer;
}


fn vk_tok_unpack(ids: u64, data: u64, count: i64, vocab: i64) -> i32 {
    if count < 0 { return -1; }
    for i in count {
        let raw = vk_gguf_read_word(data + (i as! u64) * 8);
        if raw >= (vocab as! u64) { return -1; }
        let id = raw as! u32;
        vk_gguf_copy(unsafe_c_ptr<c_ptr<void>>(ids + (i as! u64) * 4), unsafe_c_address<c_ptr<const void>>(id), 4);
    }
    return count as! i32;
}

export fn fg_vk_tokenizer_encode(t: u64, text: u64, bytes: u64, special: i32, ids: u64, capacity: u32) -> i32 {
    if t == 0 || text == 0 || bytes == 0 || bytes > 65536 || ids == 0 || capacity == 0 || capacity > 32768 { return -1; }
    let n = (bytes as! i64) + 1;
    let limits: i64[6] = [capacity as! i64, n, n * 4, n * 24, n * 15 + 40, 2];
    var total: i64 = 0;
    for i in limits.len { total += limits[i]; }
    let words = vk_tok_table(total, 8);
    let encoded = vk_tok_table(n * 8, 1);
    var result: i32 = -1;
    if words.address != 0 && encoded.address != 0 {
        var workspace: VkGgufSlice[6];
        var at: u64 = words.address;
        for i in limits.len {
            workspace[i] = vk_tok_slice(at, limits[i]);
            at += (limits[i] as! u64) * 8;
        }
        var info: i64[8];
        let count = bt_tokenize(vk_gguf_data(t), vk_gguf_data(t + TK_OFFSETS), vk_gguf_data(t + TK_LENGTHS),
            vk_gguf_data(t + TK_TOKENS), vk_gguf_data(t + TK_MERGE_OFFSETS), vk_gguf_data(t + TK_MERGE_LENGTHS),
            vk_gguf_data(t + TK_MERGES), vk_gguf_data(t + TK_ORDER), vk_gguf_read_word(t + TK_SPECIAL_COUNT) as! i64,
            vk_tok_slice(text, bytes as! i64), workspace[0], special as! i64, workspace[1], workspace[2], encoded,
            workspace[3], workspace[4], workspace[5], vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(info[0])), 8),
            vk_gguf_read_word(t + TK_MODE) as! i64);
        if count <= (capacity as! i64) { result = vk_tok_unpack(ids, words.address, count, vk_gguf_data(t + TK_OFFSETS).len); }
    }
    vk_tok_release(encoded.address);
    vk_tok_release(words.address);
    return result;
}

export fn fg_vk_tokenizer_piece(t: u64, token: u32, output: u64, size: u64) -> i32 {
    if t == 0 || output == 0 || size > 2147483647 { return -1; }
    let offsets = vk_gguf_data(t + TK_OFFSETS);
    if (token as! i64) >= offsets.len { return -1; }
    return bt_token_to_piece(vk_gguf_data(t), offsets, vk_gguf_data(t + TK_LENGTHS), vk_gguf_data(t + TK_ATTR),
        token as! i64, vk_tok_slice(output, size as! i64), 0, 0, 1, 0) as! i32;
}

export fn fg_vk_tokenizer_is_eog(t: u64, token: u32) -> i32 {
    if t == 0 { return 0; }
    return if bt_is_eog(vk_gguf_data(t + TK_EOG), vk_gguf_read_word(t + TK_EOG_COUNT) as! i64, token as! i64) != 0 { 1 } else { 0 };
}
