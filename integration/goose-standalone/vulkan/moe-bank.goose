const BANK_SIZE: u64 = 4728;
const BANK_DEVICE: u64 = 72;
const BANK_HOST: u64 = 104;
const BANK_PLANES: u64 = 112;
const BANK_SLOTS: u64 = 4720;
const BANK_LOADED: u64 = 4724;
const BANK_ALIGNMENT: u64 = 664;
const BANK_MAX_RANGE: u64 = 364;
const BANK_MAX_U64: u64 = 0xffffffffffffffff;

struct BankView { buffer: u64, offset: u64, size: u64 }
struct BankShape {
    width: u32, rows: u32, batch: u32, epsilon: f32, position: u32,
    kv_heads: u32, context: u32, theta: f32, rotary: u32
}

extern "memcpy" fn bank_copy(dst: c_ptr<void>, src: c_ptr<const void>, bytes: u64) -> c_ptr<void>;
extern "memset" fn bank_fill(dst: c_ptr<void>, value: i32, bytes: u64) -> c_ptr<void>;
extern "calloc" fn bank_alloc(count: u64, size: u64) -> c_ptr<void>;
extern "free" fn bank_free(pointer: c_ptr<void>);
extern "gs_os_write_stderr" fn bank_error(message: const u8[:]);
extern fn fg_vk_bank_seek(file: u64, offset: i64, mode: i32) -> i32;
extern fn fg_vk_bank_read(buffer: u64, size: u64, count: u64, file: u64) -> u64;
extern fn fg_vk_alloc_buffer(vk: u64, buffer: u64, size: u64, usage: u32, required: u32, preferred: u32) -> i32;
extern fn fg_vk_free_buffer(vk: u64, buffer: u64);
extern fn fg_vk_upload_open(upload: u64, vk: u64, capacity: u64) -> i32;
extern fn fg_vk_upload_close(upload: u64);
extern fn fg_vk_upload_write(upload: u64, dest: u64, offset: u64, data: u64, size: u64) -> i32;

fn bank_word(address: u64) -> u64 {
    var value: u64 = 0;
    bank_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 8);
    return value;
}

fn bank_read_u32(address: u64) -> u32 {
    var value: u32 = 0;
    bank_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 4);
    return value;
}

fn bank_write_u32(address: u64, value: u32) {
    bank_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 4);
}

fn bank_view(at: u64) -> BankView {
    return BankView { buffer: bank_word(at), offset: bank_word(at + 8), size: bank_word(at + 16) };
}

fn bank_fill_view(at: u64, view: BankView) {
    let words: u64[3] = [view.buffer, view.offset, view.size];
    bank_copy(unsafe_c_ptr<c_ptr<void>>(at), unsafe_c_address<c_ptr<const void>>(words[0]), 24);
}

fn bank_plane(bank: u64, slot: i64, plane: i64) -> u64 {
    return bank + BANK_PLANES + ((slot * 3 + plane) as! u64) * 24;
}

export fn fg_vk_expert_bank_close(bank: u64) {
    let vk = bank_word(bank);
    if vk != 0 { fg_vk_free_buffer(vk, bank + BANK_DEVICE); }
    fg_vk_upload_close(bank);
    bank_free(unsafe_c_ptr<c_ptr<void>>(bank_word(bank + BANK_HOST)));
    bank_fill(unsafe_c_ptr<c_ptr<void>>(bank), 0, BANK_SIZE);
}

export fn fg_vk_expert_bank_open(bank: u64, vk: u64, slots: u32, max_bytes: u64) -> i32 {
    bank_fill(unsafe_c_ptr<c_ptr<void>>(bank), 0, BANK_SIZE);
    if slots == 0 || slots > 64 { return 0; }
    let alignment = bank_word(vk + BANK_ALIGNMENT);
    let limit = bank_read_u32(vk + BANK_MAX_RANGE) as! u64;
    var end: u64 = 0;
    for slot in (slots as! i64) {
        for plane in 3 {
            let bytes = bank_word(max_bytes + (plane as! u64) * 8);
            if bytes == 0 { continue; }
            if bytes > limit { return 0; }
            let size = (bytes + 3) & 0xfffffffffffffffc;
            if size > limit { return 0; }
            let at = (end + alignment - 1) & (0 - alignment);
            bank_fill_view(bank_plane(bank, slot, plane), BankView { buffer: bank + BANK_DEVICE, offset: at, size: size });
            end = at + size;
        }
    }
    if end == 0 || fg_vk_upload_open(bank, vk, end) == 0 { return 0; }
    let host = bank_alloc(1, end);
    bank_copy(unsafe_c_ptr<c_ptr<void>>(bank + BANK_HOST), unsafe_c_address<c_ptr<const void>>(host), 8);
    let result = fg_vk_alloc_buffer(vk, bank + BANK_DEVICE, end, 34, 1, 0);
    if result != 0 || host == null {
        fg_vk_expert_bank_close(bank);
        if result != 0 { bank_error(str("Vulkan: allocate Vulkan expert bank (", result, ")\n")); }
        return 0;
    }
    bank_write_u32(bank + BANK_SLOTS, slots);
    return 1;
}

export fn fg_vk_expert_bank_stage(bank: u64, file: u64, file_bytes: u64, experts: u32,
        planes: u64, ids: u64, count: u32) -> i32 {
    bank_write_u32(bank + BANK_LOADED, 0);
    if experts == 0 || count == 0 || count > bank_read_u32(bank + BANK_SLOTS) ||
        file_bytes > 0x7fffffffffffffff { return 0; }
    for plane in 3 {
        let entry = planes + (plane as! u64) * 16;
        let offset = bank_word(entry);
        let bytes = bank_word(entry + 8);
        if offset > file_bytes || bytes > bank_view(bank_plane(bank, 0, plane)).size ||
            bytes > (file_bytes - offset) / (experts as! u64) { return 0; }
    }
    let host = bank_word(bank + BANK_HOST);
    var end: u64 = 0;
    for slot in (count as! i64) {
        let id = bank_read_u32(ids + (slot as! u64) * 4);
        if id >= experts { return 0; }
        for plane in 3 {
            let entry = planes + (plane as! u64) * 16;
            let bytes = bank_word(entry + 8);
            if bytes == 0 { continue; }
            let start = bank_word(entry) + (id as! u64) * bytes;
            let offset = bank_view(bank_plane(bank, slot, plane)).offset;
            let dest = host + offset;
            if fg_vk_bank_seek(file, start as! i64, 0) != 0 || fg_vk_bank_read(dest, 1, bytes, file) != bytes { return 0; }
            let padded = (bytes + 3) & 0xfffffffffffffffc;
            bank_fill(unsafe_c_ptr<c_ptr<void>>(dest + bytes), 0, padded - bytes);
            end = offset + padded;
        }
    }
    if end == 0 || fg_vk_upload_write(bank, bank + BANK_DEVICE, 0, host, end) == 0 { return 0; }
    bank_write_u32(bank + BANK_LOADED, count);
    return 1;
}

fn bank_branches_valid(graph: u64, bank: u64, loaded: u64, row: u64, branches: BankView) -> bool {
    let vk = bank_word(graph);
    return loaded != 0 && bank_word(bank) == vk && row != 0 && branches.size / row >= loaded &&
        branches.offset <= BANK_MAX_U64 - row * loaded &&
        (loaded == 1 || row % bank_word(vk + BANK_ALIGNMENT) == 0);
}

fn bank_shape(width: u32, rows: u32) -> BankShape {
    return BankShape { width: width, rows: rows, batch: 1, epsilon: 0.0 as! f32, position: 0,
        kv_heads: 0, context: 0, theta: 0.0 as! f32, rotary: 0 };
}

export fn fg_vk_expert_bank_record_split(graph: u64, bank: u64, hidden: u32, ff: u32,
        ops: u64, input: BankView, scratch: u64, branches: BankView) -> i32 {
    let loaded = bank_read_u32(bank + BANK_LOADED) as! u64;
    let row = (hidden as! u64) * 4;
    if !bank_branches_valid(graph, bank, loaded, row, branches) { return 0; }
    let up = bank_shape(hidden, ff);
    let down = bank_shape(ff, hidden);
    let activate = bank_shape(ff, 0);
    let scratch0 = bank_view(scratch);
    let scratch1 = bank_view(scratch + 24);
    let scratch2 = bank_view(scratch + 48);
    for slot in (loaded as! i64) {
        let output = BankView { buffer: branches.buffer, offset: branches.offset + (slot as! u64) * row, size: row };
        if fg_vk_graph_record(graph, bank_read_u32(ops), up, input, bank_view(bank_plane(bank, slot, 0)), scratch0) == 0 ||
            fg_vk_graph_record(graph, bank_read_u32(ops + 4), up, input, bank_view(bank_plane(bank, slot, 1)), scratch1) == 0 ||
            fg_vk_graph_record(graph, 2, activate, scratch0, scratch1, scratch2) == 0 ||
            fg_vk_graph_record(graph, bank_read_u32(ops + 8), down, scratch2, bank_view(bank_plane(bank, slot, 2)), output) == 0 {
            return 0;
        }
    }
    return 1;
}

export fn fg_vk_expert_bank_record_inkling(graph: u64, bank: u64, hidden: u32, ff: u32,
        input: BankView, gate_up: BankView, activated: BankView, branches: BankView) -> i32 {
    let loaded = bank_read_u32(bank + BANK_LOADED) as! u64;
    let row = (hidden as! u64) * 4;
    let half = (ff as! u64) * 4;
    if !bank_branches_valid(graph, bank, loaded, row, branches) || ff == 0 || ff > 0x7fffffff ||
        gate_up.size < half * 2 || gate_up.offset > BANK_MAX_U64 - half { return 0; }
    let gate = BankView { buffer: gate_up.buffer, offset: gate_up.offset, size: half };
    let up = BankView { buffer: gate_up.buffer, offset: gate_up.offset + half, size: half };
    let fused = bank_shape(hidden, ff * 2);
    let situ = bank_shape(ff, 0);
    let down = bank_shape(ff, hidden);
    for slot in (loaded as! i64) {
        let output = BankView { buffer: branches.buffer, offset: branches.offset + (slot as! u64) * row, size: row };
        if fg_vk_graph_record(graph, 26, fused, input, bank_view(bank_plane(bank, slot, 0)), gate_up) == 0 ||
            fg_vk_graph_record(graph, 36, situ, gate, up, activated) == 0 ||
            fg_vk_graph_record(graph, 25, down, activated, bank_view(bank_plane(bank, slot, 1)), output) == 0 { return 0; }
    }
    return 1;
}
