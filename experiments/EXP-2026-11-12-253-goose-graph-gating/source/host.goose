// APEX.MODEL_GRAPH: the expert-gating decision of build_moe_ffn, in Goose.
//
// The tensor operations themselves stay in ggml -- they are the graph primitives, exactly like the allocator
// and the backend calls. What Goose decides here is which of them the gating function applies, and the
// fallback that aborts on an unknown type.

extern "fg253s_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg253s_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg253s_fail" fn fail(c:i64);
extern "fg253s_soft_max" fn soft_max(t:i64)->i64;
extern "fg253s_sigmoid" fn sigmoid(t:i64)->i64;
extern "fg253s_sqrt" fn sqrt(t:i64)->i64;
extern "fg253s_softplus" fn softplus(t:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// gating_op matches llama_expert_gating_func_type: 0 NONE, 1 SOFTMAX, 2 SIGMOID,
// 3 SOFTMAX_WEIGHT (pass-through), 4 SQRT_SOFTPLUS. NONE aborts exactly as the native switch does.
// norm_w: normalise the expert weights over the used set, with the F16-smallest clamp on the sum.
// mode 2 in the packet selects it.
extern "fg253s_reshape_2d" fn reshape_2d(t:i64,n:i64,m:i64)->i64;
extern "fg253s_sum_rows" fn sum_rows(t:i64)->i64;
extern "fg253s_clamp" fn clamp(t:i64,lo:i64,hi:i64)->i64;
extern "fg253s_div" fn div(a:i64,b:i64)->i64;
extern "fg253s_reshape_3d" fn reshape_3d(t:i64,a:i64,b:i64,c:i64)->i64;

fn run(packet:i64)->bool {
    let mode:i64 = get(packet,0,2) as i64;
    if mode == 2 as i64 {
        let weights:i64 = get(packet,0,0) as i64;
        let n_used:i64  = get(packet,0,1) as i64;
        let n_tokens:i64 = get(packet,0,3) as i64;
        ensure(weights != 0 as i64 && n_used > 0 as i64, 20);
        var w:i64 = reshape_2d(weights, n_used, n_tokens);
        let ws:i64 = sum_rows(w);
        let wc:i64 = clamp(ws, 0x38800000 as i64, 0x7f800000 as i64);  // 6.103515625e-5 .. +inf
        w = div(w, wc);
        put(packet,1,0, w as u64);
        return true;
    }
    let logits:i64 = get(packet,0,0) as i64;
    let op:i64     = get(packet,0,1) as i64;
    ensure(logits != 0 as i64, 10);

    var probs:i64 = 0 as i64;
    if op == 1 as i64 { probs = soft_max(logits); }
    if op == 2 as i64 { probs = sigmoid(logits); }
    if op == 3 as i64 { probs = logits; }
    if op == 4 as i64 { probs = sqrt(softplus(logits)); }
    ensure(op >= 1 as i64 && op <= 4 as i64, 11);
    ensure(probs != 0 as i64, 12);
    put(packet,1,0, probs as u64);
    true
}

fn main(){run(0);}
