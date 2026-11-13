extern "q4_n_layer" fn q4_n_layer(frame:i64)->i64;
extern "q4_use_layer" fn q4_use_layer(frame:i64,il:i64)->i64;
extern "q4_n_embd_k" fn q4_n_embd_k(frame:i64,il:i64)->i64;
extern "q4_n_embd_v" fn q4_n_embd_v(frame:i64,il:i64)->i64;
extern "q4_type" fn q4_type(frame:i64,is_v:i64)->i64;
extern "q4_kv_size" fn q4_kv_size(frame:i64)->i64;
extern "q4_nstream" fn q4_nstream(frame:i64)->i64;
extern "q4_tag" fn q4_tag(frame:i64)->i64;
extern "q4_ctx" fn q4_ctx(frame:i64,il:i64)->i64;
extern "q4_tensor3d" fn q4_tensor3d(ctx:i64,kind:i64,ne0:i64,ne1:i64,ne2:i64,tag:i64,il:i64,is_v:i64)->i64;
extern "q4_layer_add" fn q4_layer_add(kv:i64,il:i64,k:i64,v:i64,ctx:i64,nk:i64,nv:i64,nstream:i64)->i64;
extern "q4_set_head_all" fn q4_set_head_all(kv:i64,k_all:i64,v_all:i64);
extern "q4_finalize" fn q4_finalize(kv:i64,frame:i64)->i64;
extern "q4_release_impl" fn q4_release_impl(kv:i64);
extern "q4_witness" fn q4_witness(mode:i64);
extern "q4_other" fn q4_other(frame:i64)->i64;
extern "q4_rot_disabled" fn q4_rot_disabled(frame:i64)->i64;
extern "q4_quantized" fn q4_quantized(frame:i64,is_v:i64)->i64;
extern "q4_head_k" fn q4_head_k(frame:i64)->i64;
extern "q4_head_v" fn q4_head_v(frame:i64)->i64;
extern "q4_copy_rot" fn q4_copy_rot(kv:i64,frame:i64);
extern "q4_set_rot" fn q4_set_rot(kv:i64,nk:i64,nv:i64);
extern "q4_hadamard" fn q4_hadamard(kv:i64,n:i64);
extern "q4_clear_hadamard" fn q4_clear_hadamard(kv:i64);

fn ensure(ok:bool) {if !ok {q4_witness(99);assert(false);}}
fn q4_init(kv:i64,frame:i64)->i64 {
 let n=q4_n_layer(frame);
 var il:i64=0;
 var have_k:i64=0;
 var have_v:i64=0;
 var k_all:i64=0;
 var v_all:i64=0;
 while il<n {
   if q4_use_layer(frame,il)!=0 {
     let nk=q4_n_embd_k(frame,il);
     let nv=q4_n_embd_v(frame,il);
     if have_k==0 {k_all=nk;have_k=1;} else {if k_all!=nk {k_all= -1;}}
     if have_v==0 {v_all=nv;have_v=1;} else {if v_all!=nv {v_all= -1;}}
     let ctx=q4_ctx(frame,il);
     if ctx==0 {return 0;}
     let k=q4_tensor3d(ctx,q4_type(frame,0),nk,q4_kv_size(frame),q4_nstream(frame),q4_tag(frame),il,0);
     if k==0 {return 0;}
     let v=q4_tensor3d(ctx,q4_type(frame,1),nv,q4_kv_size(frame),q4_nstream(frame),q4_tag(frame),il,1);
     if v==0 {return 0;}
     if q4_layer_add(kv,il,k,v,ctx,nk,nv,q4_nstream(frame))==0 {return 0;}
   }
   il+=1;
 }
 if q4_finalize(kv,frame)!=0 {return 0;}
 if q4_other(frame)!=0 {
   q4_copy_rot(kv,frame);
 } else {
   let dis=q4_rot_disabled(frame);
   let hk=q4_head_k(frame);
   let hv=q4_head_v(frame);
   var rk:i64=0;
   if dis==0&&k_all>0&&q4_quantized(frame,0)!=0&&hk%64==0 {rk=1;}
   var rv:i64=0;
   if dis==0&&v_all>0&&q4_quantized(frame,1)!=0&&hv%64==0 {rv=1;}
   var nk:i64=0;
   var nv:i64=0;
   if rk!=0 {
     nk=64;
     while k_all%(2*nk)==0 {nk=nk*2;}
   }
   if rv!=0 {nv=64;}
   q4_set_rot(kv,nk,nv);
   var mx:i64=k_all;
   if v_all>mx {mx=v_all;}
   var m:i64=64;
   while m<=mx {q4_hadamard(kv,m);m=m*2;}
 }
 q4_set_head_all(kv,k_all,v_all);
 return 1;
}
fn q4_release(kv:i64) {if kv!=0 {q4_clear_hadamard(kv);q4_release_impl(kv);}}
fn entry(mode:i64,a:i64,b:i64,c:i64)->i64 {
 q4_witness(mode);
 if mode==0 {return q4_init(a,b);}
 if mode==1 {q4_release(a);return 0;}
 ensure(false);return 0;
}
fn main(){print(entry(1,0,0,0));}
