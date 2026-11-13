extern "q4a_n_layer" fn n_layer(f:i64)->i64;
extern "q4a_use_layer" fn use_layer(f:i64,il:i64)->i64;
extern "q4a_n_embd_k" fn n_embd_k(f:i64,il:i64)->i64;
extern "q4a_n_embd_v" fn n_embd_v(f:i64,il:i64)->i64;
extern "q4a_type" fn type_of(f:i64,is_v:i64)->i64;
extern "q4a_kv_size" fn kv_size(f:i64)->i64;
extern "q4a_nstream" fn nstream(f:i64)->i64;
extern "q4a_tag" fn tag_of(f:i64)->i64;
extern "q4a_offload" fn offload(f:i64)->i64;
extern "q4a_vtrans" fn vtrans(f:i64)->i64;
extern "q4a_cpu_buft" fn cpu_buft()->i64;
extern "q4a_dev_buft" fn dev_buft(f:i64,il:i64)->i64;
extern "q4a_lazy_buft" fn lazy_buft(f:i64,il:i64)->i64;
extern "q4a_ctx" fn ctx_for(f:i64,buft:i64,il:i64)->i64;
extern "q4a_tensor3d" fn tensor3d(ctx:i64,kind:i64,ne0:i64,ne1:i64,ne2:i64,tag:i64,il:i64,is_v:i64)->i64;
extern "q4a_layer_add" fn layer_add(kv:i64,il:i64,k:i64,v:i64,ctx:i64,nk:i64,nv:i64,ns:i64)->i64;
extern "q4a_set_head_all" fn set_head_all(kv:i64,k_all:i64,v_all:i64);
extern "q4a_set_lazy" fn set_lazy(kv:i64,v:i64);
extern "q4a_finalize" fn finalize(kv:i64,f:i64)->i64;
extern "q4a_release_impl" fn release_impl(kv:i64);
extern "q4a_witness" fn witness(mode:i64);
extern "q4a_rot_disabled" fn rot_disabled(f:i64)->i64;
extern "q4a_quantized" fn quantized(f:i64,is_v:i64)->i64;
extern "q4a_head_k" fn head_k(f:i64)->i64;
extern "q4a_head_v" fn head_v(f:i64)->i64;
extern "q4a_set_rot" fn set_rot(kv:i64,nk:i64,nv:i64);
extern "q4a_h_begin" fn h_begin(kv:i64,n:i64);
extern "q4a_h_end" fn h_end(kv:i64,n:i64);
extern "q4a_h_get" fn h_get(kv:i64,n:i64,i:i64,j:i64)->i64;
extern "q4a_h_set" fn h_set(kv:i64,n:i64,i:i64,j:i64,v:i64);
extern "q4a_inv_sqrt" fn inv_sqrt(n:i64)->i64;

fn ensure(ok:bool) {if !ok {witness(99);assert(false);}}
fn build_hadamard(kv:i64,n:i64) {
 h_begin(kv,n);
 h_set(kv,n,0,0,inv_sqrt(n));
 var s:i64=1;
 while s<n {
   var i:i64=0;
   while i<s {
     var j:i64=0;
     while j<s {
       let val=h_get(kv,n,i,j);
       h_set(kv,n,i+s,j,val);
       h_set(kv,n,i,j+s,val);
       h_set(kv,n,i+s,j+s,0-val);
       j+=1;
     }
     i+=1;
   }
   s=s*2;
 }
 h_end(kv,n);
}
fn q4a_init(kv:i64,f:i64)->i64 {
 let n=n_layer(f);
 var il:i64=0;
 var have_k:i64=0;
 var have_v:i64=0;
 var k_all:i64=0;
 var v_all:i64=0;
 var lazy:i64=0;
 while il<n {
   if use_layer(f,il)!=0 {
     let nk=n_embd_k(f,il);
     let nv=n_embd_v(f,il);
     if have_k==0 {k_all=nk;have_k=1;} else {if k_all!=nk {k_all= -1;}}
     if have_v==0 {v_all=nv;have_v=1;} else {if v_all!=nv {v_all= -1;}}
     var buft:i64=cpu_buft();
     if offload(f)!=0 {
       buft=dev_buft(f,il);
       if vtrans(f)==0 {
         let lb=lazy_buft(f,il);
         if lb!=0 {buft=lb;lazy=1;}
       }
     }
     let ctx=ctx_for(f,buft,il);
     if ctx==0 {return 0;}
     let k=tensor3d(ctx,type_of(f,0),nk,kv_size(f),nstream(f),tag_of(f),il,0);
     if k==0 {return 0;}
     let v=tensor3d(ctx,type_of(f,1),nv,kv_size(f),nstream(f),tag_of(f),il,1);
     if v==0 {return 0;}
     if layer_add(kv,il,k,v,ctx,nk,nv,nstream(f))==0 {return 0;}
   }
   il+=1;
 }
 set_lazy(kv,lazy);
 if finalize(kv,f)!=0 {return 0;}
 let dis=rot_disabled(f);
 let hk=head_k(f);
 let hv=head_v(f);
 var rk:i64=0;
 if dis==0&&k_all>0&&quantized(f,0)!=0&&hk%64==0 {rk=1;}
 var rv:i64=0;
 if dis==0&&v_all>0&&quantized(f,1)!=0&&hv%64==0 {rv=1;}
 var nk:i64=0;
 var nv:i64=0;
 if rk!=0 {
   nk=64;
   while k_all%(2*nk)==0 {nk=nk*2;}
 }
 if rv!=0 {nv=64;}
 set_rot(kv,nk,nv);
 var mx:i64=k_all;
 if v_all>mx {mx=v_all;}
 var m:i64=64;
 while m<=mx {build_hadamard(kv,m);m=m*2;}
 set_head_all(kv,k_all,v_all);
 return 1;
}
fn q4a_release(kv:i64) {if kv!=0 {release_impl(kv);}}
fn entry(mode:i64,a:i64,b:i64,c:i64)->i64 {
 witness(mode);
 if mode==0 {return q4a_init(a,b);}
 if mode==1 {q4a_release(a);return 0;}
 ensure(false);return 0;
}
fn main(){print(entry(1,0,0,0));}
