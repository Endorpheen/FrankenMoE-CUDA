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


// build_moe_ffn decisions, from llama-graph.cpp:2189-2348.
//
// The MoE body is graph construction; what it decides, and what is ported here, is which gating
// op to apply, whether group selection runs, whether weights are normalised and scaled, and
// whether the weights are applied before the FFN.

// gating_kind: the four gating functions, as the switch at 2227-2249 expresses them.
//   0 SOFTMAX, 1 SIGMOID, 2 SOFTMAX_WEIGHT (identity before top-k), 3 SQRT_SOFTPLUS
fn gating_kind(gating_op:i64)->i64{
 if gating_op==0 as i64{return 0 as i64;}
 if gating_op==1 as i64{return 1 as i64;}
 if gating_op==2 as i64{return 2 as i64;}
 if gating_op==3 as i64{return 3 as i64;}
 ensure(false,50);   // native aborts on an unknown gating type
 return 0 as i64;
}

// group_select: line 2283, `n_expert_groups > 1 && n_tokens > 0`.
fn group_select(n_expert_groups:i64,n_tokens:i64)->i64{
 // Native has no lower bound here: a model may report 0 groups, and `> 1` then simply fails.
 ensure(n_expert_groups>=0,51);
 ensure(n_tokens>=0,52);
 if n_expert_groups>1 as i64&&n_tokens>0 as i64{return 1 as i64;}
 return 0 as i64;
}

// weight_before_ffn: line 2214, only llama4 applies the sigmoid-ed weights before the FFN.
fn weight_before_ffn(is_llama4:i64)->i64{
 if is_llama4!=0{return 1 as i64;}
 return 0 as i64;
}

// weights_need_scale: line 2345, `w_scale != 0 && w_scale != 1`.
// The comparison is on the float value; the caller passes it as a bit pattern so the decision
// stays integer arithmetic. 0.0f = 0x00000000, 1.0f = 0x3f800000.
fn weights_need_scale(w_scale_bits:i64)->i64{
 if w_scale_bits==0 as i64{return 0 as i64;}
 if w_scale_bits==1065353216 as i64{return 0 as i64;}   // 0x3f800000
 return 1 as i64;
}


// build_layer_attn, from models/qwen35moe.cpp:276-355.

// kq_scale_kind: line 344.
//   f_attention_scale == 0.0f ? 1/sqrt(n_embd_head) : f_attention_scale
// The decision is whether to compute the reciprocal square root at all; the value itself stays
// with the caller, which already has sqrtf.
//   1 -> caller must compute 1/sqrt(n_embd_head);  0 -> caller uses f_attention_scale
fn kq_scale_auto(f_attention_scale_bits:i64)->i64{
 if f_attention_scale_bits==0 as i64{return 1 as i64;}   // 0.0f
 return 0 as i64;
}

// gate_view_offset: line 311-313. The gate is the second half of the QG projection, so its byte
// offset inside one row is element_size * n_embd_head. Returned as an element count so the caller
// multiplies by element size once.
fn gate_elem_offset(n_embd_head:i64)->i64{
 ensure(n_embd_head>0,60);
 return n_embd_head;
}

// q_full_width: line 288, the QG projection emits query and gate interleaved, so the projection
// width is twice the head width.
fn q_full_width(n_embd_head:i64)->i64{
 ensure(n_embd_head>0,61);
 return n_embd_head*2 as i64;
}

fn main(){
 is_recr(0 as i64,1 as i64,0 as u64);
 crop_before_nextn(0 as i64,0 as i64,0 as i64);
 crop_after_nextn(0 as i64,0 as i64,0 as i64);
 capture_layer_input(0 as i64,1 as i64);
 shift_skip(0 as i64);
 gating_kind(0 as i64);
 group_select(1 as i64,0 as i64);
 weight_before_ffn(0 as i64);
 weights_need_scale(0 as i64);
 kq_scale_auto(0 as i64);
 gate_elem_offset(1 as i64);
 q_full_width(1 as i64);
}
