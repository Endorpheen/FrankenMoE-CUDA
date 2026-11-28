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
