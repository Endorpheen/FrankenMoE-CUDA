extern fn fg_win_page_size() -> u64;
extern fn fg_win_map_model(path: u64, bytes: u64) -> u64;

fn model_host_map(host: u64,path: u64) {
    vocab_set(host+MODEL_PAGE_SIZE,fg_win_page_size());
    let base=fg_win_map_model(path,host+VOCAB_SIZE);
    if base==0 { vocab_fatal("model open or mapping"); }
    vocab_set(host+VOCAB_BASE,base);
}
