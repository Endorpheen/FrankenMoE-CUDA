extern fn fg_cuda_model_getenv(key: u64) -> u64;
type BonsaiGraphUnary = c_fnptr(u64) -> i32;
type BonsaiGraphFlags = c_fnptr(u64, u32) -> i32;
type BonsaiGraphPair = c_fnptr(u64, u64) -> i32;
type BonsaiGraphTriple = c_fnptr(u64, u64, u64) -> i32;

var bonsai_ffn_driver: u64[9];
var bonsai_ffn_ready: bool;
extern fn fg_bonsai_ffn_bind_once() -> i32;

export fn fg_bonsai_bind_ffn_graph() {
    let library = vocab_word(fg_bonsai_cuda_dispatch() + 152);
    bonsai_ffn_driver[0] = unsafe_c_library_symbol(library, "cuStreamCreate");
    bonsai_ffn_driver[1] = unsafe_c_library_symbol(library, "cuStreamDestroy_v2");
    bonsai_ffn_driver[2] = unsafe_c_library_symbol(library, "cuStreamBeginCapture_v2");
    bonsai_ffn_driver[3] = unsafe_c_library_symbol(library, "cuStreamEndCapture");
    bonsai_ffn_driver[4] = unsafe_c_library_symbol(library, "cuGraphGetNodes");
    bonsai_ffn_driver[5] = unsafe_c_library_symbol(library, "cuGraphInstantiateWithFlags");
    bonsai_ffn_driver[6] = unsafe_c_library_symbol(library, "cuGraphLaunch");
    bonsai_ffn_driver[7] = unsafe_c_library_symbol(library, "cuGraphDestroy");
    bonsai_ffn_driver[8] = unsafe_c_library_symbol(library, "cuGraphExecDestroy");
    for index in 9 {
        if bonsai_ffn_driver[index] == 0 { return; }
    }
    bonsai_ffn_ready = true;
}

export fn fg_bonsai_ffn_graph_open(model: u64) {
    let enabled = fg_cuda_model_getenv(vocab_text("BONSAI_HTTP_FFN_GRAPH\0").address);
    if enabled == 0 || vocab_byte(enabled) != 49 || vocab_byte(enabled + 1) != 0 ||
        batch_u32(model + 4712) != 1 || batch_u32(model + 4720) == 0 { return; }
    if fg_bonsai_ffn_bind_once() != 0 || !bonsai_ffn_ready { return; }
    let capture = model + 15288;
    let status = unsafe_c_fnptr<BonsaiGraphFlags>(bonsai_ffn_driver[0])(capture, 1);
    if bonsai_cuda_status(status, "Bonsai FFN graph stream") == 0 { batch_set(capture + 1044, 1); }
}

export fn bonsai_bonsai_capture_driver(op: u32, first: u64, second: u64) -> i32 {
    if op == 0 {
        return bonsai_cuda_status(unsafe_c_fnptr<BonsaiGraphFlags>(bonsai_ffn_driver[2])(first, 1),
            "Bonsai FFN graph capture start");
    }
    if op == 1 || op == 6 {
        let result = unsafe_c_fnptr<BonsaiGraphPair>(bonsai_ffn_driver[3])(first, second);
        if op == 6 { return result; }
        return bonsai_cuda_status(result, "Bonsai FFN graph capture end");
    }
    if op == 2 {
        return bonsai_cuda_status(unsafe_c_fnptr<BonsaiGraphTriple>(bonsai_ffn_driver[4])(first, 0, second),
            "Bonsai FFN graph nodes");
    }
    if op == 3 {
        return bonsai_cuda_status(unsafe_c_fnptr<BonsaiGraphTriple>(bonsai_ffn_driver[5])(second, first, 0),
            "Bonsai FFN graph instantiate");
    }
    if op == 4 { return unsafe_c_fnptr<BonsaiGraphUnary>(bonsai_ffn_driver[7])(first); }
    if op == 5 {
        return bonsai_cuda_status(unsafe_c_fnptr<BonsaiGraphPair>(bonsai_ffn_driver[6])(first, 0),
            "Bonsai FFN graph launch");
    }
    return -1;
}
