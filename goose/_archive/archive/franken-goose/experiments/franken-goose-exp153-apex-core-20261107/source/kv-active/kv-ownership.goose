extern "own_n_layer" fn own_n_layer(frame:i64)->i64;
extern "own_use_layer" fn own_use_layer(frame:i64,il:i64)->i64;
extern "own_n_embd_k" fn own_n_embd_k(frame:i64,il:i64)->i64;
extern "own_n_embd_v" fn own_n_embd_v(frame:i64,il:i64)->i64;
extern "own_type" fn own_type(frame:i64,is_v:i64)->i64;
extern "own_kv_size" fn own_kv_size(frame:i64)->i64;
extern "own_nstream" fn own_nstream(frame:i64)->i64;
extern "own_tag" fn own_tag(frame:i64)->i64;
extern "own_ctx" fn own_ctx(frame:i64,il:i64)->i64;
extern "own_tensor3d" fn own_tensor3d(ctx:i64,kind:i64,ne0:i64,ne1:i64,ne2:i64,tag:i64,il:i64,is_v:i64)->i64;
extern "own_layer_add" fn own_layer_add(kv:i64,il:i64,k:i64,v:i64,ctx:i64,nk:i64,nv:i64,nstream:i64)->i64;
extern "own_set_head_all" fn own_set_head_all(kv:i64,k_all:i64,v_all:i64);
extern "own_finalize" fn own_finalize(kv:i64,frame:i64)->i64;
extern "own_release_impl" fn own_release_impl(kv:i64);
extern "own_witness" fn own_witness(mode:i64);

fn ensure(ok:bool) {if !ok {own_witness(99);assert(false);}}
fn own_init(kv:i64,frame:i64)->i64 {
 let n=own_n_layer(frame);
 var il:i64=0;
 var have_k:i64=0;
 var have_v:i64=0;
 var k_all:i64=0;
 var v_all:i64=0;
 while il<n {
   if own_use_layer(frame,il)!=0 {
     let nk=own_n_embd_k(frame,il);
     let nv=own_n_embd_v(frame,il);
     if have_k==0 {k_all=nk;have_k=1;} else {if k_all!=nk {k_all= -1;}}
     if have_v==0 {v_all=nv;have_v=1;} else {if v_all!=nv {v_all= -1;}}
     let ctx=own_ctx(frame,il);
     if ctx==0 {return 0;}
     let k=own_tensor3d(ctx,own_type(frame,0),nk,own_kv_size(frame),own_nstream(frame),own_tag(frame),il,0);
     if k==0 {return 0;}
     let v=own_tensor3d(ctx,own_type(frame,1),nv,own_kv_size(frame),own_nstream(frame),own_tag(frame),il,1);
     if v==0 {return 0;}
     if own_layer_add(kv,il,k,v,ctx,nk,nv,own_nstream(frame))==0 {return 0;}
   }
   il+=1;
 }
 own_set_head_all(kv,k_all,v_all);
 if own_finalize(kv,frame)!=0 {return 0;}
 return 1;
}
fn own_release(kv:i64) {if kv!=0 {own_release_impl(kv);}}
fn entry(mode:i64,a:i64,b:i64,c:i64)->i64 {
 own_witness(mode);
 if mode==0 {return own_init(a,b);}
 if mode==1 {own_release(a);return 0;}
 ensure(false);return 0;
}
fn main(){print(entry(1,0,0,0));}
