// Apex model graph decisions in Goose (APX: model runtime).
//
// From models/qwen35moe.cpp and llama-hparams / llama-graph.h. These are the per-layer decisions
// the builder makes while constructing the graph for qwen35moe (Apex), not the graph itself:
// which layer kind to build, and how the NextN/MTP hidden state is cropped.
//
// Kept separate from EXP199 (KV cells) because it is a different subsystem.

extern "fg200g_fail" fn fail(code:i64);
extern "fg200g_st_i64" fn st_i64(base:u64,byte:i64,v:i64);
extern "fg200g_ld_u32" fn ld_u32(base:u64,index:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// is_recr: llama-hparams.cpp:260. `is_recr_impl` is std::array<uint32_t, LLAMA_MAX_LAYERS> -- one
// 4-byte element per layer, NOT a packed bitset. Reading it as words was the first bug here: the
// model then loaded with the wrong attention shape and refused to start.
fn is_recr(il:i64,n_layer_all:i64,flags:u64)->i64{
 ensure(il>=0,10);
 ensure(n_layer_all>=0,11);
 if il>=n_layer_all{return -1 as i64;}   // native aborts; the caller must treat -1 as fatal
 let v:i64=ld_u32(flags,il);
 if v!=0 as i64{return 1 as i64;}
 return 0 as i64;
}

// crop_before_nextn: llama-graph.h:1086.
//   inp_out_ids != null && (!embeddings_nextn || embeddings_nextn_masked)
fn crop_before_nextn(has_out_ids:i64,emb_nextn:i64,emb_masked:i64)->i64{
 ensure(has_out_ids==0||has_out_ids==1,20);
 if has_out_ids==0{return 0 as i64;}
 if emb_nextn==0{return 1 as i64;}
 if emb_masked!=0{return 1 as i64;}
 return 0 as i64;
}

// crop_after_nextn: llama-graph.h:1091.
//   inp_out_ids != null && embeddings_nextn && !embeddings_nextn_masked
fn crop_after_nextn(has_out_ids:i64,emb_nextn:i64,emb_masked:i64)->i64{
 ensure(has_out_ids==0||has_out_ids==1,30);
 if has_out_ids==0{return 0 as i64;}
 if emb_nextn==0{return 0 as i64;}
 if emb_masked!=0{return 0 as i64;}
 return 1 as i64;
}

// capture_layer_input: models/qwen35moe.cpp:182 -- `res->t_layer_inp[il] = inpL` for every layer
// in the main pass. Returns 1 to record it, 0 when nextn layers are being executed separately.
fn capture_layer_input(il:i64,n_layer:i64)->i64{
 ensure(il>=0&&n_layer>=0,40);
 if il<n_layer{return 1 as i64;}
 return 0 as i64;
}

// shift_skip: llama-kv-cache.cpp:1303 area -- a shift of 0 means the shift graph is not built.
fn shift_skip(shift:i64)->i64{
 if shift==0 as i64{return 1 as i64;}
 return 0 as i64;
}

fn main(){
 is_recr(0 as i64,1 as i64,0 as u64);
 crop_before_nextn(0 as i64,0 as i64,0 as i64);
 crop_after_nextn(0 as i64,0 as i64,0 as i64);
 capture_layer_input(0 as i64,1 as i64);
 shift_skip(0 as i64);
}
