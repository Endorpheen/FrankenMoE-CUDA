extern "fg153_gr_const" fn gr_const(code:i64)->i64;
extern "fg153_gr_tensor_get" fn gr_tensor_get(r:i64,slot:i64)->i64;
extern "fg153_gr_tensor_set" fn gr_tensor_set(r:i64,slot:i64,t:i64);
extern "fg153_gr_layer_inp_len" fn gr_layer_inp_len(r:i64)->i64;
extern "fg153_gr_layer_inp_reset" fn gr_layer_inp_reset(r:i64,n:i64);
extern "fg153_gr_layer_inp_get" fn gr_layer_inp_get(r:i64,i:i64)->i64;
extern "fg153_gr_sampling_len" fn gr_sampling_len(r:i64,kind:i64)->i64;
extern "fg153_gr_sampling_get" fn gr_sampling_get(r:i64,kind:i64,i:i64)->i64;
extern "fg153_gr_sampling_clear" fn gr_sampling_clear(r:i64,kind:i64);
extern "fg153_gr_inputs_len" fn gr_inputs_len(r:i64)->i64;
extern "fg153_gr_input_at" fn gr_input_at(r:i64,i:i64)->i64;
extern "fg153_gr_inputs_add" fn gr_inputs_add(r:i64,raw:i64)->i64;
extern "fg153_gr_inputs_clear" fn gr_inputs_clear(r:i64);
extern "fg153_gi_set_input" fn gi_set_input(inp:i64,u:i64)->i64;
extern "fg153_gi_can_reuse" fn gi_can_reuse(inp:i64,p:i64)->i64;
extern "fg153_gr_fused_push" fn gr_fused_push(r:i64,op:i64,t:i64,il:i64);
extern "fg153_gr_fused_clear" fn gr_fused_clear(r:i64);
extern "fg153_gr_hdmd_get" fn gr_hdmd_get(r:i64,cur:i64,rot:i64)->i64;
extern "fg153_gr_hdmd_set" fn gr_hdmd_set(r:i64,cur:i64,rot:i64,res:i64);
extern "fg153_gr_hdmd_clear" fn gr_hdmd_clear(r:i64);
extern "fg153_gr_params_clear" fn gr_params_clear(params_ptr:i64);
extern "fg153_gr_params_copy" fn gr_params_copy(dst:i64,src:i64);
extern "fg153_gr_meta_resize" fn gr_meta_resize(r:i64,bytes:i64);
extern "fg153_gr_ctx_init" fn gr_ctx_init(r:i64)->i64;
extern "fg153_gr_new_graph" fn gr_new_graph(ctx:i64,max_nodes:i64)->i64;
extern "fg153_gr_gf_set" fn gr_gf_set(r:i64,gf:i64);
extern "fg153_gr_tensor_overhead" fn gr_tensor_overhead()->i64;
extern "fg153_gr_graph_overhead" fn gr_graph_overhead(max_nodes:i64)->i64;
extern "fg153_gp_get" fn gp_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_gp_sampler_key" fn gp_sampler_key(p:i64,i:i64)->i64;
extern "fg153_gp_sampler_val" fn gp_sampler_val(p:i64,i:i64)->i64;
extern "fg153_gp_layer_inp_size" fn gp_layer_inp_size(p:i64)->i64;
extern "fg153_gp_layer_inp_at" fn gp_layer_inp_at(p:i64,i:i64)->i64;
extern "fg153_gp_ubatch" fn gp_ubatch(p:i64)->i64;
extern "fg153_gu_get" fn gu_get(u:i64,k:i64,i:i64)->i64;
extern "fg153_gu_seq_id" fn gu_seq_id(u:i64,i:i64,s:i64)->i64;
extern "fg153_gx_set_output" fn gx_set_output(t:i64);
extern "fg153_gx_assert" fn gx_assert(ok:i64);
extern "fg153_gx_log" fn gx_log(site:i64,a:i64,b:i64);

fn ensure(ok:bool) { if !ok { gx_assert(0); } }

fn allow_reuse(old:i64,new:i64)->bool {
 let uo=gp_ubatch(old); let un=gp_ubatch(new);
 if gu_get(uo,5,0)!=gu_get(un,5,0) {return false;}
 if gu_get(uo,0,0)!=gu_get(un,0,0) {return false;}
 if gu_get(uo,1,0)!=gu_get(un,1,0) {return false;}
 if gu_get(uo,2,0)!=gu_get(un,2,0) {return false;}
 if gu_get(uo,3,0)!=gu_get(un,3,0) {return false;}
 if gu_get(uo,6,0)!=gu_get(un,6,0) {return false;}
 let ot=gu_get(uo,7,0); let nt=gu_get(un,7,0);
 let oe=gu_get(uo,8,0); let ne=gu_get(un,8,0);
 let both_empty=(ot==0&&nt==0)||(oe==0&&ne==0);
 let both_full=ot!=0&&nt!=0&&oe!=0&&ne!=0;
 if !both_empty&&!both_full {return false;}
 if (gu_get(uo,9,0)==0)!=(gu_get(un,9,0)==0) {return false;}
 if gu_get(uo,5,0)!=0 {
  if gu_get(uo,12,0)==0 {return false;}
  var s=0; while s<gu_get(uo,3,0) { if gu_get(uo,18,s)!=gu_get(un,18,s) {return false;} s+=1; }
 }
 if gp_get(old,1,0)!=gp_get(new,1,0) {return false;}
 if !samplers_equal(old,new) {return false;}
 if gp_get(old,7,0)>0 {
  if gu_get(uo,12,0)==0||gu_get(un,12,0)==0 {return false;}
  var i=0; while i<gu_get(uo,0,0) {
   if gu_get(uo,16,i)!=gu_get(un,16,i) {return false;}
   if gu_seq_id(uo,i,0)!=gu_seq_id(un,i,0) {return false;}
   i+=1;
  }
 }
 if gp_get(old,12,0)!=gp_get(new,12,0) {return false;}
 if gp_get(old,8,0)!=gp_get(new,8,0) {return false;}
 if gp_get(old,9,0)!=gp_get(new,9,0) {return false;}
 if gp_get(old,10,0)!=gp_get(new,10,0) {return false;}
 if gp_get(old,11,0)!=gp_get(new,11,0) {return false;}
 if gp_get(old,0,0)!=gp_get(new,0,0) {return false;}
 if gp_get(old,2,0)!=gp_get(new,2,0) {return false;}
 if gp_get(old,3,0)!=gp_get(new,3,0) {return false;}
 if gp_get(old,4,0)!=gp_get(new,4,0) {return false;}
 if gp_get(old,5,0)!=gp_get(new,5,0) {return false;}
 return true;
}

fn samplers_equal(old:i64,new:i64)->bool {
 let n=gp_get(old,7,0); if n!=gp_get(new,7,0) {return false;}
 var i=0; while i<n {
  let k=gp_sampler_key(old,i); let v=gp_sampler_val(old,i);
  var j=0; var found=0;
  while j<n {
   if gp_sampler_key(new,j)==k {
    if gp_sampler_val(new,j)!=v {return false;}
    found=1; break;
   }
   j+=1;
  }
  if found==0 {return false;}
  i+=1;
 }
 return true;
}

fn res_reset(r:i64,max_nodes:i64,params_ptr:i64)->i64 {
 gr_tensor_set(r,0,0); gr_tensor_set(r,1,0); gr_tensor_set(r,2,0);
 gr_tensor_set(r,3,0); gr_tensor_set(r,4,0); gr_tensor_set(r,5,0);
 gr_layer_inp_reset(r,gr_const(0)+1);
 gr_sampling_clear(r,0); gr_sampling_clear(r,1);
 gr_sampling_clear(r,2); gr_sampling_clear(r,3);
 gr_params_clear(params_ptr);
 gr_inputs_clear(r);
 gr_fused_clear(r);
 gr_hdmd_clear(r);
 let bytes=gr_tensor_overhead()*max_nodes+gr_graph_overhead(max_nodes);
 gr_meta_resize(r,bytes);
 let ctx=gr_ctx_init(r); ensure(ctx!=0);
 let gf=gr_new_graph(ctx,max_nodes); ensure(gf!=0);
 gr_gf_set(r,gf);
 return 0;
}

fn res_set_inputs(r:i64,u:i64)->i64 {
 let n=gr_inputs_len(r); var i=0;
 while i<n { let inp=gr_input_at(r,i); let ignored=gi_set_input(inp,u); i+=1; }
 return 0;
}

fn res_set_outputs(r:i64,p:i64)->i64 {
 if gr_tensor_get(r,2)!=0 {gx_set_output(gr_tensor_get(r,2));}
 if gr_tensor_get(r,3)!=0 {gx_set_output(gr_tensor_get(r,3));}
 if gr_tensor_get(r,4)!=0 {gx_set_output(gr_tensor_get(r,4));}
 if gr_tensor_get(r,5)!=0 {gx_set_output(gr_tensor_get(r,5));}
 let nl=gp_layer_inp_size(p); var il=0;
 while il<nl {
  if gp_layer_inp_at(p,il)!=0 {
   let t=gr_layer_inp_get(r,il); ensure(t!=0); gx_set_output(t);
  }
  il+=1;
 }
 var kind=0; while kind<4 {
  let n=gr_sampling_len(r,kind); var i=0;
  while i<n { let t=gr_sampling_get(r,kind,i); if t!=0 {gx_set_output(t);} i+=1; }
  kind+=1;
 }
 return 0;
}

fn res_can_reuse(r:i64,new:i64,old:i64,debug:i64)->i64 {
 if !allow_reuse(old,new) {
  if debug>1 {gx_log(0,0,0);}
  return 0;
 }
 if debug>1 {gx_log(1,gr_inputs_len(r),0);}
 let n=gr_inputs_len(r); var i=0; var ok=1;
 while i<n {
  let cur=gi_can_reuse(gr_input_at(r,i),new);
  if debug>1 {gx_log(2,cur,0);}
  if cur==0 {ok=0;}
  i+=1;
 }
 if debug>0 {gx_log(3,ok,0);}
 return ok;
}

fn res_entry(op:i64,r:i64,a:i64,b:i64,c:i64,d:i64)->i64 {
 if op==99 {return 0;}
 if op==0 {return res_reset(r,a,b);}
 if op==1 {return res_set_inputs(r,a);}
 if op==2 {return res_set_outputs(r,a);}
 if op==3 {return res_can_reuse(r,a,b,c);}
 if op==4 {return gr_inputs_add(r,a);}
 if op==5 {gr_fused_push(r,a,b,c);return 0;}
 if op==6 {return gr_hdmd_get(r,a,b);}
 if op==7 {gr_hdmd_set(r,a,b,c);return 0;}
 if op==8 {gr_params_copy(b,a);return 0;}
 if op==9 {return gr_layer_inp_len(r);}
 ensure(false);
 return 0;
}

fn main() { print(res_entry(99,0,0,0,0,0)); }
