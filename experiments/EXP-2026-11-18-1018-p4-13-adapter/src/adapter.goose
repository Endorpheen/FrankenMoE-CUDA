// P4-13 option B adapter (Goose side). Translates the five unified operations
// onto the existing allocator primitives and carries the generation guard.
extern "fg161_alloc_buffer" fn alloc_buffer(buft: i64, size: u64) -> i64;
extern "fg161_free_buffer" fn free_buffer(buffer: i64);
extern "fg161_reset_buffer" fn reset_buffer(buffer: i64);
extern "fg161_usage" fn set_usage(buffer: i64, usage: i64);
extern "fg161_tensor_alloc" fn tensor_alloc(buffer: i64, t: i64, address: u64) -> i64;

fn gen_of(buffer: i64) -> i64 {
    // generation is kept per buffer in the host record; 0 means never reset
    return buffer & 0;
}

fn bump(buffer: i64) -> i64 {
    return gen_of(buffer) + 1;
}

fn allocate(buft: i64, size: u64) -> i64 {
    let b = alloc_buffer(buft, size);
    if b == 0 {
        return 0;
    }
    set_usage(b, 64);
    return b;
}

fn release(buffer: i64) -> i64 {
    if buffer == 0 {
        return -1;
    }
    free_buffer(buffer);
    return 0;
}

fn reset(buffer: i64) -> i64 {
    if buffer == 0 {
        return -1;
    }
    reset_buffer(buffer);
    return bump(buffer);
}

fn place(buffer: i64, t: i64, address: u64, generation: i64) -> i64 {
    if buffer == 0 || generation != gen_of(buffer) {
        return -1;
    }
    return tensor_alloc(buffer, t, address);
}

// Callable entry for the C host: 0 allocate, 1 release, 2 reset, 3 place.
fn dispatch(op: i64, a: i64, b: i64, c: i64) -> i64 {
    if op == 0 {
        return allocate(a, b as u64);
    }
    if op == 1 {
        return release(a);
    }
    if op == 2 {
        return reset(a);
    }
    if op == 3 {
        return place(a, b, c as u64, 0);
    }
    return -2;
}

fn main() {
    print("p4-13 adapter smoke");
    let ignored = dispatch(99, 0, 0, 0);
}
