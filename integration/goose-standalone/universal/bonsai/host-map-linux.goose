extern fn vocab_unix_open(path: u64, flags: i32) -> i32;
extern fn vocab_unix_fstat(fd: i32, status: u64) -> i32;
extern fn vocab_unix_sysconf(name: i32) -> i64;
extern fn vocab_unix_mmap(address: u64, size: u64, prot: i32, flags: i32, fd: i32, offset: i64) -> u64;
extern fn vocab_unix_close(fd: i32) -> i32;

fn model_host_map(host: u64, path: u64) {
    let fd=vocab_unix_open(path,524288);
    var status: u64[18];
    if fd<0 || vocab_unix_fstat(fd,vocab_address(unsafe_c_address<c_ptr<const void>>(status[0])))!=0 ||
        (status[3]&0xf000)!=0x8000 || (status[6] as! i64)<=0 { vocab_fatal("model open"); }
    let size=status[6];
    vocab_set(host+VOCAB_SIZE,size);
    let page=vocab_unix_sysconf(30);
    if page<=0 { vocab_fatal("model page size"); }
    vocab_set(host+MODEL_PAGE_SIZE,page as! u64);
    let base=vocab_unix_mmap(0,size,1,2,fd,0);
    vocab_unix_close(fd);
    if base==0xffffffffffffffff { vocab_fatal("model mmap"); }
    vocab_set(host+VOCAB_BASE,base);
}

extern fn vocab_unix_mincore(address: u64, bytes: u64, flags: u64) -> i32;
extern fn vocab_unix_madvise(address: u64, bytes: u64, advice: i32) -> i32;
extern fn vocab_unix_errno() -> u64;
extern fn vocab_clock_gettime(clock: i32, timespec: u64) -> i32;

export fn fg_q35_generation_now() -> f64 {
    var time: i64[2];
    vocab_clock_gettime(1,vocab_address(unsafe_c_address<c_ptr<const void>>(time[0])));
    return (time[0] as! f64)+(time[1] as! f64)/1000000000.0;
}

export fn fg_model_mincore(address: u64,bytes: i64,flags: u8[:]) -> i64 {
    let vector=vocab_address(unsafe_c_slice_data<c_ptr<const void>>(flags));
    return vocab_unix_mincore(address,bytes as! u64,vector) as! i64;
}

export fn fg_model_populate(address: u64,bytes: i64) -> i64 {
    if !MODEL_HAS_POPULATE_READ { return 1; }
    if vocab_unix_madvise(address,bytes as! u64,22)==0 { return 0; }
    let reason=generation_read_u32(vocab_unix_errno()) as! i64;
    if reason==22 || reason==38 || reason==95 { return 1; }
    return 0-reason;
}

// Boundary pages also contain caller data. Only discard complete interior pages.
export fn fg_model_zero_host_pages(address: u64,length: i64) -> i64 {
    if generation_env_equal("GOOSE_HOST_KV_DECOMMIT",48) ||
        address==0 || length<=0 || (length as! u64)>0xffffffffffffffff-address { return 0; }
    let size=vocab_unix_sysconf(30);
    if size<=0 { return 0; }
    let page=size as! u64;
    let bytes=length as! u64;
    let leading=(page-address%page)%page;
    if leading>=bytes { return 0; }
    let whole=(bytes-leading)/page*page;
    if whole==0 || vocab_unix_madvise(address+leading,whole,4)!=0 { return 0; }
    vocab_memset(unsafe_c_ptr<c_ptr<void>>(address),0,leading);
    vocab_memset(unsafe_c_ptr<c_ptr<void>>(address+leading+whole),0,bytes-leading-whole);
    return 1;
}

extern fn bonsai_munmap(base: u64, size: u64) -> i32;
extern fn bonsai_perror(label: u64);

fn bonsai_map_weights(model: u64, path: u64) -> i32 {
    let fd = vocab_unix_open(path, 0);
    if fd < 0 {
        bonsai_perror(path);
        return -1;
    }
    var status: u64[18];
    if vocab_unix_fstat(fd, vocab_address(unsafe_c_address<c_ptr<const void>>(status[0]))) != 0 ||
        (status[6] as! i64) < 24 {
        vocab_unix_close(fd);
        return -1;
    }
    let bytes = status[6];
    let base = vocab_unix_mmap(0, bytes, 1, 2, fd, 0);
    vocab_unix_close(fd);
    if base == 0xffffffffffffffff {
        bonsai_perror(vocab_text("mmap\0").address);
        return -1;
    }
    let result = bonsai_load_weights(model, base, bytes);
    bonsai_munmap(base, bytes);
    return result;
}
