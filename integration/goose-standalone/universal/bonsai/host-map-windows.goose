extern fn fg_win_page_size() -> u64;
extern fn fg_win_map_model(path: u64, bytes: u64) -> u64;

fn model_host_map(host: u64,path: u64) {
    vocab_set(host+MODEL_PAGE_SIZE,fg_win_page_size());
    let base=fg_win_map_model(path,host+VOCAB_SIZE);
    if base==0 { vocab_fatal("model open or mapping"); }
    vocab_set(host+VOCAB_BASE,base);
}

extern fn fg_win_resident(address: u64,bytes: u64,flags: u64) -> i32;
extern fn vocab_clock_gettime(clock: i32,timespec: u64) -> i32;

export fn fg_q35_generation_now() -> f64 {
    var time: i64[2];
    vocab_clock_gettime(1,vocab_address(unsafe_c_address<c_ptr<const void>>(time[0])));
    let nanos=time[1] as! i32;
    return (time[0] as! f64)+(nanos as! f64)/1000000000.0;
}

export fn fg_model_mincore(address: u64,bytes: i64,flags: u8[:]) -> i64 {
    let vector=vocab_address(unsafe_c_slice_data<c_ptr<const void>>(flags));
    return fg_win_resident(address,bytes as! u64,vector) as! i64;
}

export fn fg_model_populate(address: u64,bytes: i64) -> i64 { return 1; }
export fn fg_model_zero_host_pages(address: u64,length: i64) -> i64 { return 0; }
