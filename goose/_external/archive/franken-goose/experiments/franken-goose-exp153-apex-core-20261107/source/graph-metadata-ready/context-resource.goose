// worker-4: EXP107 llama_context resource lifecycle + checkpoint/state port.
// Status: UNVERIFIED PROPOSAL for parent Codex. Model-free, not compiled here.
// Covers: resource ownership init (constructor :420-590), deterministic
// release (dtor body :592-614), sched_new (:750-765), sched_reserve (:767-945)
// and in-memory checkpoint/state orchestration (:3456-3559, :3695-3755).
// The frame is the per-instance handle (FG107ExecFrame); resources stay owned
// by llama_context RAII members, so destruction order is the C++ member order.
//
// Resource opcodes (fg107_r): 0 dev_at 1 dev_init 2 backend_add 3 gdev_count
// 4 gdev_at 5 is_accel 6 init_cpu 7 build_set_n_threads 8 set_abort
// 9 output_reserve 10 create_memory 11 enumerate_backends 12 pipeline_decide
// 13 create_moe_cache 14 flash_v_check 15 pin_predicate 16 pin_experts
// 17 init_token_ids 18 res_reserve_new 19 resolve_fused_ops 20 sched_n_splits
// 21 res_reserve_inputs_size 22 sched_set_growth_limits 23 graph_n_input_tensors
// 24 buft_is_host 25 sched_buffer_size 26 set_buf_exp_size 27 sched_create
// 28 sched_set_copy_cb 29 sched_set_expert_pipeline 30 sched_set_realloc_cb
// 31 backend_at 32 buft_at 33 res_reserve_has_logits 34 opt_free
// 35 memory_present 36 buffer_size_expect 37 backend_count 38 log_reserve
// 50 io_dummy_new 51 io_host_write_new 52 io_host_read_new 53 io_write_u32
// 54 io_read_u32 55 io_write_seq 56 io_read_seq 57 io_write_state
// 58 io_read_state 59 io_write_seq_state 60 io_read_seq_state 61 io_n_bytes
// 62 io_discard 63 mem_storage_has 64 io_device_write_new 65 io_device_read_new
// 66 on_device_flag 67 is_on_device
// fg107_rq fields: 0 vocab_only 1 device_count 2 sched_need_reserve 3 training
// 4 no_alloc 5 n_devices 6 n_gpu_layers 7 n_layer_all 8 split_layer 9 offload_kqv
// 10 tensor_overrides 11 moe_cache_size 12 n_input_tensors 13 graph_has_logits
// 14 backend_count 15 arch 16 type_v_quantized(a) 17 flash_attn
// 18 arch_qwen35moe 19 arch_kimi_or_minimax 20 n_layer
// fg107_rq_set: 0 sched_need_reserve 1 graph_has_logits 2 pipeline_parallel
// 3 n_input_tensors

extern "fg107_ctx"                fn ctx(f:i64,field:i64,a:i64)->i64;
extern "fg107_ctx_set"            fn ctx_set(f:i64,field:i64,v:i64)->i64;
extern "fg107_sched"              fn sched(f:i64,op:i64,a:i64,b:i64,d:i64)->i64;
extern "fg107_mem"                fn mem(f:i64,op:i64,a:i64,b:i64,d:i64)->i64;
extern "fg107_graph_reserve"      fn graph_reserve(f:i64,t:i64,s:i64,o:i64,m:i64,lazy:i64)->i64;
extern "fg107_res_reset_all_prev" fn res_reset_all_prev(f:i64)->i64;
extern "fg107_log"                fn log(f:i64,site:i64,a:i64,b:i64)->i64;
extern "fg107_r"                  fn r(f:i64,op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg107_rq"                 fn rq(f:i64,field:i64,a:i64)->i64;
extern "fg107_rq_set"             fn rq_set(f:i64,field:i64,v:i64)->i64;

fn sched_new(f:i64,max_nodes:i64,parallel:i64) {
    r(f,27,max_nodes,parallel,0);                       // ggml_backend_sched_new
    r(f,28,0,0,0);                                      // set_copy_callback (strong Goose export)
    let expert_pipeline = if rq(f,18,0)!=0&&ctx(f,30,0)!=0&&parallel==0&&rq(f,11,0)==0 {1} else {0};
    r(f,29,expert_pipeline,0,0);                        // set_expert_pipeline
    r(f,30,0,0,0);                                      // set_realloc_callback
}

// Port of sched_reserve (:767-945). Returns 0 on success, -1 on failure.
fn sched_reserve(f:i64)->i64 {
    if rq(f,2,0)==0 {return 0;}                         // !sched_need_reserve
    rq_set(f,2,0);
    if ctx(f,1,0)!=0 {sched(f,3,0,0,0);}                // synchronize() (no sched on first call)
    let n_seqs=ctx(f,5,0);
    let n_ctx=ctx(f,2,0);
    let n_ubatch=ctx(f,4,0);
    let n_tokens=if n_ctx<n_ubatch {n_ctx} else {n_ubatch};
    let max_nodes=ctx(f,34,n_tokens);
    res_reset_all_prev(f);
    r(f,18,max_nodes,0,0);                              // gf_res_reserve.reset(new ...)
    sched_new(f,max_nodes,ctx(f,15,0));
    var m:i64=0;
    if ctx(f,0,0)!=0 {
        m=mem(f,1,0,0,0);
        if m==0 {return -1;}
    }
    r(f,19,m,n_seqs,0);                                 // resolve_fused_ops
    var lazy=ctx(f,30,0);
    if ctx(f,0,0)==0 {lazy=0;}
    let n_outputs_pp=if n_tokens<ctx(f,21,0) {n_tokens} else {ctx(f,21,0)};
    var gf=graph_reserve(f,n_tokens,n_seqs,n_outputs_pp,m,lazy);
    if gf==0 {
        if ctx(f,15,0)!=0 {
            rq_set(f,2,0);                              // pipeline_parallel = false
            sched_new(f,max_nodes,0);
            gf=graph_reserve(f,n_tokens,n_seqs,n_outputs_pp,m,0);
        }
        if gf==0 {return -1;}
    }
    rq_set(f,1,r(f,33,0,0,0));                          // graph_has_logits
    rq_set(f,3,r(f,23,gf,0,0));                         // n_input_tensors
    if rq(f,3,0)!=0 {                                   // training: no tg graph
        r(f,38,0,0,0);
    } else {
        let gft=graph_reserve(f,n_seqs,n_seqs,n_seqs,m,rq(f,4,0));
        if gft==0 {return -1;}
    }
    // reserve pp again; two archs use n_seqs == 1 (TAG_RESERVE_DIAG_DECAY)
    if rq(f,19,0)!=0 {
        gf=graph_reserve(f,n_tokens,1,n_outputs_pp,m,lazy);
    } else {
        gf=graph_reserve(f,n_tokens,n_seqs,n_outputs_pp,m,lazy);
    }
    if gf==0 {return -1;}
    var i:i64=0;
    let nb=r(f,37,0,0,0);
    while i<nb {
        let be=r(f,31,i,0,0);
        if rq(f,4,0)==0 {r(f,26,i,r(f,25,be,0,0),0);}
        r(f,38,i,0,0);
        i+=1;
    }
    return 0;
}

fn resource_init(f:i64)->i64 {
    if rq(f,0,0)!=0 {return 0;}                        // vocab_only: no resources
    if rq(f,0,0)==0 {                                   // !vocab_only
        var i:i64=0;
        while i<rq(f,1,0) {                             // model.devices
            let dev=r(f,0,i,0,0);
            let be=r(f,1,dev,0,0);
            if be==0 {return -1;}
            r(f,2,be,0,0);
            i+=1;
        }
        i=0;
        while i<r(f,3,0,0,0) {                          // global ACCEL devices
            let dev=r(f,4,i,0,0);
            if r(f,5,dev,0,0)!=0 {
                let be=r(f,1,dev,0,0);
                if be==0 {return -1;}
                r(f,2,be,0,0);
            }
            i+=1;
        }
        let cpu=r(f,6,0,0,0);
        if cpu==0 {return -1;}
        r(f,2,cpu,0,0);
        r(f,7,0,0,0);                                   // set_n_threads fns
        r(f,8,0,0,0);                                   // abort callback (params in parent glue)
        if r(f,9,ctx(f,5,0),0,0)==0 {return -1;}        // output_reserve(n_seq_max)
    }
    if rq(f,0,0)==0 {
        if r(f,10,0,0,0)==0 {return -1;}                // create_memory
    }
    if rq(f,0,0)==0 {
        r(f,11,0,0,0);                                  // enumerate backends/bufts
        r(f,12,0,0,0);                                  // pipeline_parallel decision
        r(f,13,0,0,0);                                  // moe cache
        if sched_reserve(f)!=0 {return -1;}
        if r(f,14,0,0,0)==0 {return -1;}                // flash/type_v check
    }
    if r(f,15,0,0,0)!=0 {r(f,16,0,0,0);}                // pin predicate + pin
    r(f,17,0,0,0);                                      // sampling token ids
    return 0;
}

// Port of the destructor body (:592-614). Member destruction order is the
// C++ declaration order and remains deterministic (RAII).
fn resource_release(f:i64) {
    if ctx(f,1,0)!=0 {sched(f,3,0,0,0);}                // synchronize()
    if rq(f,4,0)==0&&r(f,34,0,0,0)==0 {                 // !no_alloc && !opt_ctx
        var i:i64=0;
        let nb=r(f,37,0,0,0);
        while i<nb {
            r(f,36,i,0,0);                              // compare expected/actual, log
            i+=1;
        }
    }
    r(f,34,0,0,0);                                      // ggml_opt_free(opt_ctx)
}

// checkpoint/state orchestration
fn state_get_size(f:i64)->i64 {
    r(f,50,0,0,0);                                      // dummy(false)
    let n=r(f,57,0,0,0);
    if n<0 {log(f,30,0,0);return 0;}
    return n;
}
fn state_get_data(f:i64,dst:i64,size:i64)->i64 {
    r(f,51,dst,size,0);                                 // host write io
    let n=r(f,57,0,0,0);
    if n<0 {log(f,31,0,0);return 0;}
    return n;
}
fn state_set_data(f:i64,src:i64,size:i64)->i64 {
    r(f,52,src,size,0);                                 // host read io
    let n=r(f,58,0,0,0);
    if n<0 {r(f,62,0,0,0);log(f,32,0,0);return 0;}
    return n;
}
fn state_seq_get_size(f:i64,seq:i64,flags:i64)->i64 {
    r(f,50,r(f,67,flags,0,0),0,0);                      // dummy(ON_DEVICE)
    r(f,53,0xaf143cd8,0,0);
    r(f,55,seq,0,0);
    let n=r(f,59,seq,flags,0);
    if n<0 {log(f,30,0,0);return 0;}
    return n;
}
fn state_seq_get_data(f:i64,seq:i64,dst:i64,size:i64,flags:i64)->i64 {
    if r(f,67,flags,0,0)!=0 {r(f,64,dst,size,seq);}
    else {r(f,51,dst,size,0);}
    r(f,53,0xaf143cd8,0,0);
    r(f,55,seq,0,0);
    let n=r(f,59,seq,flags,0);
    if n<0 {log(f,31,0,0);return 0;}
    return n;
}
fn state_seq_set_data(f:i64,seq:i64,src:i64,size:i64,flags:i64)->i64 {
    if r(f,67,flags,0,0)!=0 {
        r(f,52,src,size,0);                             // temp host read for magic/seq
        if r(f,54,0,0,0)!=0xaf143cd8 {return 0;}
        let sid=r(f,56,0,0,0);
        assert(r(f,63,sid,0,0)!=0);
        r(f,65,src,size,sid);                           // device read io for sid
    } else {
        r(f,52,src,size,0);
    }
    if r(f,54,0,0,0)!=0xaf143cd8 {r(f,62,0,0,0);log(f,32,0,0);return 0;}
    r(f,56,0,0,0);
    let n=r(f,60,seq,flags,0);
    if n<0 {r(f,62,0,0,0);log(f,32,0,0);return 0;}
    return n;
}

// strong exports for the parent glue
fn goose_resource_init(f:i64)->i64 {return resource_init(f);}
fn goose_resource_release(f:i64) {resource_release(f);}
fn goose_sched_new(f:i64,max_nodes:i64,parallel:i64) {sched_new(f,max_nodes,parallel);}
fn goose_sched_reserve(f:i64)->i64 {return sched_reserve(f);}
fn goose_state_get_size(f:i64)->i64 {return state_get_size(f);}
fn goose_state_get_data(f:i64,dst:i64,size:i64)->i64 {return state_get_data(f,dst,size);}
fn goose_state_set_data(f:i64,src:i64,size:i64)->i64 {return state_set_data(f,src,size);}
fn goose_state_seq_get_size(f:i64,seq:i64,flags:i64)->i64 {return state_seq_get_size(f,seq,flags);}
fn goose_state_seq_get_data(f:i64,seq:i64,dst:i64,size:i64,flags:i64)->i64 {return state_seq_get_data(f,seq,dst,size,flags);}
fn goose_state_seq_set_data(f:i64,seq:i64,src:i64,size:i64,flags:i64)->i64 {return state_seq_set_data(f,seq,src,size,flags);}

fn main() {
    // Reachability only: keeps the exported resource/state entries in the
    // generated object; unit-resource.c wraps the generated gg_function_*.
    let a=goose_resource_init(0);
    goose_resource_release(0);
    goose_sched_new(0,0,0);
    let b=goose_sched_reserve(0);
    let c=goose_state_get_size(0);
    let d=goose_state_get_data(0,0,0);
    let e=goose_state_set_data(0,0,0);
    let f=goose_state_seq_get_size(0,0,0);
    let g=goose_state_seq_get_data(0,0,0,0,0);
    let h=goose_state_seq_set_data(0,0,0,0,0);
    print(a+b+c+d+e+f+g+h);
}
