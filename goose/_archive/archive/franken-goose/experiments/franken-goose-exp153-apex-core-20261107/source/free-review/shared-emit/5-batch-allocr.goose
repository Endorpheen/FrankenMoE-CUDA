// batch-allocr.goose - Goose port of llama_batch_allocr control logic.
// Source: inputs/llama-batch.cpp (init 28-496, ubatch_reserve 498-546,
// split_reset 576-583, split_simple 585-617, split_equal 619-788,
// split_seq 790-830, clear 832-862, ubatch_add 864-988,
// seq_pos_min/max 568-574). Source: inputs/llama-batch.h:154-261.
//
// Unverified proposal for Codex. Not built, not run, not integrated.
//
// Split/validation decisions, loops, ordering and seq bookkeeping are Goose.
// The bridge only provides typed storage primitives: scalar fields, integer
// vectors, position sets, coupling bits, per-sequence bitsets, seq-set map
// slots, ubatch field vectors, and float rawcopy for embd/state rows.
//
// The control state handle `a` is opaque. The input batch handle `in` is
// opaque. A returned ubatch handle is single-owner (see bridge contract).
// Empty split results are handle 0 with n_tokens 0 (C returns {}).
//
// Future native-free cleanup: the bridge must remain a pure data store. None
// of its primitives may implement splitting, sequence-set comparison policy,
// tail retention, validation, or ordering; all of that is below.

extern "fg153_ba_get"  fn get(a:i64,k:i64,i:i64)->i64;
extern "fg153_ba_put"  fn put(a:i64,k:i64,i:i64,v:i64);
extern "fg153_ba_call" fn call(op:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg153_ba_rcopy" fn rcopy(a:i64,src:i64,dst_row:i64,n:i64)->i64;
extern "fg153_ba_witness" fn witness(mode:i64);

fn ensure(ok:bool) {if !ok {let ignored=call(0,0,0,0,0);assert(false);}}
fn add(a:i64,b:i64)->i64 {
    ensure(a>=0&&b>=0&&a<=9223372036854775807-b);
    return a+b;
}
fn mul(a:i64,b:i64)->i64 {
    ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));
    return a*b;
}
fn sub(a:i64,b:i64)->i64 {ensure(a>=b);return a-b;}
fn minimum(a:i64,b:i64)->i64 {return if a<b {a}else{b};}
fn maximum(a:i64,b:i64)->i64 {return if a>b {a}else{b};}
fn pos_max_const()->i64 {return 2147483647;}
fn token_null()->i64 {return call(3,0,0,0,0);}
fn max_seq()->i64 {return call(4,0,0,0,0);}

// scalar state keys (bridge contract, not C offsets)
fn K_N_TOK()->i64 {return 0;}
fn K_N_EMBD()->i64 {return 1;}
fn K_N_EMBD_STATE()->i64 {return 2;}
fn K_N_SEQ_MAX()->i64 {return 3;}
fn K_N_POS()->i64 {return 4;}
fn K_N_OUTPUTS()->i64 {return 5;}
fn K_N_USED()->i64 {return 6;}
fn K_HAS_CPL()->i64 {return 7;}
fn K_MIXED()->i64 {return 8;}
fn K_HAS_TOKEN()->i64 {return 9;}
fn K_HAS_EMBD()->i64 {return 10;}
fn K_HAS_STATE()->i64 {return 11;}
fn K_DEBUG()->i64 {return 12;}

// vector state keys (authored by bridge, addressed by logical index)
fn V_TOKEN()->i64 {return 20;}
fn V_EMBD()->i64 {return 21;}
fn V_STATE()->i64 {return 22;}
fn V_IS_EMBD()->i64 {return 23;}
fn V_POS()->i64 {return 24;}
fn V_NSQ()->i64 {return 25;}
fn V_SQFLAT()->i64 {return 26;}
fn V_SQOFF()->i64 {return 27;}
fn V_OUTPUT()->i64 {return 28;}
fn V_DECISION()->i64 {return 29;}
fn V_SQUNQ()->i64 {return 30;}
fn V_SQIDX()->i64 {return 31;}
fn V_USED()->i64 {return 32;}
fn V_OUTIDS()->i64 {return 33;}

fn n_tok(a:i64)->i64 {return get(a,K_N_TOK(),0);}
fn n_pos(a:i64)->i64 {return get(a,K_N_POS(),0);}
fn n_seq_max(a:i64)->i64 {return get(a,K_N_SEQ_MAX(),0);}
fn n_out(a:i64)->i64 {return get(a,K_N_OUTPUTS(),0);}
fn n_used(a:i64)->i64 {return get(a,K_N_USED(),0);}
fn set_n_used(a:i64,v:i64) {put(a,K_N_USED(),0,v);}
fn has_cpl(a:i64)->bool {return get(a,K_HAS_CPL(),0)!=0;}
fn set_has_cpl(a:i64,v:i64) {put(a,K_HAS_CPL(),0,v);}

// token seq-set accessors: seq_set[i] stored as ordered unique seq ids
fn tok_seq_n(a:i64,i:i64)->i64 {return call(20,a,i,0,0);}
fn tok_seq_at(a:i64,i:i64,s:i64)->i64 {return call(21,a,i,s,0);}
fn tok_has_seq(a:i64,i:i64,s:i64)->bool {return call(22,a,i,s,0)!=0;}
fn tok_set_clear(a:i64,i:i64) {let ignored=call(23,a,i,0,0);}
fn tok_set_add(a:i64,i:i64,s:i64) {let ignored=call(24,a,i,s,0);}
// set relations from token i to token j
fn set_intersects(a:i64,i:i64,j:i64)->bool {return call(25,a,i,j,0)!=0;}
fn set_subset_ij(a:i64,i:i64,j:i64)->bool {return call(26,a,i,j,0)!=0;}
fn set_equal_ij(a:i64,i:i64,j:i64)->bool {return call(27,a,i,j,0)!=0;}

// seq_set_map: grouping of tokens by equal sequence set
fn map_clear(a:i64) {let ignored=call(28,a,0,0,0);}
fn map_put_token(a:i64,i:i64)->i64 {return call(29,a,i,0,0);}
fn map_slot_of_token(a:i64,i:i64)->i64 {return call(30,a,i,0,0);}
fn map_idx_n(a:i64,slot:i64)->i64 {return call(31,a,slot,0,0);}
fn map_idx_at(a:i64,slot:i64,pos:i64)->i64 {return call(32,a,slot,pos,0);}
fn map_idx_push(a:i64,slot:i64,i:i64) {let ignored=call(33,a,slot,i,0);}
fn map_rep_token(a:i64,slot:i64)->i64 {return call(34,a,slot,0,0);}

// seq_pos[s] is an ordered set of int positions
fn seq_pos_clear(a:i64,s:i64) {let ignored=call(35,a,s,0,0);}
fn seq_pos_insert(a:i64,s:i64,p:i64) {let ignored=call(36,a,s,p,0);}
fn seq_pos_min(a:i64,s:i64)->i64 {return call(37,a,s,0,0);}
fn seq_pos_max(a:i64,s:i64)->i64 {return call(38,a,s,0,0);}
fn seq_pos_empty(a:i64,s:i64)->bool {return call(39,a,s,0,0)!=0;}
fn seq_pos_size(a:i64,s:i64)->i64 {return call(40,a,s,0,0);}
fn seq_pos_reset_all(a:i64) {let ignored=call(41,a,0,0,0);}

// coupling / per-sequence bitsets used by init validation
fn cpl_clear(a:i64) {let ignored=call(42,a,0,0,0);}
fn cpl_set(a:i64,s0:i64,s1:i64) {let ignored=call(43,a,s0,s1,0);}
fn cpl_get(a:i64,s0:i64,s1:i64)->bool {return call(44,a,s0,s1,0)!=0;}
fn seqset_reset_all(a:i64) {let ignored=call(45,a,0,0,0);}
fn seqset_and_tok(a:i64,s:i64,i:i64) {let ignored=call(46,a,s,i,0);}
fn seqset_none(a:i64,s:i64)->bool {return call(47,a,s,0,0)!=0;}

// input accessors (llama_batch_ext)
fn in_n_tok(in:i64)->i64 {return call(50,in,0,0,0);}
fn in_n_embd(in:i64)->i64 {return call(51,in,0,0,0);}
fn in_n_embd_inp(in:i64)->i64 {return call(52,in,0,0,0);}
fn in_n_embd_state(in:i64)->i64 {return call(53,in,0,0,0);}
fn in_n_seq_max(in:i64)->i64 {return call(54,in,0,0,0);}
fn in_n_vocab(in:i64)->i64 {return call(55,in,0,0,0);}
fn in_n_pos(in:i64)->i64 {return call(56,in,0,0,0);}
fn in_allow_mixed(in:i64)->i64 {return call(57,in,0,0,0);}
fn in_tok_id(in:i64,i:i64)->i64 {return call(58,in,i,0,0);}
fn in_tok_has_embd(in:i64,i:i64)->bool {return call(59,in,i,0,0)!=0;}
fn in_tok_has_state(in:i64,i:i64)->bool {return call(60,in,i,0,0)!=0;}
fn in_tok_output(in:i64,i:i64)->bool {return call(61,in,i,0,0)!=0;}
fn in_tok_decision(in:i64,i:i64)->i64 {return call(62,in,i,0,0);}
fn in_tok_pos(in:i64,i:i64,j:i64)->i64 {return call(63,in,i,j,0);}
fn in_tok_seq_n(in:i64,i:i64)->i64 {return call(64,in,i,0,0);}
fn in_tok_seq_at(in:i64,i:i64,s:i64)->i64 {return call(65,in,i,s,0);}
fn in_has_mem(in:i64)->bool {return call(66,in,0,0,0)!=0;}
fn in_mem_seq_max(in:i64,s:i64)->i64 {return call(67,in,s,0,0);}
fn in_mem_seq_min(in:i64,s:i64)->i64 {return call(68,in,s,0,0);}

fn vec_len(a:i64,k:i64)->i64 {return call(70,a,k,0,0);}
fn vec_get(a:i64,k:i64,i:i64)->i64 {return call(71,a,k,i,0);}
fn vec_set(a:i64,k:i64,i:i64,v:i64) {let ignored=call(72,a,k,i,v);}
fn vec_push(a:i64,k:i64,v:i64) {let ignored=call(73,a,k,v,0);}
fn vec_resize(a:i64,k:i64,n:i64,init:i64) {let ignored=call(74,a,k,n,init);}
fn vec_clear(a:i64,k:i64) {let ignored=call(75,a,k,0,0);}

// ubatch construction primitives; `u` is an output arena handle
fn u_new(a:i64)->i64 {return call(80,a,0,0,0);}
fn u_clear(u:i64) {let ignored=call(81,u,0,0,0);}
fn u_alloc(u:i64,nt:i64,nembd:i64,nstate:i64,ntype:i64,ndec:i64) {
    let ignored=call(82,u,nt,nembd,nstate);
    let ignored2=call(83,u,ntype,ndec,0);
}
fn u_set_token(u:i64,i:i64,v:i64) {let ignored=call(84,u,i,v,0);}
fn u_set_pos(u:i64,i:i64,v:i64) {let ignored=call(85,u,i,v,0);}
fn u_set_nsq(u:i64,i:i64,v:i64) {let ignored=call(86,u,i,v,0);}
fn u_set_output(u:i64,i:i64,v:i64) {let ignored=call(87,u,i,v,0);}
fn u_set_type(u:i64,i:i64,v:i64) {let ignored=call(88,u,i,v,0);}
fn u_set_decision(u:i64,i:i64,v:i64) {let ignored=call(89,u,i,v,0);}
fn u_set_bidx(u:i64,i:i64,v:i64) {let ignored=call(90,u,i,v,0);}
fn u_add_seq(u:i64,i:i64,sid:i64) {let ignored=call(91,u,i,sid,0);}
fn u_add_squnq(u:i64,s:i64) {let ignored=call(92,u,s,0,0);}
fn u_set_sqidx(u:i64,s:i64,v:i64) {let ignored=call(93,u,s,v,0);}
fn u_finish(u:i64,nt:i64,nseq:i64,eq:i64)->i64 {return call(94,u,nt,nseq,eq);}
fn u_n_tokens(u:i64)->i64 {return call(95,u,0,0,0);}

// ---------------------------------------------------------------------------
// clear() - llama-batch.cpp:832-862
// ---------------------------------------------------------------------------
fn clear(a:i64) {
    put(a,K_N_OUTPUTS(),0,0);
    put(a,K_N_TOK(),0,0);
    put(a,K_N_USED(),0,0);
    put(a,K_HAS_CPL(),0,0);
    vec_clear(a,V_TOKEN());vec_clear(a,V_EMBD());vec_clear(a,V_STATE());
    vec_clear(a,V_IS_EMBD());vec_clear(a,V_POS());vec_clear(a,V_NSQ());
    vec_clear(a,V_SQFLAT());vec_clear(a,V_SQOFF());vec_clear(a,V_OUTPUT());
    vec_clear(a,V_DECISION());vec_clear(a,V_SQUNQ());vec_clear(a,V_OUTIDS());
    vec_clear(a,V_USED());
    if n_seq_max(a)>0 {vec_resize(a,V_SQIDX(),n_seq_max(a),-1);}
    seq_pos_reset_all(a);cpl_clear(a);
    map_clear(a);
}

// ---------------------------------------------------------------------------
// split_reset() - llama-batch.cpp:576-583
// ---------------------------------------------------------------------------
fn split_reset(a:i64) {
    vec_clear(a,V_OUTIDS());
    set_n_used(a,0);
    vec_clear(a,V_USED());
    vec_resize(a,V_USED(),n_tok(a),0);
}

fn used_at(a:i64,i:i64)->bool {return vec_get(a,V_USED(),i)!=0;}
fn set_used(a:i64,i:i64,v:i64) {vec_set(a,V_USED(),i,v);}
fn add_used(a:i64,i:i64) {set_used(a,i,1);set_n_used(a,add(n_used(a),1));}
fn unuse(a:i64,i:i64) {ensure(n_used(a)>0);set_used(a,i,0);set_n_used(a,sub(n_used(a),1));}

// ---------------------------------------------------------------------------
// split_simple() - llama-batch.cpp:585-617
// ---------------------------------------------------------------------------
fn split_simple(a:i64,n_ubatch:i64)->i64 {
    call(135,a,0,0,0);
    var cur:i64=0;
    while cur<n_tok(a)&&used_at(a,cur) {cur=add(cur,1);}
    if cur>=n_tok(a) {return 0;}
    var idxs:i64=call(96,a,0,0,0);  // per-call idx vector handle
    call(97,idxs,0,0,0);
    while true {
        call(98,idxs,cur,0,0);      // push idx
        add_used(a,cur);
        cur=add(cur,1);
        if cur>=n_tok(a) {break;}
        if call(99,idxs,0,0,0)>=n_ubatch {break;}
    }
    return ubatch_add(a,idxs,0,0);
}

// ---------------------------------------------------------------------------
// split_seq() - llama-batch.cpp:790-830
// ---------------------------------------------------------------------------
fn split_seq(a:i64,n_ubatch:i64)->i64 {
    call(135,a,0,0,0);
    var cur:i64=0;
    while cur<n_tok(a)&&used_at(a,cur) {cur=add(cur,1);}
    if cur>=n_tok(a) {return 0;}
    var idxs:i64=call(96,a,0,0,0);
    call(97,idxs,0,0,0);
    // current sequence set is token cur
    while true {
        call(98,idxs,cur,0,0);
        add_used(a,cur);
        if call(99,idxs,0,0,0)>=n_ubatch {break;}
        var next=add(cur,1);
        while next<n_tok(a)&&(used_at(a,next)||!set_subset_ij(a,next,cur)) {next=add(next,1);}
        if next==n_tok(a) {break;}
        cur=next;
    }
    return ubatch_add(a,idxs,1,1);
}

// ---------------------------------------------------------------------------
// split_equal() - llama-batch.cpp:619-788
// ---------------------------------------------------------------------------
fn split_equal(a:i64,n_ubatch:i64,sequential:i64,n_keep_tail:i64)->i64 {
    call(135,a,0,0,0);
    if sequential!=0&&has_cpl(a) {let ignored=call(1,a,0,0,0);return 0;}
    var cur_sets:i64=call(96,a,0,0,0);call(97,cur_sets,0,0,0);  // token idx per set
    var last_seq:i64=-1;
    var i:i64=0;
    while i<n_tok(a) {
        if used_at(a,i) {i=add(i,1);continue;}
        var take=true;
        var s:i64=0;
        while s<call(99,cur_sets,0,0,0) {
            let rep=call(100,cur_sets,s,0,0);
            if set_intersects(a,rep,i) {take=false;break;}
            s=add(s,1);
        }
        if sequential!=0 {
            let sid=call(101,a,i,0,0); // seq_id[i][0], -1 if no seq
            take=take&&(call(99,cur_sets,0,0,0)==0||sid==add(last_seq,1));
        }
        if take {
            call(98,cur_sets,i,0,0);
            last_seq=call(101,a,i,0,0);
            if call(99,cur_sets,0,0,0)>n_ubatch {break;}
        }
        i=add(i,1);
    }
    var n_seqs=call(99,cur_sets,0,0,0);
    if n_seqs==0 {return 0;}
    // cur_idx[s]
    var cur_idx:i64=call(102,a,n_seqs,0,0); // int vector zeroed
    var s2:i64=0;
    while s2<n_seqs {
        let rep=call(100,cur_sets,s2,0,0);
        let slot=map_slot_of_token(a,rep);
        var ci:i64=call(103,cur_idx,s2,0,0);
        while used_at(a,map_idx_at(a,slot,ci)) {ci=add(ci,1);}
        call(104,cur_idx,s2,ci,0);
        s2=add(s2,1);
    }
    // idxs_per_seq
    var per:i64=call(105,a,n_seqs,0,0); // vector of idx-vector handles
    var t:i64=0;while t<n_seqs {call(106,per,t,call(96,a,0,0,0),0);t=add(t,1);}
    var expanding=true;
    while expanding {
        var can=true;var se:i64=0;
        while se<n_seqs {
            let rep=call(100,cur_sets,se,0,0);
            let slot=map_slot_of_token(a,rep);
            if call(103,cur_idx,se,0,0)>=map_idx_n(a,slot) {can=false;break;}
            se=add(se,1);
        }
        if !can {break;}
        let first_len=call(99,call(107,per,0,0,0),0,0,0);
        se=0;
        while se<n_seqs {
            let rep=call(100,cur_sets,se,0,0);
            let slot=map_slot_of_token(a,rep);
            let ci=call(103,cur_idx,se,0,0);
            let idx=map_idx_at(a,slot,ci);
            call(98,call(107,per,se,0,0),idx,0,0);
            add_used(a,idx);
            call(104,cur_idx,se,add(ci,1),0);
            se=add(se,1);
        }
        if mul(add(first_len,1),n_seqs)>n_ubatch {break;}
    }
    // tail retention
    if n_keep_tail>0 {
        ensure(n_ubatch>n_keep_tail);
        var n_keep:i64=0;
        var keep_going=true;
        while keep_going&&n_keep<n_seqs {
            let rep=call(100,cur_sets,n_keep,0,0);
            let slot=map_slot_of_token(a,rep);
            let remaining=sub(map_idx_n(a,slot),call(103,cur_idx,n_keep,0,0));
            if remaining!=0&&remaining<n_keep_tail {keep_going=false;}else {n_keep=add(n_keep,1);}
        }
        if n_keep==0 {
            let rep=call(100,cur_sets,0,0,0);
            let slot=map_slot_of_token(a,rep);
            let list=call(107,per,0,0,0);
            var remaining=sub(map_idx_n(a,slot),call(103,cur_idx,0,0,0));
            if add(call(99,list,0,0,0),remaining)<=n_ubatch {
                while remaining>0 {
                    let idx=map_idx_at(a,slot,call(103,cur_idx,0,0,0));
                    call(98,list,idx,0,0);add_used(a,idx);
                    call(104,cur_idx,0,add(call(103,cur_idx,0,0,0),1),0);
                    remaining=sub(remaining,1);
                }
            } else {
                while remaining<n_keep_tail {
                    ensure(call(99,list,0,0,0)>0);
                    let last=call(109,list,call(99,list,0,0,0)-1,0,0);
                    unuse(a,last);
                    call(110,list,0,0,0); // pop_back
                    call(104,cur_idx,0,sub(call(103,cur_idx,0,0,0),1),0);
                    remaining=add(remaining,1);
                }
            }
            n_keep=1;
        }
        // return deferred seqs to pool
        var ds:i64=n_keep;
        while ds<n_seqs {
            let list=call(107,per,ds,0,0);
            var k:i64=0;
            while k<call(99,list,0,0,0) {unuse(a,call(109,list,k,0,0));k=add(k,1);}
            ds=add(ds,1);
        }
        n_seqs=n_keep;
    }
    // concat
    var idxs=call(96,a,0,0,0);call(97,idxs,0,0,0);
    var s3:i64=0;
    while s3<n_seqs {
        let list=call(107,per,s3,0,0);
        var k2:i64=0;
        while k2<call(99,list,0,0,0) {call(98,idxs,call(109,list,k2,0,0),0,0);k2=add(k2,1);}
        s3=add(s3,1);
    }
    return ubatch_add(a,idxs,n_seqs,1);
}

// ---------------------------------------------------------------------------
// ubatch_reserve() - llama-batch.cpp:498-546
// ---------------------------------------------------------------------------
fn ubatch_reserve(a:i64,n_seq_tokens:i64,n_seqs:i64)->i64 {
    call(135,a,0,0,0);
    let nt=mul(n_seq_tokens,n_seqs);
    clear(a);split_reset(a);
    let u=u_new(a);
    u_alloc(u,nt,0,0,0,0);
    var s:i64=0;
    while s<n_seqs {
        u_set_sqidx(u,s,s);
        u_add_squnq(u,s);
        s=add(s,1);
    }
    return u_finish(u,nt,n_seqs,1);
}

// ---------------------------------------------------------------------------
// ubatch_add() - llama-batch.cpp:864-988
// ---------------------------------------------------------------------------
fn ubatch_add(a:i64,idxs:i64,n_seqs:i64,equal_seqs:i64)->i64 {
    let nt=call(99,idxs,0,0,0);
    ensure(n_seqs>0&&nt%n_seqs==0);
    let mixed_batch=vec_len(a,V_IS_EMBD())>0;
    var n_embd_rows:i64=0;
    if mixed_batch {var i:i64=0;while i<nt {n_embd_rows=add(n_embd_rows,vec_get(a,V_IS_EMBD(),call(109,idxs,i,0,0)));i=add(i,1);}}
    let mixed=mixed_batch&&n_embd_rows>0&&n_embd_rows<nt;
    let use_token=get(a,K_HAS_TOKEN(),0)!=0&&!(mixed_batch&&n_embd_rows==nt);
    let use_embd=get(a,K_HAS_EMBD(),0)!=0&&!(mixed_batch&&n_embd_rows==0);
    let has_state=get(a,K_HAS_STATE(),0)!=0;
    let n_embd=get(a,K_N_EMBD(),0);
    let n_state=get(a,K_N_EMBD_STATE(),0);
    let u=u_new(a);
    u_alloc(u,nt,if use_embd {mul(nt,n_embd)} else {0},if has_state {mul(nt,n_state)} else {0},if mixed {nt} else {0},if vec_len(a,V_DECISION())>0 {nt} else {0});
    var unq:i64=call(111,a,0,0,0); // temporary unique-seq set handle, cleared
    call(112,unq,0,0,0);
    var i2:i64=0;
    while i2<nt {
        let bi=call(109,idxs,i2,0,0);
        if use_token {u_set_token(u,i2,vec_get(a,V_TOKEN(),bi));}
        if use_embd {let ignored=call(119,u,bi,i2,0);}     // raw row copy
        if mixed {u_set_type(u,i2,vec_get(a,V_IS_EMBD(),bi));}
        if has_state {let ignored2=call(132,u,bi,i2,0);} // raw row copy
        var j:i64=0;
        while j<n_pos(a) {u_set_pos(u,add(mul(j,nt),i2),vec_get(a,V_POS(),add(mul(j,n_tok(a)),bi)));j=add(j,1);}
        let nsq=vec_get(a,V_NSQ(),bi);
        u_set_nsq(u,i2,nsq);
        u_set_output(u,i2,vec_get(a,V_OUTPUT(),bi));
        if vec_len(a,V_DECISION())>0 {u_set_decision(u,i2,vec_get(a,V_DECISION(),bi));}
        var s:i64=0;
        while s<nsq {
            let sid=vec_get(a,V_SQFLAT(),add(vec_get(a,V_SQOFF(),bi),s));
            u_add_seq(u,i2,sid);
            call(115,unq,sid,0,0);
            s=add(s,1);
        }
        if vec_get(a,V_OUTPUT(),bi)!=0 {vec_push(a,V_OUTIDS(),bi);}
        u_set_bidx(u,i2,bi);
        i2=add(i2,1);
    }
    var s2:i64=0;
    while s2<n_seq_max(a) {
        if call(116,unq,s2,0,0)!=0 {
            u_set_sqidx(u,s2,call(117,u,0,0,0));
            u_add_squnq(u,s2);
        }
        s2=add(s2,1);
    }
    call(118,unq,0,0,0);
    return u_finish(u,nt,n_seqs,equal_seqs);
}

// ---------------------------------------------------------------------------
// init() control logic - llama-batch.cpp:28-496
// ---------------------------------------------------------------------------
fn init(a:i64,in:i64,output_all:i64)->i64 {
    clear(a);
    put(a,K_N_EMBD(),0,if in_n_embd(in)>0 {in_n_embd(in)} else {in_n_embd_inp(in)});
    put(a,K_N_EMBD_STATE(),0,in_n_embd_state(in));
    put(a,K_N_SEQ_MAX(),0,in_n_seq_max(in));
    put(a,K_N_POS(),0,in_n_pos(in));
    let ntok=in_n_tok(in);
    if !(ntok>0) {let ignored=call(0,0,0,0,0);assert(false);}
    if in_n_seq_max(in)>max_seq() {let ignored=call(2,0,0,0,0);return 0;}
    put(a,K_N_TOK(),0,ntok);

    // content-type classification
    var n_tok_only:i64=0;var n_embd_only:i64=0;var n_both:i64=0;
    let has_state=in_tok_has_state(in,0);
    var i:i64=0;
    while i<ntok {
        let is_tok=in_tok_id(in,i)!=token_null();
        let is_emb=in_tok_has_embd(in,i);
        if !is_tok&&!is_emb {let ignored=call(2,i,0,0,0);return 0;}
        if in_tok_has_state(in,i)!=has_state {let ignored=call(2,i,0,0,0);return 0;}
        if is_tok&&!is_emb {n_tok_only=add(n_tok_only,1);}
        if is_emb&&!is_tok {n_embd_only=add(n_embd_only,1);}
        if is_tok&&is_emb {n_both=add(n_both,1);}
        i=add(i,1);
    }
    if n_both>0&&n_both!=ntok {let ignored=call(2,0,0,0,0);return 0;}
    let mixed=n_tok_only>0&&n_embd_only>0;
    if mixed&&in_allow_mixed(in)==0 {let ignored=call(2,0,0,0,0);return 0;}
    let has_token=n_tok_only>0||n_both>0;
    let has_embd=n_embd_only>0||n_both>0;
    put(a,K_MIXED(),0,if mixed {1} else {0});
    put(a,K_HAS_TOKEN(),0,if has_token {1} else {0});
    put(a,K_HAS_EMBD(),0,if has_embd {1} else {0});
    put(a,K_HAS_STATE(),0,if has_state {1} else {0});

    if mixed {vec_resize(a,V_IS_EMBD(),ntok,0);}
    if has_token {vec_resize(a,V_TOKEN(),ntok,0);}
    vec_resize(a,V_NSQ(),ntok,0);
    vec_resize(a,V_SQOFF(),ntok,0);
    vec_resize(a,V_OUTPUT(),ntok,0);
    vec_resize(a,V_POS(),mul(ntok,n_pos(a)),0);

    // token vector + mixed flag
    i=0;
    while i<ntok {
        if mixed {vec_set(a,V_IS_EMBD(),i,if in_tok_has_embd(in,i) {1} else {0});}
        if has_token {
            if mixed&&in_tok_has_embd(in,i) {vec_set(a,V_TOKEN(),i,0);}
            else {
                let id=in_tok_id(in,i);
                if id<0||id>=in_n_vocab(in) {let ignored=call(2,i,0,0,0);return 0;}
                vec_set(a,V_TOKEN(),i,id);
            }
        }
        i=add(i,1);
    }
    // embedding/state row storage: raw copies, no float math in Goose
    if has_embd {
        vec_resize(a,V_EMBD(),mul(ntok,get(a,K_N_EMBD(),0)),0);
        if mixed {
            i=0;
            while i<ntok {
                if vec_get(a,V_IS_EMBD(),i)!=0 {let ignored=call(129,a,in,i,0);}
                i=add(i,1);
            }
        } else {let ignored=call(130,a,in,0,0);}
    }
    if has_state {
        vec_resize(a,V_STATE(),mul(ntok,get(a,K_N_EMBD_STATE(),0)),0);
        let ignored=call(131,a,in,0,0);
    }
    // embeddings / state rows are raw-copied later by ubatch_add; positions:
    i=0;
    while i<ntok {
        let expand=in_tok_id(in,i)!=token_null();
        var j:i64=0;
        while j<n_pos(a) {
            var p=in_tok_pos(in,i,j);
            if expand {p=if j<3 {in_tok_pos(in,i,0)} else {0};}
            vec_set(a,V_POS(),add(mul(j,ntok),i),p);
            j=add(j,1);
        }
        i=add(i,1);
    }
    // n_seq_id / seq_id arrays
    var total:i64=0;i=0;
    while i<ntok {total=add(total,in_tok_seq_n(in,i));i=add(i,1);}
    var off:i64=0;
    i=0;
    while i<ntok {
        let nsq=in_tok_seq_n(in,i);
        vec_set(a,V_NSQ(),i,nsq);
        vec_set(a,V_SQOFF(),i,off);
        var s:i64=0;
        while s<nsq {
            let sid=in_tok_seq_at(in,i,s);
            vec_push(a,V_SQFLAT(),sid);
            if sid<0||sid>=n_seq_max(a) {let ignored=call(2,i,0,0,0);return 0;}
            s=add(s,1);
        }
        off=add(off,nsq);
        i=add(i,1);
    }
    // output/logits
    i=0;
    while i<ntok {vec_set(a,V_OUTPUT(),i,if in_tok_output(in,i) {1} else {0});i=add(i,1);}
    if output_all!=0 {
        var warn=0;
        i=0;while i<ntok {if vec_get(a,V_OUTPUT(),i)==0 {warn=1;break;}i=add(i,1);}
        if warn!=0 {i=0;while i<ntok {vec_set(a,V_OUTPUT(),i,1);i=add(i,1);}}
    }
    // decision order: allocated on first non-zero entry, values for others stay 0
    i=0;
    while i<ntok {
        let d=in_tok_decision(in,i);
        if d!=0 {
            if vec_len(a,V_DECISION())==0 {vec_resize(a,V_DECISION(),ntok,0);}
            vec_set(a,V_DECISION(),i,d);
        }
        i=add(i,1);
    }
    // counts, coupling, seq sets
    i=0;
    while i<ntok {put(a,K_N_OUTPUTS(),0,add(n_out(a),if vec_get(a,V_OUTPUT(),i)!=0 {1} else {0}));i=add(i,1);}
    set_has_cpl(a,0);
    i=0;
    while i<ntok {
        let s0=vec_get(a,V_SQFLAT(),vec_get(a,V_SQOFF(),i));
        let nsq=vec_get(a,V_NSQ(),i);
        var s:i64=0;
        while s<nsq {
            let s1=vec_get(a,V_SQFLAT(),add(vec_get(a,V_SQOFF(),i),s));
            seq_pos_insert(a,s1,vec_get(a,V_POS(),i));
            if s>0 {cpl_set(a,s1,s0);set_has_cpl(a,1);}
            s=add(s,1);
        }
        i=add(i,1);
    }
    map_clear(a);
    tok_set_clear(a,0); // no-op guard for empty token sets (bridge ignores i)
    i=0;
    while i<ntok {
        tok_set_clear(a,i);
        var s3:i64=0;
        while s3<vec_get(a,V_NSQ(),i) {
            let sid=vec_get(a,V_SQFLAT(),add(vec_get(a,V_SQOFF(),i),s3));
            tok_set_add(a,i,sid);
            s3=add(s3,1);
        }
        let slot=map_put_token(a,i);
        map_idx_push(a,slot,i);
        i=add(i,1);
    }
    vec_clear(a,V_SQUNQ());vec_resize(a,V_SQIDX(),n_seq_max(a),-1);
    var s4:i64=0;
    while s4<n_seq_max(a) {
        var any=0;i=0;
        while i<ntok {if tok_has_seq(a,i,s4) {any=1;break;}i=add(i,1);}
        if any!=0 {vec_push(a,V_SQUNQ(),s4);vec_set(a,V_SQIDX(),s4,vec_len(a,V_SQUNQ())-1);}
        s4=add(s4,1);
    }

    // consistency checks
    if n_pos(a)>1 {
        var first:i64=call(120,a,n_seq_max(a),if get(a,K_HAS_TOKEN(),0)!=0 {0} else {1},0); // per-seq int8
        if mixed {
            var seen:i64=call(121,a,n_seq_max(a),0,0);
            i=0;
            while i<ntok {
                var s5:i64=0;
                while s5<vec_get(a,V_NSQ(),i) {
                    let sid=vec_get(a,V_SQFLAT(),add(vec_get(a,V_SQOFF(),i),s5));
                    if call(122,seen,sid,0,0)==0 {
                        call(123,seen,sid,1,0);
                        call(124,first,sid,vec_get(a,V_IS_EMBD(),i),0);
                    }
                    s5=add(s5,1);
                }
                i=add(i,1);
            }
        }
        var s6:i64=0;
        while s6<n_seq_max(a) {
            if !seq_pos_empty(a,s6) {
                let p0=if in_has_mem(in) {in_mem_seq_max(in,s6)} else {-1};
                if call(125,first,s6,0,0)==0 {
                    if p0>=0&&p0>=seq_pos_min(a,s6) {let ignored=call(2,s6,p0,0,0);return 0;}
                } else {
                    if p0>=0&&p0>seq_pos_min(a,s6) {let ignored=call(2,s6,p0,0,0);return 0;}
                }
            }
            s6=add(s6,1);
        }
    } else {
        var s7:i64=0;
        while s7<n_seq_max(a) {
            if !seq_pos_empty(a,s7) {
                let p0=if in_has_mem(in) {in_mem_seq_max(in,s7)} else {-1};
                if p0>=0&&seq_pos_min(a,s7)!=add(p0,1) {let ignored=call(2,s7,p0,0,0);return 0;}
                if sub(seq_pos_max(a,s7),seq_pos_min(a,s7))+1>seq_pos_size(a,s7) {let ignored=call(2,s7,0,0,0);return 0;}
            }
            s7=add(s7,1);
        }
    }
    if in_has_mem(in) {
        var s0:i64=0;
        while s0<n_seq_max(a) {
            var s1:i64=0;
            while s1<n_seq_max(a) {
                if cpl_get(a,s0,s1) {
                    if in_mem_seq_min(in,s0)!=in_mem_seq_min(in,s1)||in_mem_seq_max(in,s0)!=in_mem_seq_max(in,s1) {
                        let ignored=call(2,s0,s1,0,0);return 0;
                    }
                }
                s1=add(s1,1);
            }
            s0=add(s0,1);
        }
    }
    // disallow partial subsets / decreasing positions
    seqset_reset_all(a);
    var curpos:i64=call(126,a,n_seq_max(a),0,0); // i64 array inited -1
    i=0;
    while i<ntok {
        let p=vec_get(a,V_POS(),i);
        var s8:i64=0;
        while s8<vec_get(a,V_NSQ(),i) {
            let sid=vec_get(a,V_SQFLAT(),add(vec_get(a,V_SQOFF(),i),s8));
            seqset_and_tok(a,sid,i);
            if seqset_none(a,sid) {let ignored=call(2,sid,0,0,0);return 0;}
            if p<call(127,curpos,sid,0,0) {let ignored=call(2,sid,0,0,0);return 0;}
            call(128,curpos,sid,p,0);
            s8=add(s8,1);
        }
        i=add(i,1);
    }
    split_reset(a);
    return 1;
}

// ---------------------------------------------------------------------------
// entry / selftest
// ---------------------------------------------------------------------------
fn selftest()->i64 {
    ensure(max_seq()>0);
    ensure(pos_max_const()==2147483647);
    return 1;
}

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
    witness(mode);
    if mode==0 {return init(a,b,c);}
    if mode==1 {return split_simple(a,b);}
    if mode==2 {return split_equal(a,b,c,d);}
    if mode==3 {return split_seq(a,b);}
    if mode==4 {return ubatch_reserve(a,b,c);}
    if mode==5 {split_reset(a);return 0;}
    if mode==6 {return n_tok(a);}
    if mode==7 {return n_used(a);}
    if mode==8 {return n_out(a);}
    if mode==9 {return vec_len(a,V_OUTIDS());}
    if mode==10 {return vec_get(a,V_OUTIDS(),b);}
    if mode==11 {return seq_pos_min(a,b);}
    if mode==12 {return seq_pos_max(a,b);}
    if mode==13 {return u_n_tokens(a);}
    if mode==14 {clear(a);return 0;}
    if mode==15 {return selftest();}
    ensure(false);
    return 0;
}

fn main() {print(entry(15,0,0,0,0,0,0,0));}
