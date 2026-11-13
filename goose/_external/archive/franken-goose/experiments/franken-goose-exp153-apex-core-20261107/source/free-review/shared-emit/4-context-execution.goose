// worker-4: EXP107 active llama_context execution port.
// Status: UNVERIFIED PROPOSAL for parent Codex. Not compiled, not integrated.
// This module contains real orchestration logic (decode loop, ubatch/process,
// output reservation/extraction, status/abort handling, expert-overlap copy).
// It does NOT call llama_context::decode / process_ubatch / graph_compute.
// Only replaceable primitives are imported: graph build/schedule/compute,
// memory, batch allocator, backend tensor transfers, MoE cache, output views.
//
// Source anchors (inputs/active-llama-context.cpp):
//   decode                  :1908-2308      process_ubatch        :1587-1667
//   graph_compute           :2818-2847      sched_copy_experts    :2849-2931
//   output_reserve          :2314-2474      output_reorder        :2509-2588
//   output_resolve_row      :1099-1126      extract_layer_inputs  :2476-2507
//   memory_update           :1025-1087      synchronize           :947-979
//   get_gf_res_prev         :2643-2649      graph_params          :2772-2797
//   graph_reserve           :2708-2770      set_abort_callback    :1386-1401
// Header members/ownership: inputs/active-llama-context.h:300-447.
// Expert overlap to preserve: sched_copy_experts + MoE cache + grouped copies
// + MMQ padding (:2893-2928) and graph_get_cb backend pinning (:2933-2958).
//
// Return codes mirror llama_context::decode/encode: 0 ok, 1 no memory slot,
// -1 invalid input, -2 alloc/compute setup failure, -3 compute failure,
// 2 aborted. -100 means "context has no memory, parent must route to encode".
//
// Service field/opcode legends are in worker-4/context-services.h.

// ---------------------------------------------------------------------------
// imported services (typed; all handles are opaque i64)
// ---------------------------------------------------------------------------
extern "fg107_ctx"                fn ctx(c:i64,field:i64,a:i64)->i64;
extern "fg107_ctx_set"            fn ctx_set(c:i64,field:i64,v:i64)->i64;
extern "fg107_perf"               fn perf(c:i64,op:i64,field:i64,value:i64)->i64;
extern "fg107_log"                fn log(c:i64,site:i64,a:i64,b:i64)->i64;
extern "fg107_batch"              fn batch(b:i64,field:i64,i:i64,s:i64)->i64;
extern "fg107_ub"                 fn ub(u:i64,field:i64,i:i64,s:i64)->i64;
extern "fg107_balloc"             fn balloc(c:i64,op:i64,a:i64,b:i64)->i64;
extern "fg107_mem"                fn mem(c:i64,op:i64,a:i64,b:i64,d:i64)->i64;
extern "fg107_mctx"               fn mctx(m:i64,op:i64,a:i64)->i64;
extern "fg107_res_prev"           fn res_prev(c:i64)->i64;
extern "fg107_res_prev_active"    fn res_prev_active(c:i64)->i64;
extern "fg107_res_set_prev_active" fn res_set_prev_active(c:i64,r:i64)->i64;
extern "fg107_res_reset_all_prev" fn res_reset_all_prev(c:i64)->i64;
extern "fg107_res"                fn res(r:i64,op:i64,a:i64)->i64;
extern "fg107_res_sampling_tensor" fn res_sampling_tensor(r:i64,kind:i64,i:i64)->i64;
extern "fg107_graph_params"       fn graph_params(c:i64,r:i64,u:i64,m:i64,g:i64)->i64;
extern "fg107_model_build_graph"  fn model_build_graph(c:i64,p:i64)->i64;
extern "fg107_graph_reserve"      fn graph_reserve(c:i64,t:i64,s:i64,o:i64,m:i64,lazy:i64)->i64;
extern "fg107_graph"              fn graph(g:i64,op:i64,a:i64)->i64;
extern "fg107_sched"              fn sched(c:i64,op:i64,a:i64,b:i64,d:i64)->i64;
extern "fg107_tensor"             fn tensor(t:i64,field:i64,i:i64)->i64;
extern "fg107_tensor_get_async"   fn tensor_get_async(be:i64,t:i64,dst:i64,off:i64,size:i64)->i64;
extern "fg107_tensor_set_async"   fn tensor_set_async(be:i64,t:i64,src:i64,off:i64,size:i64)->i64;
extern "fg107_backend_synchronize" fn backend_synchronize(be:i64)->i64;
extern "fg107_moe"                fn moe(c:i64,op:i64,a:i64,b:i64,d:i64,e:i64,f:i64)->i64;
extern "fg107_op_mul_mat_id"      fn op_mul_mat_id()->i64;
extern "fg107_out"                fn out(c:i64,op:i64,a:i64,b:i64,d:i64)->i64;
extern "fg107_out_bind"           fn out_bind(c:i64,which:i64,il:i64,off:i64,count:i64)->i64;
extern "fg107_out_view_swap"      fn out_view_swap(c:i64,which:i64,il:i64,i:i64,j:i64)->i64;
extern "fg107_exec"               fn exec(c:i64,op:i64,a:i64,b:i64,d:i64)->i64;
extern "fg107_exec_copy_id_at"    fn exec_copy_id_at(c:i64,i:i64)->i64;

// Field/opcode legends (must match worker-4/context-services.h):
//   ctx: 0 memory_present 1 sched_present 2 n_ctx 3 n_batch 4 n_ubatch
//        5 n_seq_max 6 n_vocab 7 n_embd 8 n_embd_out 9 n_cls_out
//        10 embeddings 11 embeddings_nextn 12 embeddings_nextn_masked
//        13 causal_attn 14 kv_unified 15 pipeline_parallel
//        16 graph_reuse_disabled 17 graph_type 18 pooling_type 19 arch_is_t5
//        20 graph_has_logits 21 n_outputs_max 22 n_outputs_max_per_seq
//        23 has_samplers 24 layer_inp_count 25 n_outputs 26 no_perf
//        27 n_threads 28 n_threads_batch 29 threadpool(a=batched)
//        30 lazy_pp 31 max_seq 32 pos_max 33 time_us 34 max_nodes(a=n_tokens)
//        35 layer_inp_enabled(a=il) 36 sampler_has(a=seq) 37 count_inc(a=seq)
//        39 last_ret
//   ctx_set: 0 n_outputs 1 note_reused 2 last_error 3 samplers_begin 4 last_ret
//        5 sampler_counts_reset
//   perf field: 0 queued 1 t_start 2 t_compute_start 3 n_eval 4 t_eval
//        5 n_p_eval 6 t_p_eval 7 has_evaluated 8 t_load 9 no_perf
//   batch: 0 n_tokens 1 n_embd 2 n_embd_inp 3 token_output(i)
//        4 token_n_seq(i) 5 token_seq(i,s)
//   ub: 0 n_tokens 1 output(i) 2 n_seq_id(i) 3 seq_id(i,s) 4 pos(i)
//        5 n_seqs_unq 6 seq_id_unq(s) 7 seq_idx(s) 8 batch_idxs_count 9 batch_idx(i)
//   balloc: 0 init(a=batch,b=output_all) 1 n_tokens 2 n_outputs 3 out_ids_size
//        4 out_id(a) 5 out_swap(a,b)
//   mem: 0 init_batch(a=n_ubatch,b=embd_all) 1 init_full
//        2 init_update(a=optimize) 3 seq_rm(a=seq,b=p0,d=p1)
//   mctx: 0 status 1 apply 2 next 3 ubatch
//   res: 0 gf 1 can_reuse(a) 2 reset 3 set_inputs(a) 4 logits 5 embd
//        6 embd_pooled 7 h_nextn 8 layer_inp(a=il) 9 sampling_count(a=kind)
//   graph: 0 n_nodes 1 node(a)
//   sched: 0 reset 1 set_eval_cb 2 alloc_graph(a) 3 synchronize
//        4 tensor_backend(a) 5 backend_count 6 compute_async(a) 7 reserve
//        8 copy_experts_reset 9 apply_threadpool(a) 10 apply_n_threads(a)
//        11 threads(a=batched) 12 threadpool(a=batched)
//        13 buffer_size(a=backend) 14 record_buf_size(a=backend) 15 backend_at(a=i)
//   tensor: 0 op 1 src(i) 2 ne(i) 3 nb(i) 4 nelements 5 nbytes
//        6 is_contiguous 7 data
//   moe: 0 cache_present 1 cache_copy(a=be,b=src,d=dst,e=graph)
//        2 cache_copy_experts(a=be,b=src,d=dst,e=first,f=last)
//   out: 0 buf_size 1 buf_alloc(a=size) 2 buf_base 3 logits_ptr 4 logits_size
//        5 embd_ptr 6 embd_size 7 nextn_ptr 8 nextn_size 9 layer_ptr(a=il)
//        10 layer_size(a=il) 11 layer_has_data(a=il) 12 embd_seq_size
//        13 embd_seq_clear 14 embd_seq_resize(a=seq,b=n) 15 embd_seq_ptr(a=seq)
//        16 sampling_prepare(a=n_outputs_max) 17 sampling_clear
//        18 sampling_ptr(a=kind) 19 sampling_size(a=kind)
//        20 sampling_count_capacity(a=kind) 21 sampling_count_set(a=kind,b=row,d=n)
//        22 sampling_elem_size(a=kind) 23 sampling_swap_scalar(a=kind,b=i,d=j)
//   exec: 0 output_ids_init(a=n) 1 output_ids_size 2 output_ids_get(a=i)
//        3 output_ids_set(a=i,b=v) 4 output_ids_fill(a=v) 5 swap_count
//        6 swap_get(a=k,b=which) 7 swap_push(a=i0,b=i1) 8 swaps_clear
//        9 embd_idx_count 10 embd_idx_get(a=i) 11 embd_idx_append_batch(a=ub)
//        12 embd_idx_clear 13 embd_idx_swap(a=i,b=j) 14 copy_ids
//        15 copy_ids_set(a=t) 16 copy_ids_data_resize(a=n) 17 copy_ids_data_ptr
//        18 copy_id_at(a=i) 19 copy_used_size 20 copy_used_assign(a=n)
//        21 copy_used_get(a=i) 22 copy_used_set(a=i,b=v)

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
fn needs_raw_logits(c:i64,u:i64)->i64 {
    var i:i64=0;
    while i<ub(u,0,0,0) {
        if ub(u,1,i,0)!=0 {
            var j:i64=0;
            while j<ub(u,2,i,0) {
                let seq_id=ub(u,3,i,j);
                if ctx(c,36,seq_id)==0 {return 1;}
                j+=1;
            }
        }
        i+=1;
    }
    return 0;
}

// Port of copy_tensor_async_rows (:1856-1889). kind: 0 sampled, 1 logits,
// 2 probs, 3 candidates. stride is in elements.
fn copy_async_rows(c:i64,r:i64,kind:i64,stride:i64,row_offset:i64,with_counts:i64) {
    if out(c,18,kind,0,0)==0 {return;}
    let n=res_sampling_tensor(r,kind,-1);          // count of tensors in the list
    let elem=out(c,22,kind,0,0);
    var i:i64=0;
    while i<n {
        let t=res_sampling_tensor(r,kind,i);
        if t!=0 {
            let row=row_offset+i;
            let ne=tensor(t,4,0);
            assert(tensor(t,6,0)!=0);
            assert(ne<=stride);
            assert(row*stride+ne<=out(c,19,kind,0,0));
            let be=sched(c,4,t,0,0);
            let dst=out(c,18,kind,0,0)+row*stride*elem;
            tensor_get_async(be,t,dst,0,tensor(t,5,0));
            if with_counts!=0 {
                assert(row<out(c,20,kind,0,0));
                out(c,21,kind,row,ne);
            }
        }
        i+=1;
    }
}

// ---------------------------------------------------------------------------
// synchronize (:947-979)
// ---------------------------------------------------------------------------
fn synchronize(c:i64) {
    if ctx(c,1,0)==0 {return;}
    sched(c,3,0,0,0);
    let q=perf(c,0,0,0);
    if q==1 {
        if perf(c,0,9,0)==0 {perf(c,1,4,perf(c,0,4,0)+ctx(c,33,0)-perf(c,0,2,0));}
        perf(c,1,3,perf(c,0,3,0)+1);
    } else if q>1 {
        if perf(c,0,9,0)==0 {perf(c,1,6,perf(c,0,6,0)+ctx(c,33,0)-perf(c,0,2,0));}
        perf(c,1,5,perf(c,0,5,0)+q);
    }
    if q>0&&perf(c,0,7,0)==0 {perf(c,1,8,ctx(c,33,0)-perf(c,0,1,0));perf(c,1,7,1);}
    perf(c,1,0,0);
    perf(c,1,2,0);
}

// ---------------------------------------------------------------------------
// graph_compute (:2818-2847)
// ---------------------------------------------------------------------------
fn graph_compute(c:i64,gf:i64,batched:i64)->i64 {
    let n_threads=sched(c,11,batched,0,0);
    let tp=sched(c,12,batched,0,0);
    sched(c,9,tp,0,0);            // backend_cpu threadpool glue (no-op if no CPU backend)
    sched(c,10,n_threads,0,0);    // all backends: set_n_threads
    sched(c,8,0,0,0);             // copy_experts.reset()
    let st=sched(c,6,gf,0,0);     // ggml_backend_sched_graph_compute_async
    if st!=0 {log(c,1,st,0);}
    return st;
}

// ---------------------------------------------------------------------------
// process_ubatch (:1587-1667)
// ---------------------------------------------------------------------------
fn process_ubatch(c:i64,u:i64,gtype:i64,m:i64)->i64 {
    if m!=0&&mctx(m,1,0)==0 {log(c,2,0,0);ctx_set(c,4,-1);return 0;}
    var r=res_prev(c);
    var gf=res(r,0,0);
    let gp=graph_params(c,r,u,m,gtype);
    let reuse=if ctx(c,16,0)==0&&res_prev_active(c)==r&&res(r,1,gp)!=0 {1} else {0};
    if reuse!=0 {
        if ctx(c,15,0)!=0 {sched(c,3,0,0,0);}   // pipeline_parallel: sync before set_inputs
        ctx_set(c,1,0);                          // n_reused++
    } else {
        res_set_prev_active(c,0);
        res(r,2,0);                              // res->reset()
        sched(c,0,0,0,0);                        // ggml_backend_sched_reset
        sched(c,1,0,0,0);                        // set_eval_callback
        gf=model_build_graph(c,gp);
        if gf==0 {log(c,3,0,0);ctx_set(c,4,-1);return 0;}
        if sched(c,2,gf,0,0)==0 {log(c,4,0,0);ctx_set(c,4,-2);return 0;}
        if ctx(c,0,0)!=0&&ctx(c,30,0)!=0 {
            var i:i64=0;
            let nb=sched(c,5,0,0,0);             // number of backends
            while i<nb {
                let be=sched(c,15,i,0,0);
                sched(c,14,be,0,0);              // record backend_buf_exp_size
                i+=1;
            }
        }
        res_set_prev_active(c,r);
    }
    res(r,3,u);                                  // set_inputs(&ubatch)
    let st=graph_compute(c,res(r,0,0),if ub(u,0,0,0)>1 {1} else {0});
    if st!=0 {log(c,5,st,0);ctx_set(c,4,st);return 0;}
    ctx_set(c,4,0);
    return r;
}

// ---------------------------------------------------------------------------
// output_reserve (:2314-2474)
// ---------------------------------------------------------------------------
fn output_reserve(c:i64,n_outputs:i64)->i64 {
    let n_seq_max=ctx(c,5,0);
    let n_outputs_max=if n_outputs>n_seq_max {n_outputs} else {n_seq_max};
    let n_batch=ctx(c,3,0);
    let n_vocab=ctx(c,6,0);
    let n_embd=ctx(c,7,0);
    let n_embd_out=ctx(c,8,0);
    var has_logits=if ctx(c,10,0)==0||ctx(c,20,0)!=0 {1} else {0};
    var has_embd=ctx(c,10,0);
    var has_nextn=ctx(c,11,0);
    if ctx(c,19,0)!=0 {has_logits=1;has_embd=1;}
    let has_sampling=ctx(c,23,0);
    var logits_size=if has_logits!=0 {n_vocab*n_outputs_max} else {0};
    var embd_size=if has_embd!=0 {n_embd_out*n_outputs_max} else {0};
    var nextn_size=if has_nextn!=0 {n_embd_out*n_outputs_max} else {0};
    if has_nextn!=0&&ctx(c,12,0)==0 {nextn_size=n_embd_out*n_batch;}
    var layer_floats:i64=0;
    var il:i64=0;
    while il<ctx(c,24,0) {
        if ctx(c,35,il)!=0 {layer_floats+=n_embd*n_batch;}
        il+=1;
    }
    var backend_float:i64=0;
    var backend_token:i64=0;
    if has_sampling!=0 {backend_float=2*n_vocab*n_outputs_max;backend_token=(1+n_vocab)*n_outputs_max;}
    if exec(c,1,0,0,0)==0 {exec(c,0,n_batch,0,0);}   // output_ids.resize(n_batch)
    let new_size=(logits_size+embd_size+nextn_size+layer_floats+backend_float)*4+backend_token*4;
    if out(c,1,new_size,0,0)==0 {log(c,6,new_size,0);return 0;}
    var off:i64=0;
    out_bind(c,0,0,off,logits_size); off+=logits_size*4;
    out_bind(c,1,0,off,embd_size);   off+=embd_size*4;
    out_bind(c,2,0,off,nextn_size);  off+=nextn_size*4;
    il=0;
    while il<ctx(c,24,0) {
        if ctx(c,35,il)!=0 {out_bind(c,3,il,off,n_embd*n_batch);off+=n_embd*n_batch*4;}
        else {out_bind(c,3,il,0,0);}
        il+=1;
    }
    if has_sampling!=0 {
        out_bind(c,4,0,off,n_vocab*n_outputs_max); off+=n_vocab*n_outputs_max*4;
        out_bind(c,5,0,off,n_vocab*n_outputs_max); off+=n_vocab*n_outputs_max*4;
        out_bind(c,6,0,off,n_outputs_max);          off+=n_outputs_max*4;
        out_bind(c,7,0,off,n_vocab*n_outputs_max); off+=n_vocab*n_outputs_max*4;
        out(c,16,n_outputs_max,0,0);
    } else {
        out(c,17,0,0,0);
    }
    exec(c,4,-1,0,0);            // output_ids.fill(-1)
    ctx_set(c,0,0);              // this->n_outputs = 0
    assert(n_outputs_max<=ctx(c,21,0));
    return n_outputs_max;
}

// ---------------------------------------------------------------------------
// extract_layer_inputs (:2476-2507)
// ---------------------------------------------------------------------------
fn extract_layer_inputs(c:i64,r:i64,token_offset:i64,n_tokens:i64)->bool {
    var extracted=false;
    var il:i64=0;
    while il<ctx(c,24,0) {
        if ctx(c,35,il)!=0 {
            if out(c,11,il,0,0)==0 {assert(false);}
            let t=res(r,8,il);
            if t==0 {assert(false);}
            let nbytes=tensor(t,5,0);
            let nfloats=nbytes/4;
            assert(n_tokens>0);
            assert(nfloats%n_tokens==0);
            let row_floats=nfloats/n_tokens;
            assert(row_floats==ctx(c,7,0));
            let dst_offset=token_offset*row_floats;
            assert(dst_offset+nfloats<=out(c,10,il,0,0));
            let be=sched(c,4,t,0,0);
            assert(be!=0);
            tensor_get_async(be,t,out(c,9,il,0,0)+dst_offset*4,0,nbytes);
            extracted=true;
        }
        il+=1;
    }
    return extracted;
}

// ---------------------------------------------------------------------------
// output_reorder (:2509-2588)
// ---------------------------------------------------------------------------
fn output_reorder(c:i64) {
    let n_vocab=ctx(c,6,0);
    let n_embd=ctx(c,7,0);
    let n_embd_out=ctx(c,8,0);
    let n_swaps=exec(c,5,0,0,0);
    var s:i64=0;
    while s<n_swaps {
        let i0=exec(c,6,s,0,0);
        let i1=exec(c,6,s,1,0);
        if out(c,4,0,0,0)>0 {
            var k:i64=0;while k<n_vocab {out_view_swap(c,0,0,i0*n_vocab+k,i1*n_vocab+k);k+=1;}
        }
        if out(c,6,0,0,0)>0 {
            var k:i64=0;while k<n_embd_out {out_view_swap(c,1,0,i0*n_embd_out+k,i1*n_embd_out+k);k+=1;}
        }
        if out(c,8,0,0,0)>0&&ctx(c,12,0)!=0 {
            var k:i64=0;while k<n_embd_out {out_view_swap(c,2,0,i0*n_embd_out+k,i1*n_embd_out+k);k+=1;}
        }
        if ctx(c,23,0)!=0 {
            assert(out(c,19,4,0,0)>0);
            assert(out(c,19,5,0,0)>0);
            assert(out(c,19,6,0,0)>0);
            assert(out(c,19,7,0,0)>0);
            var k:i64=0;while k<n_vocab {out_view_swap(c,4,0,i0*n_vocab+k,i1*n_vocab+k);k+=1;}
            var k2:i64=0;while k2<n_vocab {out_view_swap(c,5,0,i0*n_vocab+k2,i1*n_vocab+k2);k2+=1;}
            var k3:i64=0;while k3<n_vocab {out_view_swap(c,6,0,i0*n_vocab+k3,i1*n_vocab+k3);k3+=1;}
            out(c,23,0,i0,i1);
            out(c,23,1,i0,i1);
            out(c,23,2,i0,i1);
            out(c,23,3,i0,i1);
        }
        s+=1;
    }
    exec(c,8,0,0,0);                 // output_swaps.clear()
    let n_idx=exec(c,9,0,0,0);
    var i:i64=0;
    while i<n_idx {
        var cur=exec(c,10,i,0,0);
        while cur!=i {
            let j=cur;
            assert(j>=0&&j<n_idx);
            if out(c,8,0,0,0)>0&&ctx(c,12,0)==0 {
                var k:i64=0;while k<n_embd_out {out_view_swap(c,2,0,i*n_embd_out+k,j*n_embd_out+k);k+=1;}
            }
            var l:i64=0;
            while l<ctx(c,24,0) {
                if out(c,11,l,0,0)!=0 {
                    var k:i64=0;while k<n_embd {out_view_swap(c,3,l,i*n_embd+k,j*n_embd+k);k+=1;}
                }
                l+=1;
            }
            exec(c,13,i,j,0);        // embd_batch_idxs swap
            cur=exec(c,10,i,0,0);
        }
        i+=1;
    }
    exec(c,12,0,0,0);                // embd_batch_idxs.clear()
}

// ---------------------------------------------------------------------------
// output_resolve_row (:1099-1126)
// ---------------------------------------------------------------------------
fn output_resolve_row(c:i64,i:i64)->i64 {
    let n_out=ctx(c,25,0);
    var j:i64=-1;
    if i<0 {
        j=n_out+i;
        if j<0 {ctx_set(c,2,1);return -1;}
    } else if i>=exec(c,1,0,0,0) {
        ctx_set(c,2,2);
        return -1;
    } else {
        j=exec(c,2,i,0,0);
    }
    if j<0 {ctx_set(c,2,3);return -1;}
    if j>=n_out {ctx_set(c,2,4);return -1;}
    return j;
}

// ---------------------------------------------------------------------------
// memory_update (:1025-1087)
// ---------------------------------------------------------------------------
fn memory_update(c:i64,optimize:i64)->bool {
    if ctx(c,0,0)==0 {return false;}
    let m=mem(c,2,optimize,0,0);            // memory->init_update(this, optimize)
    let st=mctx(m,0,0);
    if st==1 {return false;}                // NO_UPDATE
    if st==2||st==3 {log(c,7,0,0);return false;}
    res_reset_all_prev(c);
    res_set_prev_active(c,0);
    if mctx(m,1,0)==0 {log(c,8,0,0);}
    let full=mem(c,1,0,0,0);
    if full==0 {assert(false);}
    let n_seqs=ctx(c,5,0);
    let n_tokens=if ctx(c,2,0)<ctx(c,4,0) {ctx(c,2,0)} else {ctx(c,4,0)};
    let n_outputs_max=if n_tokens<ctx(c,21,0) {n_tokens} else {ctx(c,21,0)};
    let lazy=ctx(c,30,0);
    let gf=graph_reserve(c,n_tokens,n_seqs,n_outputs_max,full,lazy);
    if gf==0 {log(c,9,0,0);}
    return true;
}

// ---------------------------------------------------------------------------
// sched_copy_experts (:2849-2931) - expert overlap must be preserved
// ---------------------------------------------------------------------------
fn sched_copy_experts(c:i64,be:i64,src:i64,dst:i64,g:i64)->bool {
    if moe(c,1,be,src,dst,g,0)!=0 {return true;}
    if graph(g,0,0,0,0)==0 {return false;}
    let node=graph(g,1,0,0,0);
    if tensor(node,0,0)!=op_mul_mat_id()||tensor(node,1,0)!=dst {return false;}
    let ids=tensor(node,1,2);
    if tensor(ids,4,0)==0 {return true;}
    let n_expert=tensor(src,2,2);
    let expert_size=tensor(src,3,2);
    if ids!=exec(c,14,0,0,0)||exec(c,19,0,0,0)!=n_expert {
        exec(c,16,tensor(ids,5,0)/4,0,0);
        tensor_get_async(be,ids,exec(c,17,0,0,0),0,tensor(ids,5,0));
        backend_synchronize(be);
        exec(c,20,n_expert,0,0);
        var i1:i64=0;
        while i1<tensor(ids,2,1) {
            var i0:i64=0;
            while i0<tensor(ids,2,0) {
                let id=exec_copy_id_at(c,i1*tensor(ids,3,1)/4+i0*tensor(ids,3,0)/4);
                assert(id>=0&&id<n_expert);
                exec(c,22,id,1,0);
                i0+=1;
            }
            i1+=1;
        }
        exec(c,15,ids,0,0);
    }
    var first:i64=0;
    while first<n_expert {
        if exec(c,21,first,0,0)==0 {
            first+=1;
        } else {
            var last=first;
            while last+1<n_expert&&exec(c,21,last+1,0,0)!=0 {last+=1;}
            var next=first;
            var e=first;
            while e<=last&&moe(c,0,0,0,0,0,0)!=0 {
                let n=moe(c,2,be,src,dst,e,last);
                if n==0 {
                    e+=1;
                } else {
                    if next<e {tensor_set_async(be,dst,tensor(src,7,0)+next*expert_size,next*expert_size,(e-next)*expert_size);}
                    e+=n;
                    next=e;
                }
            }
            let offset=next*expert_size;
            let padding=if last<n_expert-1 {if expert_size<512 {expert_size} else {512}} else {0};
            let size=(last+1-next)*expert_size+padding;
            if size>0 {tensor_set_async(be,dst,tensor(src,7,0)+offset,offset,size);}
            first=last+1;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// decode (:1908-2308)
// ---------------------------------------------------------------------------
fn decode(c:i64,b:i64)->i64 {
    if ctx(c,0,0)==0 {return -100;}              // parent routes to encode
    if batch(b,0,0,0)==0 {log(c,10,0,0);return -1;}
    if batch(b,1,0,0)>0&&batch(b,1,0,0)!=batch(b,2,0,0) {log(c,11,batch(b,1,0,0),batch(b,2,0,0));return -1;}

    let output_all=if ctx(c,10,0)!=0 {1} else {0};
    let embd_all=if output_all!=0||ctx(c,13,0)==0 {1} else {0};
    let has_samplers=ctx(c,23,0);
    let n_seq_max=if ctx(c,14,0)!=0 {ctx(c,31,0)} else {ctx(c,5,0)};

    ctx_set(c,5,0);                              // fresh per-batch sampler counters
    if has_samplers!=0 {
        var i:i64=0;
        while i<batch(b,0,0,0) {
            if output_all!=0||batch(b,3,i,0)!=0 {
                var s:i64=0;
                while s<batch(b,4,i,0) {
                    let seq_id=batch(b,5,i,s);
                    if seq_id>=0&&seq_id<n_seq_max {
                        let cnt=ctx(c,37,seq_id);
                        if ctx(c,36,seq_id)!=0&&cnt>ctx(c,22,0) {log(c,12,seq_id,cnt);return -1;}
                    }
                    s+=1;
                }
            }
            i+=1;
        }
    }

    if balloc(c,0,b,output_all)==0 {log(c,13,0,0);return -1;}
    let n_tokens_all=balloc(c,1,0,0);
    let n_outputs_all=balloc(c,2,0,0);
    if output_all!=0&&n_outputs_all!=n_tokens_all {log(c,14,n_outputs_all,n_tokens_all);return -1;}
    assert(n_tokens_all<=ctx(c,3,0));
    assert(ctx(c,13,0)!=0||ctx(c,4,0)>=n_tokens_all);

    if out(c,12,0,0,0)>0 {synchronize(c);}
    out(c,13,0,0,0);
    if perf(c,0,2,0)==0 {perf(c,1,2,ctx(c,33,0));}
    perf(c,1,0,perf(c,0,0,0)+n_tokens_all);
    exec(c,8,0,0,0);
    exec(c,12,0,0,0);
    sched(c,7,0,0,0);                            // sched_reserve()

    var did_optimize:i64=0;
    memory_update(c,0);

    var m:i64=0;
    var ready:i64=0;
    while ready==0 {
        m=mem(c,0,ctx(c,4,0),embd_all,0);
        if m==0 {return -2;}
        let st=mctx(m,0,0);
        if st==0 {ready=1;}
        else if st==1 {log(c,15,st,0);return -2;}
        else if st==2 {
            if did_optimize==0 {
                did_optimize=1;
                if memory_update(c,1)!=0 {log(c,16,balloc(c,1,0,0),0);}
                else {log(c,17,balloc(c,1,0,0),0);return 1;}
            } else {log(c,17,balloc(c,1,0,0),0);return 1;}
        } else {log(c,18,balloc(c,1,0,0),0);return -2;}
    }

    if output_reserve(c,n_outputs_all)<n_outputs_all {log(c,19,n_outputs_all,0);return -2;}
    ctx_set(c,3,0);                              // samplers_begin

    var n_outputs_prev:i64=0;
    var n_tokens_prev:i64=0;
    var more:i64=1;
    while more!=0 {
        let u=mctx(m,3,0);
        let u_nt=ub(u,0,0,0);
        var n_outputs_new:i64=0;
        if n_outputs_all==n_tokens_all {n_outputs_new=u_nt;}
        else {
            var i:i64=0;
            while i<u_nt {n_outputs_new+=ub(u,1,i,0);i+=1;}
        }
        ctx_set(c,0,n_outputs_new);

        let r=process_ubatch(c,u,ctx(c,17,0),m);
        if r==0 {
            var s:i64=0;
            while s<ctx(c,31,0) {
                var pmin:i64=ctx(c,32,0);
                var i:i64=0;
                while i<u_nt {
                    if ub(u,3,i,0)==s&&ub(u,4,i,0)<pmin {pmin=ub(u,4,i,0);}
                    i+=1;
                }
                if pmin!=ctx(c,32,0) {log(c,20,s,pmin);mem(c,3,s,pmin,-1);}
                s+=1;
            }
            let st=ctx(c,39,0);
            if st==1 {return 2;}
            else if st==-2 {return -2;}
            else if st==-1 {return -3;}
            else {assert(false);}
        }

        let t_logits=res(r,4,0);
        var t_embd:i64=0;
        if ctx(c,10,0)!=0 {t_embd=res(r,5,0);}
        var t_h:i64=0;
        if ctx(c,11,0)!=0 {t_h=res(r,7,0);}
        if t_embd!=0&&res(r,6,0)!=0 {t_embd=res(r,6,0);}
        let n_out=ctx(c,25,0);

        if out(c,3,0,0,0)!=0&&t_logits!=0&&n_out>0&&needs_raw_logits(c,u)!=0 {
            let be=sched(c,4,t_logits,0,0);
            assert(be!=0);
            assert(n_outputs_prev+n_out<=n_outputs_all);
            assert((n_outputs_prev+n_out)*ctx(c,6,0)<=out(c,4,0,0,0));
            tensor_get_async(be,t_logits,out(c,3,0,0,0)+n_outputs_prev*ctx(c,6,0)*4,0,n_out*ctx(c,6,0)*4);
        }

        if out(c,5,0,0,0)!=0&&t_embd!=0&&n_out>0 {
            let be=sched(c,4,t_embd,0,0);
            assert(be!=0);
            let pt=ctx(c,18,0);
            if pt==0 {
                let ne=ctx(c,8,0);
                assert(n_outputs_prev+n_out<=n_outputs_all);
                assert((n_outputs_prev+n_out)*ne<=out(c,6,0,0,0));
                tensor_get_async(be,t_embd,out(c,5,0,0,0)+n_outputs_prev*ne*4,0,n_out*ne*4);
            } else if pt==1||pt==2||pt==3 {
                let ne=ctx(c,8,0);
                var s:i64=0;
                while s<ub(u,5,0,0) {
                    let seq_id=ub(u,6,s,0);
                    let seq_idx=ub(u,7,seq_id,0);
                    out(c,14,seq_id,ne,0);
                    tensor_get_async(be,t_embd,out(c,15,seq_id,0,0)+(ne*seq_idx)*4,0,ne*4);
                    s+=1;
                }
            } else if pt==4 {
                let nc=ctx(c,9,0);
                var s:i64=0;
                while s<ub(u,5,0,0) {
                    let seq_id=ub(u,6,s,0);
                    let seq_idx=ub(u,7,seq_id,0);
                    out(c,14,seq_id,nc,0);
                    tensor_get_async(be,t_embd,out(c,15,seq_id,0,0)+(nc*seq_idx)*4,0,nc*4);
                    s+=1;
                }
            } else {assert(false);}                  // POOLING_TYPE_UNSPECIFIED aborts
        }

        var extract_all:i64=0;
        if extract_layer_inputs(c,r,n_tokens_prev,u_nt) {extract_all=1;}

        {
            let masked=ctx(c,12,0);
            let n_rows=if masked!=0 {ctx(c,25,0)} else {u_nt};
            let off=if masked!=0 {n_outputs_prev} else {n_tokens_prev};
            if out(c,7,0,0,0)!=0&&t_h!=0&&n_rows>0&&ctx(c,18,0)==0 {
                let be=sched(c,4,t_h,0,0);
                assert(be!=0);
                let ne=ctx(c,8,0);
                assert((off+n_rows)*ne<=out(c,8,0,0,0));
                tensor_get_async(be,t_h,out(c,7,0,0,0)+off*ne*4,0,n_rows*ne*4);
                if masked==0 {extract_all=1;}
            }
        }

        if extract_all!=0 {
            assert(ub(u,8,0,0)==u_nt);
            assert(exec(c,9,0,0,0)==n_tokens_prev);
            exec(c,11,u,0,0);
        }

        if has_samplers!=0 {
            copy_async_rows(c,r,0,1,n_outputs_prev,0);
            copy_async_rows(c,r,1,ctx(c,6,0),n_outputs_prev,1);
            copy_async_rows(c,r,2,ctx(c,6,0),n_outputs_prev,1);
            copy_async_rows(c,r,3,ctx(c,6,0),n_outputs_prev,1);
        }

        n_outputs_prev+=n_out;
        n_tokens_prev+=u_nt;
        more=if mctx(m,2,0)!=0 {1} else {0};
    }

    ctx_set(c,0,n_outputs_all);
    if n_outputs_all>0 {
        assert(balloc(c,3,0,0)==n_outputs_all);
        var sorted:i64=1;
        var i:i64=0;
        while i<n_outputs_all {
            let out_id=balloc(c,4,i,0);
            exec(c,3,out_id,i,0);
            if out_id!=i {sorted=0;}
            i+=1;
        }
        if sorted==0&&n_outputs_all>1 {
            var a:i64=0;
            while a<n_outputs_all-1 {
                var jmin=a;
                var j=a+1;
                while j<n_outputs_all {
                    if balloc(c,4,j,0)<balloc(c,4,jmin,0) {jmin=j;}
                    j+=1;
                }
                if jmin!=a {
                    balloc(c,5,a,jmin);
                    exec(c,7,a,jmin,0);
                }
                a+=1;
            }
            exec(c,4,-1,0,0);
            var k:i64=0;
            while k<n_outputs_all {
                exec(c,3,balloc(c,4,k,0),k,0);
                k+=1;
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// exported bridge used by the C copy-callback trampoline
// ---------------------------------------------------------------------------
fn goose_sched_copy_experts(c:i64,be:i64,src:i64,dst:i64,g:i64)->i64 {
    if sched_copy_experts(c,be,src,dst,g) {return 1;}
    return 0;
}

fn main() {
    print(0);
}
