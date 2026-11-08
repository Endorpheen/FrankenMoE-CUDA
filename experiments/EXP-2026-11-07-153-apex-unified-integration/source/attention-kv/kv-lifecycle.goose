extern "fg_w2_get" fn get(p:i64,k:i64,i:i64)->i64;
extern "fg_w2_put" fn put(p:i64,k:i64,i:i64,v:i64);
extern "fg_w2_service" fn service(op:i64,a:i64,b:i64)->i64;
extern "fg_w2_set_create" fn set_create()->i64;
extern "fg_w2_set_destroy" fn set_destroy(h:i64);
extern "fg_w2_set_insert" fn set_insert(h:i64,key:i64)->i64;
extern "fg_w2_set_erase" fn set_erase(h:i64,key:i64)->i64;
extern "fg_w2_set_first" fn set_first(h:i64)->i64;
extern "fg_w2_set_last" fn set_last(h:i64)->i64;
extern "fg_w2_set_size" fn set_size(h:i64)->i64;
extern "fg_w2_set_key" fn set_key(h:i64,it:i64)->i64;

extern "fg_kv_get" fn kv_get(h:i64,k:i64,i:i64)->i64;
extern "fg_kv_put" fn kv_put(h:i64,k:i64,i:i64,v:i64);
extern "fg_kv_service" fn kv_service(op:i64,a:i64,b:i64)->i64;
extern "fg_kv_obj" fn kv_obj(kind:i64)->i64;
extern "fg_kv_create" fn kv_create_raw(n_stream:i64,kv_size:i64)->i64;
extern "fg_kv_destroy" fn kv_destroy_raw(h:i64);
extern "fg_kv_vec_push" fn kv_vec_push(v:i64,item:i64);
extern "fg_kv_slot_push" fn kv_slot_push(s:i64,strm_idx:i64,cell:i64);
extern "fg_kv_slot_len" fn kv_slot_len(s:i64,strm_idx:i64)->i64;
extern "fg_kv_slot_clear" fn kv_slot_clear(s:i64,strm_idx:i64);
extern "fg_kv_swa_masked" fn kv_swa_masked(kv:i64,n_swa:i64,swa_type:i64,p0:i64,p1:i64)->i64;
extern "fg_kv_sync" fn kv_sync(kv:i64,lctx:i64);
extern "fg_kv_tensor_copy" fn kv_tensor_copy(kv:i64,src:i64,dst:i64,il:i64,is_v:i64);
extern "fg_kv_shift_compute" fn kv_shift_compute(kv:i64,lctx:i64)->i64;
extern "fg_kv_zero_cells" fn fg_kv_zero_cells(kv:i64,strm:i64,sinfo:i64,contiguous:i64);
extern "fg_kv_clear_data" fn fg_kv_clear_data(kv:i64);
extern "fg_state_write_u32" fn state_w_u32(io:i64,v:i64);
extern "fg_state_read_u32" fn state_r_u32(io:i64)->i64;
extern "fg_state_write_i32" fn state_w_i32(io:i64,v:i64);
extern "fg_state_read_i32" fn state_r_i32(io:i64)->i64;
extern "fg_state_write_u64" fn state_w_u64(io:i64,v:i64);
extern "fg_state_read_u64" fn state_r_u64(io:i64)->i64;
extern "fg_state_v_trans" fn fg_state_v_trans(kv:i64)->i64;
extern "fg_state_n_rot" fn fg_state_n_rot(kv:i64,is_v:i64)->i64;
extern "fg_state_has_cell_ext" fn fg_state_has_cell_ext(kv:i64)->i64;
extern "fg_state_layer_il" fn fg_state_layer_il(kv:i64,idx:i64)->i64;
extern "fg_state_layer_has_v" fn fg_state_layer_has_v(kv:i64,idx:i64)->i64;
extern "fg_state_n_embd_k" fn fg_state_n_embd_k(kv:i64,il:i64)->i64;
extern "fg_state_n_embd_v" fn fg_state_n_embd_v(kv:i64,il:i64)->i64;
extern "fg_state_tensor_type" fn fg_state_tensor_type(kv:i64,strm:i64,idx:i64,is_v:i64)->i64;
extern "fg_state_tensor_row_size" fn fg_state_tensor_row_size(kv:i64,strm:i64,idx:i64,is_v:i64)->i64;
extern "fg_state_tensor_size_el" fn fg_state_tensor_size_el(kv:i64,strm:i64,idx:i64,is_v:i64)->i64;
extern "fg_state_write_rows" fn fg_state_write_rows(kv:i64,io:i64,strm:i64,idx:i64,is_v:i64,from:i64,to:i64);
extern "fg_state_read_rows" fn fg_state_read_rows(kv:i64,io:i64,strm:i64,idx:i64,is_v:i64,from:i64,to:i64);
extern "fg_state_write_trans" fn fg_state_write_trans(kv:i64,io:i64,strm:i64,idx:i64,j:i64,from:i64,to:i64);
extern "fg_state_read_trans" fn fg_state_read_trans(kv:i64,io:i64,strm:i64,idx:i64,j:i64,from:i64,to:i64);
extern "fg_state_fail" fn fg_state_fail(code:i64);
extern "fg_sinfos_count" fn fg_sinfos_count(p:i64)->i64;
extern "fg_sinfos_init" fn fg_sinfos_init(p:i64,n:i64);
extern "fg_sinfos_in_empty" fn fg_sinfos_in_empty(p:i64,s:i64)->i64;
extern "fg_sinfos_in_to_slot" fn fg_sinfos_in_to_slot(p:i64,s:i64,slot:i64);
extern "fg_sinfos_set_out" fn fg_sinfos_set_out(p:i64,s:i64,slot:i64);
extern "fg_kv_test_ub_set_token" fn fg_kv_test_ub_set_token(u:i64,i:i64,v:i64);

extern "fg_kv_ub_n_tokens" fn ub_n_tokens(u:i64)->i64;
extern "fg_kv_ub_n_seqs_unq" fn ub_n_seqs_unq(u:i64)->i64;
extern "fg_kv_ub_seq_id_unq" fn ub_seq_id_unq(u:i64,s:i64)->i64;
extern "fg_kv_ub_pos" fn ub_pos(u:i64,i:i64)->i64;
extern "fg_kv_ub_n_seq_id" fn ub_n_seq_id(u:i64,i:i64)->i64;
extern "fg_kv_ub_seq_id" fn ub_seq_id(u:i64,i:i64,j:i64)->i64;
extern "fg_kv_ub_token" fn ub_token(u:i64,i:i64)->i64;
extern "fg_kv_ub_has_token" fn ub_has_token(u:i64)->i64;
extern "fg_kv_ub_is_pos_2d" fn ub_is_pos_2d(u:i64)->i64;
extern "fg_kv_ub_is_mixed" fn ub_is_mixed(u:i64)->i64;
extern "fg_kv_ub_type" fn ub_type(u:i64,i:i64)->i64;
extern "fg_kv_ub_destroy" fn ub_destroy(u:i64);
extern "fg_kv_balloc_reset" fn balloc_reset(b:i64);
extern "fg_kv_balloc_split" fn balloc_split(b:i64,n:i64,kind:i64)->i64;
extern "fg_kv_balloc_n_used" fn balloc_n_used(b:i64)->i64;
extern "fg_kv_balloc_n_tokens" fn balloc_n_tokens(b:i64)->i64;
extern "fg_kv_test_ubatch" fn fg_kv_test_ubatch(n_tokens:i64)->i64;
extern "fg_kv_test_ub_set_pos" fn fg_kv_test_ub_set_pos(u:i64,i:i64,v:i64);
extern "fg_kv_test_ub_set_seq" fn fg_kv_test_ub_set_seq(u:i64,i:i64,j:i64,seq:i64);
extern "fg_kv_test_ub_set_seq_unq" fn fg_kv_test_ub_set_seq_unq(u:i64,s:i64,seq:i64);
extern "fg_kv_test_balloc" fn fg_kv_test_balloc(u:i64)->i64;
extern "fg_kv_test_ubatch_free" fn fg_kv_test_ubatch_free(u:i64);
extern "fg_kv_balloc_destroy" fn fg_kv_balloc_destroy(b:i64);

fn ensure(ok:bool) {if !ok {let ignored=kv_service(0,0,0);assert(false);}}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0);ensure(a==0||b<=9223372036854775807/a);return a*b;}
fn max_seq()->i64 {let m=get(0,900,0);ensure(m>0);return m;}
fn tok_null()->i64 {return get(0,901,0);}
fn kv_cells(kv:i64,s:i64)->i64 {return kv_get(kv,10+s,0);}
fn kv_head(kv:i64,s:i64)->i64 {return kv_get(kv,100+s,0);}
fn kv_set_head(kv:i64,s:i64,v:i64) {kv_put(kv,100+s,0,v);}
fn kv_stream(kv:i64,seq:i64)->i64 {let n=kv_get(kv,0,0);if n==1 {return 0;}ensure(seq>=0&&seq<n);return seq;}
fn kv_n_stream(kv:i64)->i64 {return kv_get(kv,0,0);}

fn lc_n(c:i64)->i64 {return get(c,0,0);}
fn lc_check(c:i64,i:i64) {ensure(c!=0);ensure(i>=0&&i<lc_n(c));}
fn lc_seq_read(c:i64,i:i64,s:i64)->i64 {return get(c,1000+s,i);}
fn lc_seq_write(c:i64,i:i64,s:i64,v:i64) {put(c,1000+s,i,v);}
fn lc_seq_clear(c:i64,i:i64) {let m=max_seq();for s in m {lc_seq_write(c,i,s,0);}}
fn lc_seq_any(c:i64,i:i64)->i64 {let m=max_seq();var a:i64=0;for s in m {if lc_seq_read(c,i,s)!=0 {a=1;}}return a;}
fn lc_seq_total(c:i64,i:i64)->i64 {let m=max_seq();var t:i64=0;for s in m {if lc_seq_read(c,i,s)!=0 {t+=1;}}return t;}
fn lc_cell_clear(c:i64,i:i64) {put(c,10,i,-1);put(c,11,i,0);put(c,12,i,0);put(c,13,i,0);put(c,14,i,tok_null());lc_seq_clear(c,i);}
fn lc_is_empty(c:i64,i:i64)->i64 {lc_check(c,i);if get(c,10,i)==-1 {return 1;}return 0;}
fn lc_pos_get(c:i64,i:i64)->i64 {lc_check(c,i);let v=get(c,10,i);ensure(v!=-1);return v;}
fn lc_get_shift(c:i64,i:i64)->i64 {lc_check(c,i);ensure(get(c,10,i)!=-1);return get(c,11,i);}
fn lc_seq_count(c:i64,i:i64)->i64 {lc_check(c,i);ensure(get(c,10,i)!=-1);return lc_seq_total(c,i);}
fn lc_seq_has(c:i64,i:i64,s:i64)->i64 {lc_check(c,i);if lc_seq_read(c,i,s)!=0 {return 1;}return 0;}
fn lc_seq_get_single(c:i64,i:i64)->i64 {lc_check(c,i);ensure(lc_seq_total(c,i)==1);let m=max_seq();for s in m {if lc_seq_read(c,i,s)!=0 {return s;}}return -1;}
fn lc_pos_in(c:i64,i:i64,p0:i64,p1:i64)->i64 {let v=get(c,10,i);if v>=p0&&v<p1 {return 1;}return 0;}
fn lc_used_count(c:i64)->i64 {return set_size(get(c,20,0));}
fn lc_used_max_p1(c:i64)->i64 {let h=get(c,20,0);if set_size(h)==0 {return 0;}return set_key(h,set_last(h))+1;}
fn lc_seq_key(pos:i64,cell:i64)->i64 {ensure(pos>=0&&pos<2147483648);ensure(cell>=0&&cell<4294967296);return mul(pos,4294967296)+cell;}
fn lc_seq_pos_min(c:i64,s:i64)->i64 {let h=get(c,25+s,0);if set_size(h)==0 {return -1;}let k=set_key(h,set_first(h));return k/4294967296;}
fn lc_seq_pos_max(c:i64,s:i64)->i64 {let h=get(c,25+s,0);if set_size(h)==0 {return -1;}let k=set_key(h,set_last(h));return k/4294967296;}
fn lc_pos_set(c:i64,i:i64,v:i64) {lc_check(c,i);ensure(get(c,10,i)==-1);ensure(lc_seq_any(c,i)==0);put(c,10,i,v);let ignored=set_insert(get(c,20,0),i);}
fn lc_seq_add(c:i64,i:i64,s:i64) {lc_check(c,i);ensure(s>=0&&s<max_seq());ensure(get(c,10,i)!=-1);ensure(lc_seq_read(c,i,s)==0);lc_seq_write(c,i,s,1);let ignored=set_insert(get(c,25+s,0),lc_seq_key(get(c,10,i),i));}
fn lc_ext_set(c:i64,i:i64,x:i64,y:i64,t:i64) {lc_check(c,i);put(c,12,i,x);put(c,13,i,y);put(c,14,i,t);}
fn lc_ext_x(c:i64,i:i64)->i64 {return get(c,12,i);}
fn lc_ext_y(c:i64,i:i64)->i64 {return get(c,13,i);}
fn lc_ext_tok(c:i64,i:i64)->i64 {return get(c,14,i);}
fn lc_seq_pos_rm_cell(c:i64,i:i64) {let m=max_seq();let v=get(c,10,i);for s in m {if lc_seq_read(c,i,s)!=0 {let ignored=set_erase(get(c,25+s,0),lc_seq_key(v,i));}}}
fn lc_rm(c:i64,i:i64) {lc_check(c,i);ensure(get(c,10,i)!=-1);lc_seq_pos_rm_cell(c,i);lc_cell_clear(c,i);let ignored=set_erase(get(c,20,0),i);}
fn lc_reset_shift(c:i64) {let n=lc_n(c);for i in n {put(c,11,i,0);}put(c,1,0,0);}
fn lc_reset(c:i64) {let n=lc_n(c);for i in n {lc_cell_clear(c,i);}put(c,1,0,0);set_destroy(get(c,20,0));put(c,20,0,set_create());let m=max_seq();for s in m {set_destroy(get(c,25+s,0));put(c,25+s,0,set_create());}}
fn lc_create(n:i64)->i64 {let c=service(1,n,0);ensure(c!=0);put(c,0,0,n);lc_reset(c);return c;}
fn lc_destroy(c:i64) {if c!=0 {set_destroy(get(c,20,0));let m=max_seq();for s in m {set_destroy(get(c,25+s,0));}let ignored=service(2,c,0);}}
fn lc_seq_rm_cell_range(kv:i64,seq:i64,p0:i64,p1:i64) {let s=kv_stream(kv,seq);let c=kv_cells(kv,s);let n=lc_n(c);var new_head=n;var i:i64=0;while i<n {if lc_pos_in(c,i,p0,p1)!=0&&lc_seq_has(c,i,seq)!=0 {lc_seq_write(c,i,seq,0);let ignored=set_erase(get(c,25+seq,0),lc_seq_key(get(c,10,i),i));if lc_seq_any(c,i)==0 {lc_cell_clear(c,i);let ignored2=set_erase(get(c,20,0),i);if new_head==n {new_head=i;}}}i+=1;}if new_head!=n {let h=kv_head(kv,s);if new_head<h {kv_set_head(kv,s,new_head);}}}

fn kv_size(kv:i64)->i64 {return lc_n(kv_cells(kv,0));}
fn kv_has_shift(kv:i64)->i64 {let n=kv_n_stream(kv);var r:i64=0;for s in n {if get(kv_cells(kv,s),1,0)!=0 {r=1;}}return r;}
fn kv_can_shift(kv:i64)->i64 {if kv_get(kv,5,0)!=0 {return 0;}if kv_get(kv,6,0)>1 {return 0;}return 1;}

fn slot_n_stream(s:i64)->i64 {return kv_get(s,3,0);}
fn slot_strm(s:i64,x:i64)->i64 {return kv_get(s,10+x,0);}
fn slot_size(s:i64)->i64 {return kv_slot_len(s,0);}
fn slot_idx(s:i64,x:i64,j:i64)->i64 {return kv_get(s,100+x,j);}

fn kv_find_slot(kv:i64,u:i64,cont:i64)->i64 {
 let nstream=kv_n_stream(kv);
 var n_tokens=ub_n_tokens(u);
 var n_seqs:i64=1;
 if nstream>1 {let unq=ub_n_seqs_unq(u);ensure(unq>0);ensure(n_tokens%unq==0);n_seqs=unq;n_tokens=n_tokens/unq;}
 let res=kv_obj(3);
 kv_put(res,0,0,2147483647);
 kv_put(res,1,0,0);
 kv_put(res,2,0,n_tokens);
 kv_put(res,3,0,n_seqs);
 var sx:i64=0;
 while sx<n_seqs {
   let seq_id=ub_seq_id_unq(u,sx);
   if nstream>1 {ensure(ub_n_seq_id(u,sx*n_tokens)==1);ensure(ub_seq_id(u,sx*n_tokens,0)==seq_id);}
   let strm=kv_stream(kv,seq_id);
   if strm<kv_get(res,0,0) {kv_put(res,0,0,strm);}
   if strm>kv_get(res,1,0) {kv_put(res,1,0,strm);}
   kv_put(res,10+sx,0,strm);
   let c=kv_cells(kv,strm);
   var head=kv_head(kv,strm);
   if head>lc_used_count(c)+2*n_tokens {head=0;}
   if n_tokens>lc_n(c) {kv_destroy_raw(res);return 0;}
   var tested:i64=0;
   var ntest:i64=1;
   if cont!=0 {ntest=n_tokens;}
   var done:i64=0;
   while done==0 {
     let ncell=lc_n(c);
     if head+ntest>ncell {tested=tested+ncell-head;head=0;} else {
       var i:i64=0;
       while i<ntest {
         let idx=head;
         head+=1;
         tested+=1;
         var can=lc_is_empty(c,idx);
         if can==0&&lc_seq_count(c,idx)==1 {
           let pos_cell=lc_pos_get(c,idx);
           if can==0 {
             let sq=lc_seq_get_single(c,idx);
             let n_swa=kv_get(kv,3,0);
             var masked:i64=0;
             if n_swa>0 {masked=kv_swa_masked(kv,n_swa,kv_get(kv,4,0),pos_cell,lc_seq_pos_max(c,sq)+1);}
             if masked!=0 {can=1;}
           }
         }
         if can!=0 {kv_slot_push(res,sx,idx);} else {if cont!=0 {break;}}
         i+=1;
       }
       if kv_slot_len(res,sx)==n_tokens {done=1;} else {
         if cont!=0 {kv_slot_clear(res,sx);}
         if tested>=ncell {kv_destroy_raw(res);return 0;}
       }
     }
   }
   if kv_slot_len(res,sx)<n_tokens {kv_destroy_raw(res);return 0;}
   sx+=1;
 }
 ensure(kv_get(res,1,0)>=kv_get(res,0,0));
 return res;
}

fn kv_snapshot_cells(c:i64,sinfo:i64,x:i64)->i64 {
 let cnt=slot_size(sinfo);
 let snap=lc_create(cnt);
 var j:i64=0;
 while j<cnt {
   let idx=slot_idx(sinfo,x,j);
   ensure(get(c,11,idx)==0);
   if lc_is_empty(c,idx)==0 {
     lc_pos_set(snap,j,lc_pos_get(c,idx));
     lc_ext_set(snap,j,lc_ext_x(c,idx),lc_ext_y(c,idx),lc_ext_tok(c,idx));
     let m=max_seq();
     for q in m {if lc_seq_has(c,idx,q)!=0 {lc_seq_add(snap,j,q);}}
   }
   j+=1;
 }
 return snap;
}

fn kv_restore_cells(c:i64,snap:i64,sinfo:i64,x:i64) {
 let cnt=slot_size(sinfo);
 var j:i64=0;
 while j<cnt {
   let idx=slot_idx(sinfo,x,j);
   ensure(get(c,11,idx)==0);
   if lc_is_empty(c,idx)==0 {lc_rm(c,idx);}
   if lc_is_empty(snap,j)==0 {
     lc_pos_set(c,idx,lc_pos_get(snap,j));
     lc_ext_set(c,idx,lc_ext_x(snap,j),lc_ext_y(snap,j),lc_ext_tok(snap,j));
     let m=max_seq();
     for q in m {if lc_seq_has(snap,j,q)!=0 {lc_seq_add(c,idx,q);}}
   }
   j+=1;
 }
}

fn kv_prepare(kv:i64,ubatches:i64)->i64 {
 let res=kv_obj(4);
 let states=kv_obj(4);
 var success:i64=1;
 var k:i64=0;
 let nueb=kv_get(ubatches,0,0);
 while k<nueb {
   let u=kv_get(ubatches,10+k,0);
   let sinfo=kv_find_slot(kv,u,0);
   if sinfo==0 {success=0;k=nueb;} else {
     kv_vec_push(res,sinfo);
     let st=kv_obj(5);
     kv_put(st,0,0,sinfo);
     let ns=slot_n_stream(sinfo);
     let nstr=kv_n_stream(kv);
     var s:i64=0;
     while s<nstr {kv_put(st,10+s,0,kv_head(kv,s));s+=1;}
     s=0;
     while s<ns {
       let strm=slot_strm(sinfo,s);
       let c=kv_cells(kv,strm);
       let snap=kv_snapshot_cells(c,sinfo,s);
       kv_put(st,100+s,0,snap);
       s+=1;
     }
     kv_vec_push(states,st);
     kv_apply_ubatch(kv,sinfo,u);
     k+=1;
   }
 }
 var j=kv_get(states,0,0);
 while j>0 {
   j-=1;
   let st=kv_get(states,10+j,0);
   let sinfo=kv_get(st,0,0);
   let ns=slot_n_stream(sinfo);
   var s:i64=0;
   while s<ns {
     let strm=slot_strm(sinfo,s);
     let c=kv_cells(kv,strm);
     let snap=kv_get(st,100+s,0);
     kv_restore_cells(c,snap,sinfo,s);
     kv_set_head(kv,strm,kv_get(st,10+s,0));
     s+=1;
   }
   kv_destroy_raw(st);
 }
 if success==0 {kv_destroy_raw(res);kv_destroy_raw(states);return 0;}
 kv_destroy_raw(states);
 return res;
}

fn kv_apply_ubatch(kv:i64,sinfo:i64,u:i64) {
 if kv_get(kv,62,0)!=0 {return;}
 let rm=kv_obj(4);
 let m=max_seq();
 for q in m {kv_put(rm,1000+q,0,-1);}
 let nstr=kv_n_stream(kv);
 let ns=slot_n_stream(sinfo);
 let sz=slot_size(sinfo);
 ensure(ub_n_tokens(u)==ns*sz);
 var s:i64=0;
 while s<ns {
   let strm=slot_strm(sinfo,s);
   let c=kv_cells(kv,strm);
   var ii:i64=0;
   while ii<sz {
     let i=s*sz+ii;
     let idx=slot_idx(sinfo,s,ii);
     if lc_is_empty(c,idx)==0 {
       ensure(lc_seq_count(c,idx)==1);
       let sq=lc_seq_get_single(c,idx);
       let p=lc_pos_get(c,idx);
       if p>kv_get(rm,1000+sq,0) {kv_put(rm,1000+sq,0,p);}
       lc_rm(c,idx);
     }
     lc_pos_set(c,idx,ub_pos(u,i));
     let has_tok=ub_has_token(u);
     let is2d=ub_is_pos_2d(u);
     let ple=kv_get(kv,8,0);
     if is2d!=0||has_tok!=0||ple>0 {
       var x:i64=0;
       var y:i64=0;
       var tk:i64=tok_null();
       if is2d!=0 {x=ub_pos(u,i+ub_n_tokens(u)*2);y=ub_pos(u,i+ub_n_tokens(u));}
       var embd:i64=0;
       if has_tok==0 {embd=1;} else {if ub_is_mixed(u)!=0 {embd=ub_type(u,i);}}
       if embd==0 {tk=ub_token(u,i);} else {if ple>0 {let img=kv_get(kv,9,0);if img!=0 {tk=img;}else {tk=kv_get(kv,60,0);}}}
       lc_ext_set(c,idx,x,y,tk);
     }
     let nid=ub_n_seq_id(u,i);
     var jj:i64=0;
     while jj<nid {lc_seq_add(c,idx,ub_seq_id(u,i,jj));jj+=1;}
     ii+=1;
   }
   s+=1;
 }
 let m2=max_seq();
 for sq in m2 {
   let v=kv_get(rm,1000+sq,0);
   if v!=-1 {
     let strm2=kv_stream(kv,sq);
     let c2=kv_cells(kv,strm2);
     let pmin=lc_seq_pos_min(c2,sq);
     if pmin<=v {lc_seq_rm_cell_range(kv,sq,pmin,v+1);}
   }
 }
 s=0;
 while s<ns {
   let strm=slot_strm(sinfo,s);
   let cnt=kv_slot_len(sinfo,s);
   kv_set_head(kv,strm,slot_idx(sinfo,s,cnt-1)+1);
   s+=1;
 }
 kv_destroy_raw(rm);
}

fn align_up_kv(v:i64,a:i64)->i64 {ensure(a>0);return ((v+a-1)/a)*a;}
fn kv_n_kv(kv:i64,sinfo:i64)->i64 {
 let n_pad=kv_get(kv,2,0);
 var npc=n_pad;
 if npc<256 {npc=256;}
 var result:i64=0;
 let ns=slot_n_stream(sinfo);
 var s:i64=0;
 while s<ns {
   let c=kv_cells(kv,slot_strm(sinfo,s));
   let n=lc_n(c);
   var p=align_up_kv(lc_used_max_p1(c),npc);
   if p<npc {p=npc;}
   var v=n;
   if p<v {v=p;}
   if v>result {result=v;}
   s+=1;
 }
 return result;
}

fn kv_ctx_error(status:i64)->i64 {let c=kv_obj(1);kv_put(c,0,0,status);kv_put(c,1,0,0);kv_put(c,3,0,0);kv_put(c,4,0,0);kv_put(c,6,0,0);kv_put(c,7,0,0);return c;}
fn kv_ctx_full(kv:i64)->i64 {
 let c=kv_ctx_error(0);
 kv_put(c,1,0,kv);
 kv_put(c,4,0,kv_size(kv));
 let n=kv_n_stream(kv);
 let slot=kv_obj(3);
 kv_put(slot,0,0,0);
 kv_put(slot,1,0,n-1);
 kv_put(slot,2,0,1);
 kv_put(slot,3,0,n);
 var s:i64=0;
 while s<n {kv_put(slot,10+s,0,s);kv_slot_push(slot,s,0);s+=1;}
 kv_put(c,7,0,1);
 kv_put(c,200,0,slot);
 return c;
}
fn kv_ctx_update(kv:i64,lctx:i64,do_shift:i64)->i64 {
 let c=kv_ctx_error(0);
 kv_put(c,1,0,kv);
 kv_put(c,2,0,lctx);
 kv_put(c,5,0,do_shift);
   let nsc=kv_get(kv,65,0);
 kv_put(c,8,0,nsc);
 var i:i64=0;
 while i<nsc {kv_put(c,300+i,0,kv_get(kv,200+i,0));kv_put(c,400+i,0,kv_get(kv,300+i,0));i+=1;}
 kv_put(kv,65,0,0);
 if do_shift==0&&nsc==0 {kv_put(c,0,0,1);}
 return c;
}
fn kv_ctx_batch(kv:i64,sinfos:i64,ubs:i64)->i64 {
 let c=kv_ctx_error(0);
 kv_put(c,1,0,kv);
 let ns=kv_get(sinfos,0,0);
 let nu=kv_get(ubs,0,0);
 kv_put(c,7,0,ns);
 kv_put(c,6,0,nu);
 var i:i64=0;
 while i<nu {kv_put(c,100+i,0,kv_get(ubs,10+i,0));i+=1;}
 i=0;
 while i<ns {kv_put(c,200+i,0,kv_get(sinfos,10+i,0));i+=1;}
 return c;
}
fn kv_init_batch(kv:i64,b:i64,n_ubatch:i64,embd_all:i64)->i64 {
 balloc_reset(b);
 let ubs=kv_obj(4);
 var kind:i64=2;
 if kv_n_stream(kv)==1 {kind=0;} else {if embd_all!=0 {kind=1;}}
 var go:i64=1;
 while go==1 {
   let u=balloc_split(b,n_ubatch,kind);
   if ub_n_tokens(u)==0 {ub_destroy(u);go=0;} else {kv_vec_push(ubs,u);}
 }
 var ok:i64=1;
 if balloc_n_used(b)<balloc_n_tokens(b) {ok=0;}
 if ok!=0 {
   let sinfos=kv_prepare(kv,ubs);
   if sinfos!=0 {let ctx=kv_ctx_batch(kv,sinfos,ubs);kv_destroy_raw(sinfos);kv_destroy_raw(ubs);return ctx;}
   ok=0;
 }
 let nu=kv_get(ubs,0,0);
 var i:i64=0;
 while i<nu {ub_destroy(kv_get(ubs,10+i,0));i+=1;}
 kv_destroy_raw(ubs);
 return kv_ctx_error(2);
}
fn kv_init_full(kv:i64)->i64 {return kv_ctx_full(kv);}
fn kv_init_update(kv:i64,lctx:i64)->i64 {return kv_ctx_update(kv,lctx,kv_has_shift(kv));}

fn kv_update(kv:i64,lctx:i64,do_shift:i64,sc_count:i64,sc_ctx:i64)->i64 {
 if kv_get(kv,62,0)!=0 {return 1;}
 var updated:i64=0;
 let nstream=kv_n_stream(kv);
 if sc_count>0 {
   ensure(nstream>1);
   ensure(sc_ctx!=0);
   kv_sync(kv,lctx);
   let n_layers=kv_get(kv,7,0);
   var i:i64=0;
   while i<sc_count {
     let src=kv_get(sc_ctx,300+i,0);
     let dst=kv_get(sc_ctx,400+i,0);
     ensure(src>=0&&src<nstream);
     ensure(dst>=0&&dst<nstream);
     ensure(src!=dst);
     var il:i64=0;
     while il<n_layers {
       kv_tensor_copy(kv,src,dst,il,0);
       if kv_get(kv,400+il,0)!=0 {kv_tensor_copy(kv,src,dst,il,1);}
       il+=1;
     }
     i+=1;
   }
 }
 if do_shift!=0 {
   ensure(kv_can_shift(kv)!=0);
   if kv_get(kv,61,0)==0 {
     var anyrope:i64=0;
     let n_layers=kv_get(kv,7,0);
     var il:i64=0;
     while il<n_layers {if kv_get(kv,500+il,0)!=0 {anyrope=1;}il+=1;}
     if anyrope!=0 {
       let st=kv_shift_compute(kv,lctx);
       if st!=0 {return updated;}
       updated=1;
     }
   }
   var s:i64=0;
   while s<nstream {lc_reset_shift(kv_cells(kv,s));s+=1;}
 }
 return updated;
}

fn ctx_status(c:i64)->i64 {return kv_get(c,0,0);}
fn ctx_next(c:i64)->i64 {ensure(ctx_status(c)==0);let i=kv_get(c,3,0)+1;kv_put(c,3,0,i);if i>=kv_get(c,6,0) {return 0;}return 1;}
fn ctx_apply(c:i64)->i64 {
 let st=ctx_status(c);
 ensure(!(st==2||st==3));
 let kv=kv_get(c,1,0);
 let nu=kv_get(c,6,0);
 if nu==0 {
   kv_update(kv,kv_get(c,2,0),kv_get(c,5,0),kv_get(c,8,0),c);
   return 1;
 }
 let i=kv_get(c,3,0);
 let sinfo=kv_get(c,200+i,0);
 let u=kv_get(c,100+i,0);
 kv_apply_ubatch(kv,sinfo,u);
 kv_put(c,4,0,kv_n_kv(kv,sinfo));
 return 1;
}
fn ctx_get_status(c:i64)->i64 {return ctx_status(c);}
fn ctx_get_ubatch(c:i64)->i64 {ensure(ctx_status(c)==0);return kv_get(c,100+kv_get(c,3,0),0);}
fn ctx_get_n_kv(c:i64)->i64 {return kv_get(c,4,0);}
fn ctx_type_k(c:i64)->i64 {return kv_get(kv_get(c,1,0),63,0);}
fn ctx_type_v(c:i64)->i64 {return kv_get(kv_get(c,1,0),64,0);}

fn kv_ctx_destroy(c:i64) {
 if c!=0 {
   let nu=kv_get(c,6,0);
   var i:i64=0;
   while i<nu {let u=kv_get(c,100+i,0);if u!=0 {ub_destroy(u);}i+=1;}
   let ns=kv_get(c,7,0);
   i=0;
   while i<ns {let sl=kv_get(c,200+i,0);if sl!=0 {kv_destroy_raw(sl);}i+=1;}
   kv_destroy_raw(c);
 }
}
fn kv_create(n_stream:i64,kv_size:i64)->i64 {
 ensure(n_stream>=1);
 let kv=kv_create_raw(n_stream,kv_size);
 var s:i64=0;
 while s<n_stream {let c=kv_cells(kv,s);put(c,0,0,kv_size);lc_reset(c);kv_set_head(kv,s,0);s+=1;}
 return kv;
}
fn kv_destroy(kv:i64) {if kv!=0 {let n=kv_n_stream(kv);var s:i64=0;while s<n {lc_destroy(kv_cells(kv,s));s+=1;}kv_destroy_raw(kv);}}

fn kv_seq_ok(kv:i64,seq:i64)->i64 {if seq<0 {return 0;}let n=kv_n_stream(kv);if n==1 {if seq<max_seq() {return 1;}return 0;}if seq<n {return 1;}return 0;}
fn state_write_data(kv:i64,io:i64,strm:i64,layers:i64,ranges:i64) {
 let nr=kv_get(ranges,0,0);
 state_w_u32(io,fg_state_v_trans(kv));
 state_w_u32(io,layers);
 state_w_u32(io,fg_state_n_rot(kv,0));
 state_w_u32(io,fg_state_n_rot(kv,1));
 var lay:i64=0;
 while lay<layers {
   state_w_i32(io,fg_state_tensor_type(kv,strm,lay,0));
   state_w_u64(io,fg_state_tensor_row_size(kv,strm,lay,0));
   var r:i64=0;
   while r<nr {let f=kv_get(ranges,10+r,0);let t=kv_get(ranges,11+r,0);fg_state_write_rows(kv,io,strm,lay,0,f,t);r+=2;}
   lay+=1;
 }
 if fg_state_v_trans(kv)==0 {
   lay=0;
   while lay<layers {
     if fg_state_layer_has_v(kv,lay)!=0 {
       state_w_i32(io,fg_state_tensor_type(kv,strm,lay,1));
       state_w_u64(io,fg_state_tensor_row_size(kv,strm,lay,1));
       var r:i64=0;
       while r<nr {let f=kv_get(ranges,10+r,0);let t=kv_get(ranges,11+r,0);fg_state_write_rows(kv,io,strm,lay,1,f,t);r+=2;}
     }
     lay+=1;
   }
 } else {
   lay=0;
   while lay<layers {
     if fg_state_layer_has_v(kv,lay)!=0 {
       let il=fg_state_layer_il(kv,lay);
       state_w_i32(io,fg_state_tensor_type(kv,strm,lay,1));
       state_w_u32(io,fg_state_tensor_size_el(kv,strm,lay,1));
       let nv=fg_state_n_embd_v(kv,il);
       state_w_u32(io,nv);
       var j:i64=0;
       while j<nv {
         var r:i64=0;
         while r<nr {let f=kv_get(ranges,10+r,0);let t=kv_get(ranges,11+r,0);fg_state_write_trans(kv,io,strm,lay,j,f,t);r+=2;}
         j+=1;
       }
     }
     lay+=1;
   }
 }
}
fn kv_state_write(kv:i64,io:i64,seq:i64,flags:i64) {
 if kv_get(kv,62,0)!=0 {return;}
 if (flags&1)!=0&&kv_get(kv,4,0)==0 {return;}
 let nstream=kv_n_stream(kv);
 let ext=fg_state_has_cell_ext(kv);
 let nsmax=kv_get(kv,1,0);
 let nl=kv_get(kv,7,0);
 state_w_u32(io,nstream);
 var s:i64=0;
 while s<nstream {
   let c=kv_cells(kv,s);
   let n=lc_n(c);
   let ranges=kv_obj(4);
   var count:i64=0;
   var begin:i64= -1;
   var i:i64=0;
   while i<n {
     var add:i64=0;
     if lc_is_empty(c,i)==0 {
       if seq==-1 {add=1;} else {if lc_seq_has(c,i,seq)!=0 {add=1;}}
     }
     if add!=0&&seq!=-1 {
       let n_swa=kv_get(kv,3,0);
       if n_swa>0 {if kv_swa_masked(kv,n_swa,kv_get(kv,4,0),lc_pos_get(c,i),lc_seq_pos_max(c,seq))!=0 {add=0;}}
     }
     if add!=0 {count+=1;if begin<0 {begin=i;}} else {if begin>=0 {kv_vec_push(ranges,begin);kv_vec_push(ranges,i);begin= -1;}}
     i+=1;
   }
   if begin>=0 {kv_vec_push(ranges,begin);kv_vec_push(ranges,n);}
   state_w_u32(io,count);
   if count!=0 {
     let nr=kv_get(ranges,0,0);
     var r:i64=0;
     while r<nr {
       let from=kv_get(ranges,10+r,0);
       let to=kv_get(ranges,11+r,0);
       var j:i64=from;
       while j<to {
         state_w_i32(io,lc_pos_get(c,j));
         var nseq:i64=0;
         var q:i64=0;
         while q<nsmax {if (seq==-1||q==seq)&&lc_seq_has(c,j,q)!=0 {nseq+=1;}q+=1;}
         state_w_u32(io,nseq);
         if ext!=0 {state_w_i32(io,lc_ext_x(c,j));state_w_i32(io,lc_ext_y(c,j));state_w_i32(io,lc_ext_tok(c,j));}
         q=0;
         while q<nsmax {if (seq==-1||q==seq)&&lc_seq_has(c,j,q)!=0 {state_w_i32(io,q);}q+=1;}
         j+=1;
       }
       r+=2;
     }
     state_write_data(kv,io,s,nl,ranges);
   }
   kv_destroy_raw(ranges);
   s+=1;
 }
}
fn state_read_meta_whole(kv:i64,io:i64,strm:i64,count:i64,sinfo:i64)->i64 {
 let c=kv_cells(kv,strm);
 let ext=fg_state_has_cell_ext(kv);
 let nsmax=kv_get(kv,1,0);
 if count>lc_n(c) {return 0;}
 var i:i64=0;
 while i<count {
   let p=state_r_i32(io);
   let nseq=state_r_u32(io);
   lc_pos_set(c,i,p);
   if ext!=0 {let x=state_r_i32(io);let y=state_r_i32(io);let tk=state_r_i32(io);lc_ext_set(c,i,x,y,tk);}
   var j:i64=0;
   while j<nseq {let sq=state_r_i32(io);if sq<0||sq>=nsmax {return -1;}lc_seq_add(c,i,sq);j+=1;}
   i+=1;
 }
 kv_put(sinfo,0,0,strm);kv_put(sinfo,1,0,strm);kv_put(sinfo,3,0,1);kv_put(sinfo,10,0,strm);kv_put(sinfo,2,0,count);
 i=0;
 while i<count {kv_slot_push(sinfo,0,i);i+=1;}
 kv_set_head(kv,strm,0);
 return 1;
}
fn state_read_meta_seq(kv:i64,io:i64,seq:i64,strm:i64,count:i64,sinfo:i64,has_in:i64,ins:i64,slot_in:i64)->i64 {
 let c=kv_cells(kv,strm);
 let ext=fg_state_has_cell_ext(kv);
 if count>lc_n(c) {return 0;}
 lc_seq_rm_cell_range(kv,seq,-1,-1);
 let ub=fg_kv_test_ubatch(count);
 fg_kv_test_ub_set_seq_unq(ub,0,seq);
 let exts=kv_obj(4);
 var i:i64=0;
 while i<count {
   let p=state_r_i32(io);
   let nseq=state_r_u32(io);
   if nseq!=1 {fg_kv_test_ubatch_free(ub);kv_destroy_raw(exts);return -1;}
   if ext!=0 {let x=state_r_i32(io);let y=state_r_i32(io);let tk=state_r_i32(io);kv_vec_push(exts,x);kv_vec_push(exts,y);kv_vec_push(exts,tk);fg_kv_test_ub_set_token(ub,i,tk);}
   let ignored=state_r_i32(io);
   fg_kv_test_ub_set_pos(ub,i,p);
   fg_kv_test_ub_set_seq(ub,i,0,seq);
   i+=1;
 }
 var slot:i64=0;
 if has_in!=0 {
   if fg_sinfos_in_empty(ins,slot_in)!=0 {fg_kv_test_ubatch_free(ub);kv_destroy_raw(exts);return 0;}
   slot=kv_obj(3);
   fg_sinfos_in_to_slot(ins,slot_in,slot);
   if slot_size(slot)!=count {fg_kv_test_ubatch_free(ub);kv_destroy_raw(exts);kv_destroy_raw(slot);return 0;}
   kv_put(slot,0,0,strm);kv_put(slot,1,0,strm);kv_put(slot,3,0,1);kv_put(slot,10,0,strm);
   var t:i64=0;
   while t<count {let idx=slot_idx(slot,0,t);if idx>=lc_n(c)||lc_is_empty(c,idx)==0 {fg_kv_test_ubatch_free(ub);kv_destroy_raw(exts);kv_destroy_raw(slot);return 0;}t+=1;}
 } else {
   slot=kv_find_slot(kv,ub,0);
   if slot==0 {fg_kv_test_ubatch_free(ub);kv_destroy_raw(exts);return 0;}
 }
 kv_apply_ubatch(kv,slot,ub);
 if ext!=0 {
   i=0;
   while i<count {let idx=slot_idx(slot,0,i);lc_ext_set(c,idx,kv_get(exts,10+3*i,0),kv_get(exts,11+3*i,0),kv_get(exts,12+3*i,0));i+=1;}
 }
 fg_kv_test_ubatch_free(ub);
 kv_destroy_raw(exts);
 return 1;
}
fn state_read_data(kv:i64,io:i64,strm:i64,count:i64,sinfo:i64)->i64 {
 let c=kv_cells(kv,strm);
 let vtrans=state_r_u32(io);
 let nlayer=state_r_u32(io);
 let nrotk=state_r_u32(io);
 let nrotv=state_r_u32(io);
 if nlayer!=kv_get(kv,7,0) {return 0;}
 if count>lc_n(c) {return 0;}
 if vtrans!=fg_state_v_trans(kv) {return 0;}
 if nrotk!=fg_state_n_rot(kv,0) {return 0;}
 if nrotv!=fg_state_n_rot(kv,1) {return 0;}
 let runs=kv_obj(4);
 if count>0 {
   var i0:i64=0;
   while i0<count {
     let first=slot_idx(sinfo,0,i0);
     var i1:i64=i0+1;
     while i1<count&&slot_idx(sinfo,0,i1)==slot_idx(sinfo,0,i1-1)+1 {i1+=1;}
     kv_vec_push(runs,first);
     kv_vec_push(runs,slot_idx(sinfo,0,i1-1)+1);
     i0=i1;
   }
 }
 let nr=kv_get(runs,0,0);
 var lay:i64=0;
 while lay<nlayer {
   let kt=state_r_i32(io);
   if kt!=fg_state_tensor_type(kv,strm,lay,0) {kv_destroy_raw(runs);return 0;}
   let kr=state_r_u64(io);
   if kr!=fg_state_tensor_row_size(kv,strm,lay,0) {kv_destroy_raw(runs);return 0;}
   var r:i64=0;
   while r<nr {fg_state_read_rows(kv,io,strm,lay,0,kv_get(runs,10+r,0),kv_get(runs,11+r,0));r+=2;}
   lay+=1;
 }
 if vtrans==0 {
   lay=0;
   while lay<nlayer {
     if fg_state_layer_has_v(kv,lay)!=0 {
       let vt=state_r_i32(io);
       if vt!=fg_state_tensor_type(kv,strm,lay,1) {kv_destroy_raw(runs);return 0;}
       let vr=state_r_u64(io);
       if vr!=fg_state_tensor_row_size(kv,strm,lay,1) {kv_destroy_raw(runs);return 0;}
       var r:i64=0;
       while r<nr {fg_state_read_rows(kv,io,strm,lay,1,kv_get(runs,10+r,0),kv_get(runs,11+r,0));r+=2;}
     }
     lay+=1;
   }
 } else {
   lay=0;
   while lay<nlayer {
     if fg_state_layer_has_v(kv,lay)!=0 {
       let vt=state_r_i32(io);
       if vt!=fg_state_tensor_type(kv,strm,lay,1) {kv_destroy_raw(runs);return 0;}
       let el=state_r_u32(io);
       if el!=fg_state_tensor_size_el(kv,strm,lay,1) {kv_destroy_raw(runs);return 0;}
       let nv=state_r_u32(io);
       let il=fg_state_layer_il(kv,lay);
       if nv!=fg_state_n_embd_v(kv,il) {kv_destroy_raw(runs);return 0;}
       var j:i64=0;
       while j<nv {
         var r:i64=0;
         while r<nr {fg_state_read_trans(kv,io,strm,lay,j,kv_get(runs,10+r,0),kv_get(runs,11+r,0));r+=2;}
         j+=1;
       }
     }
     lay+=1;
   }
 }
 kv_destroy_raw(runs);
 return 1;
}
fn state_clear_local(kv:i64,seq:i64,strm:i64,sinfo:i64) {
 if seq==-1 {
   let n=kv_n_stream(kv);
   var s:i64=0;
   while s<n {lc_reset(kv_cells(kv,s));kv_set_head(kv,s,0);s+=1;}
   fg_kv_clear_data(kv);
 } else {
   lc_seq_rm_cell_range(kv,seq,-1,-1);
   if slot_size(sinfo)>0 {fg_kv_zero_cells(kv,strm,sinfo,0);}
 }
}
fn kv_state_read_sinfo(kv:i64,io:i64,seq:i64,flags:i64,out:i64,has_out:i64,ins:i64,has_in:i64) {
 if kv_get(kv,62,0)!=0 {return;}
 if (flags&1)!=0&&kv_get(kv,4,0)==0 {return;}
 ensure(seq==-1||kv_seq_ok(kv,seq)!=0);
 let nstream=kv_n_stream(kv);
 if has_out!=0 {fg_sinfos_init(out,nstream);}
 if has_in!=0 {if fg_sinfos_count(ins)!=nstream {fg_state_fail(2);}}
 let ncur=state_r_u32(io);
 if ncur!=nstream {fg_state_fail(1);}
 if seq==-1 {
   var t:i64=0;
   while t<nstream {lc_reset(kv_cells(kv,t));kv_set_head(kv,t,0);t+=1;}
   fg_kv_clear_data(kv);
 }
 var s:i64=0;
 while s<nstream {
   let count=state_r_u32(io);
   if count==0 {
     if has_in!=0 {if fg_sinfos_in_empty(ins,s)==0 {fg_state_fail(3);}}
     s+=1;
   } else {
     var strm:i64=s;
     if seq!=-1 {strm=kv_stream(kv,seq);}
     let sinfo=kv_obj(3);
     var ok:i64=1;
     if seq==-1 {ok=state_read_meta_whole(kv,io,strm,count,sinfo);}
     else {ok=state_read_meta_seq(kv,io,seq,strm,count,sinfo,has_in,ins,s);}
     if ok>0 {ok=state_read_data(kv,io,strm,count,sinfo);}
     if ok<=0 {state_clear_local(kv,seq,strm,sinfo);fg_state_fail(4);}
     if has_out!=0 {fg_sinfos_set_out(out,s,sinfo);}
     kv_destroy_raw(sinfo);
     s+=1;
   }
 }
}

fn selftest()->i64 {
 let kv=kv_create(1,4);
 let u=fg_kv_test_ubatch(1);
 fg_kv_test_ub_set_seq(u,0,0,0);
 fg_kv_test_ub_set_seq_unq(u,0,0);
 fg_kv_test_ub_set_pos(u,0,0);
 let slot=kv_find_slot(kv,u,0);
 ensure(slot!=0);
 ensure(slot_size(slot)==1);
 ensure(slot_idx(slot,0,0)==0);
 kv_destroy_raw(slot);
 let ub=kv_obj(4);
 kv_vec_push(ub,u);
 let sf=kv_prepare(kv,ub);
 ensure(sf!=0);
 ensure(lc_is_empty(kv_cells(kv,0),0)==1);
 let ctx=kv_ctx_batch(kv,sf,ub);
 kv_destroy_raw(sf);
 kv_destroy_raw(ub);
 ensure(ctx_get_status(ctx)==0);
 ensure(ctx_apply(ctx)==1);
 ensure(lc_is_empty(kv_cells(kv,0),0)==0);
 ensure(lc_pos_get(kv_cells(kv,0),0)==0);
 ensure(lc_seq_has(kv_cells(kv,0),0,0)==1);
 let kv2=kv_create(1,2);
 let full=kv_init_full(kv2);
 ensure(ctx_get_n_kv(full)==2);
 let upd=kv_init_update(kv2,0);
 ensure(ctx_get_status(upd)==1);
 let u2=fg_kv_test_ubatch(1);
 fg_kv_test_ub_set_seq(u2,0,0,0);
 fg_kv_test_ub_set_seq_unq(u2,0,0);
 fg_kv_test_ub_set_pos(u2,0,0);
 let b=fg_kv_test_balloc(u2);
 let ib=kv_init_batch(kv2,b,1,0);
 ensure(ctx_get_status(ib)==0);
 kv_ctx_destroy(ib);
 fg_kv_balloc_destroy(b);
 kv_ctx_destroy(upd);
 kv_ctx_destroy(full);
 kv_ctx_destroy(ctx);
 kv_destroy(kv2);
 kv_destroy(kv);
 return 1;
}

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
 kv_service(100,mode,0);
 if mode==0 {return kv_create(a,b);}
 if mode==1 {kv_destroy(a);return 0;}
 if mode==2 {return kv_find_slot(a,b,c);}
 if mode==3 {return kv_prepare(a,b);}
 if mode==4 {kv_apply_ubatch(a,b,c);return 0;}
 if mode==5 {return kv_init_batch(a,b,c,d);}
 if mode==6 {return kv_init_full(a);}
 if mode==7 {return kv_init_update(a,b);}
 if mode==8 {return ctx_next(a);}
 if mode==9 {return ctx_apply(a);}
 if mode==10 {return ctx_get_status(a);}
 if mode==11 {return ctx_get_ubatch(a);}
 if mode==12 {return ctx_get_n_kv(a);}
 if mode==13 {return ctx_type_k(a);}
 if mode==14 {return ctx_type_v(a);}
 if mode==15 {kv_ctx_destroy(a);return 0;}
 if mode==16 {return kv_update(a,b,c,d,e);}
 if mode==17 {return kv_n_kv(a,b);}
 if mode==18 {return selftest();}
 if mode==19 {return kv_ctx_batch(a,b,c);}
 if mode==20 {return kv_ctx_error(a);}
 if mode==21 {return kv_update(a,b,c,d,e);}
 if mode==22 {kv_state_write(a,b,c,d);return 0;}
 if mode==23 {var ho:i64=0;if e!=0 {ho=1;}var hi:i64=0;if f!=0 {hi=1;}kv_state_read_sinfo(a,b,c,d,e,ho,f,hi);return 0;}
 ensure(false);return 0;
}
fn main(){print(entry(18,0,0,0,0,0,0));}
