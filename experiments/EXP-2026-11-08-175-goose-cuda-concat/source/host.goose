// Apex CUDA CONCAT host planning and routing (Goose), from ggml-cuda/concat.cu.
//
// Scope, exactly ggml_cuda_op_concat and concat_cuda:
//   - type agreement asserts and quantized alignment asserts,
//   - the quantized-vs-element-type width selection (uint8_t for quantized and
//     for type sizes 1/2/4/8),
//   - the contiguous-to-3 / contiguous branch choice for each dim,
//   - per-i3 slice pointer advance and grid/block sizing for concat_cont,
//   - grid/block sizing for concat_non_cont.
//
// Everything above is Goose. The parent C++ keeps only raw tensor-field reads,
// enum constant queries, the plan arena, and the parent-owned launch adapter
// that starts the Goose device entry. No original kernel, ggml struct, heap or
// original algorithm is wrapped or called.
//
// Caller-owned bounds: this module proves the shape/type/alignment contract and
// representable spans. It cannot see allocation sizes; the graph allocator owns
// them. The dim==3 contiguous and same-type-contiguous byte copies remain a raw
// cudaMemcpyAsync in the parent (they are transport, not computation) and are
// requested through `copy_bytes` so the parent performs exactly that call.
extern "fg175cn_arg" fn arg(packet:i64,k:i64)->i64;
extern "fg175cn_plan" fn plan_ptr(packet:i64)->i64;
extern "fg175cn_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg175cn_ti" fn ti(t:i64,k:i64,i:i64)->i64;
extern "fg175cn_tc" fn tc(k:i64)->i64;
extern "fg175cn_oc" fn oc(k:i64)->i64;
extern "fg175cn_tblk" fn tblk(ty:i64)->i64;
extern "fg175cn_tsz" fn tsz(ty:i64)->i64;
extern "fg175cn_cont3" fn cont3(t:i64)->i64;
extern "fg175cn_cont" fn cont(t:i64)->i64;
extern "fg175cn_nb3" fn nb3(t:i64)->i64;
extern "fg175cn_launch" fn launch(packet:i64)->i64;
extern "fg175cn_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn U()->u64{return 18446744073709551615 as u64;}
fn add(a:u64,b:u64)->u64{ensure(a<=U()-b,1);return a+b;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,2);return a*b;}
fn itg(a:i64)->u64{ensure(a>=0,3);return a as u64;}
fn cld(a:u64,b:u64)->u64{ensure(b!=0,4);return a/b+if a%b!=0{1 as u64}else{0 as u64};}
fn mni(a:i64,b:i64)->i64{return if a<b{a}else{b};}
fn nev(t:i64,i:i64)->i64{return ti(t,3,i);}
fn nbv(t:i64,i:i64)->i64{return ti(t,4,i);}

// ggml_cuda_op_concat asserts. Quantized types must be block-aligned on ne[0]
// and contiguous to 3 dims (dim != 3) or fully contiguous (dim == 3).
fn check_src(t:i64,dim:i64){
 let q:i64=if tblk(ti(t,2,0))>1{1}else{0};
 if q!=0{
  if dim==3{ensure(cont3(t)==1&&cont(t)==1,20);}
  else{ensure(cont3(t)==1,21);}
  let bs:i64=tblk(ti(t,2,0));
  ensure(nev(t,0)%bs==0,22);
 }
}
// Copy unit width: quantized -> 1 byte (the tensors are viewed as bytes);
// otherwise the type size, which the native switch requires to be 1/2/4/8.
fn width_of(ty:i64)->i64{
 let q:i64=if tblk(ty)>1{1}else{0};
 if q!=0{return 1;}
 let s:i64=tsz(ty);
 ensure(s==1||s==2||s==4||s==8,23);
 return s;
}
// concat_cont path for one i3 slice, native concat_cuda first branch.
// n = ne0*ne1*ne2 elements of width w; grid is flat over n.
fn plan_cont(p:i64,w:i64,ne00:i64,ne01:i64,ne02:i64,ne0:i64,ne1:i64,ne2:i64,dim:i64){
 ensure(ne0>0&&ne1>0&&ne2>0,30);
 let n=mul(mul(itg(ne0),itg(ne1)),itg(ne2));
 let bs:u64=64 as u64;
 let g=cld(n,bs);
 ensure(g<=9223372036854775807,31);
 ensure(g<=2147483647,32);
 put(p,0,0,w as u64);put(p,0,1,n);put(p,0,2,bs);put(p,0,3,g);
 put(p,1,0,ne00 as u64);put(p,1,1,ne01 as u64);put(p,1,2,ne02 as u64);
 put(p,1,3,ne0 as u64);put(p,1,4,ne1 as u64);put(p,1,5,ne2 as u64);
 put(p,2,0,dim as u64);
}
// concat_non_cont path: grid (dst.ne1, dst.ne2, dst.ne3), block (64,1,1),
// i0 strided by blockDim.x over dst.ne0.
fn plan_nc(p:i64,w:i64,
 ne00:i64,ne01:i64,ne02:i64,ne03:i64,nb00:i64,nb01:i64,nb02:i64,nb03:i64,
 nb10:i64,nb11:i64,nb12:i64,nb13:i64,
 ne0:i64,ne1:i64,ne2:i64,ne3:i64,nb0:i64,nb1:i64,nb2:i64,nb3:i64,dim:i64){
 ensure(ne0>0&&ne1>0&&ne2>0&&ne3>0,40);
 // native: dim3 grid_dim(dst->ne[1], dst->ne[2], dst->ne[3])
 ensure(ne1<=2147483647&&ne2<=65535&&ne3<=65535,41);
 put(p,0,0,w as u64);
 put(p,1,0,ne00 as u64);put(p,1,1,ne01 as u64);put(p,1,2,ne02 as u64);put(p,1,3,ne03 as u64);
 put(p,1,4,nb00 as u64);put(p,1,5,nb01 as u64);put(p,1,6,nb02 as u64);put(p,1,7,nb03 as u64);
 put(p,1,8,nb10 as u64);put(p,1,9,nb11 as u64);put(p,1,10,nb12 as u64);put(p,1,11,nb13 as u64);
 put(p,2,0,ne0 as u64);put(p,2,1,ne1 as u64);put(p,2,2,ne2 as u64);put(p,2,3,ne3 as u64);
 put(p,2,4,nb0 as u64);put(p,2,5,nb1 as u64);put(p,2,6,nb2 as u64);put(p,2,7,nb3 as u64);
 put(p,3,0,dim as u64);
 put(p,4,0,ne1 as u64);put(p,4,1,ne2 as u64);put(p,4,2,ne3 as u64);put(p,4,3,64 as u64);
}

fn run(packet:i64)->bool{
 let dst=arg(packet,0);
 ensure(dst!=0,10);
 if ti(dst,5,0)!=oc(0){return false;}
 let s0=ti(dst,0,0);
 let s1=ti(dst,0,1);
 ensure(s0!=0&&s1!=0,11);
 let t0=ti(s0,2,0);
 let t1=ti(s1,2,0);
 let td=ti(dst,2,0);
 // native: src0->type == src1->type && dst->type == src0->type
 if !(t0==t1&&td==t0){return false;}
 let dim=ti(dst,8,0);
 if dim<0||dim>3{return false;}
 let w=width_of(t0);
 // quantized alignment/contiguity asserts belong to both operands
 if tblk(t0)>1{
  if dim==3{ensure(cont3(s0)==1&&cont(s0)==1&&cont3(s1)==1&&cont(s1)==1,24);}
  else{ensure(cont3(s0)==1&&cont3(s1)==1,25);}
  let bs=tblk(t0);
  ensure(nev(s0,0)%bs==0&&nev(s1,0)%bs==0,26);
 }
 let ne00=nev(s0,0);let ne01=nev(s0,1);let ne02=nev(s0,2);let ne03=nev(s0,3);
 let ne0=nev(dst,0);let ne1=nev(dst,1);let ne2=nev(dst,2);let ne3=nev(dst,3);
 let p=plan_ptr(packet);
 // Native branch order:
 // 1) dim != 3 && contiguous_to_3(src0) && contiguous_to_3(src1) -> concat_cont per i3
 // 2) dim == 3 && contiguous(src0) && contiguous(src1)            -> two memcpy
 // 3) otherwise                                                   -> concat_non_cont
 let c3a=cont3(s0);let c3b=cont3(s1);
 if dim!=3 as i64&&c3a==1&&c3b==1{
  // per-i3 slice pointers advance by nb3/width elements in native; the parent
  // adds that offset itself through the launch adapter, one launch per i3.
  ensure(ne3>0,50);
  // Native passes ggml_row_size(type, ne[0])/sizeof(T) as the row length in
  // copy units: row bytes = ne[0]*type_size/block_size.
  // Native passes ggml_row_size(type, ne[0])/sizeof(T) for BOTH operands, so a
  // quantized row is measured in bytes, not in elements. ne1/ne2 stay element
  // extents (dimension counts, not row lengths).
  let row0=row_units(t0,ne00,w);
  let rowd=row_units(td,ne0,w);
  ensure(row0>0&&rowd>0,51);
  let np0=nbv(s0,3);let np1=nbv(s1,3);let npd=nbv(dst,3);
  ensure(np0%(w as i64)==0&&np1%(w as i64)==0&&npd%(w as i64)==0,52);
  plan_cont(p,w,row0,ne01,ne02,rowd,ne1,ne2,dim);
  put(p,0,4,ne3 as u64);
  put(p,0,5,nbv(s0,3) as u64);put(p,0,6,nbv(s1,3) as u64);put(p,0,7,nbv(dst,3) as u64);
  put(p,5,0,1 as u64);
  ensure(launch(packet)==0,90);
  return true;
 }
 if dim==3 as i64&&cont(s0)==1&&cont(s1)==1{
  // Transport only: the parent issues cudaMemcpyAsync(dst,src0,n0) then
  // (dst+n0,src1,n1) on the same stream, exactly as native.
  let n0=itg(ti(s0,9,0));let n1=itg(ti(s1,9,0));
  put(p,5,0,2 as u64);
  put(p,7,0,n0);put(p,7,1,n1);
  ensure(launch(packet)==0,91);
  return true;
 }
 // Non-contiguous path. Native asserts !ggml_is_quantized here; a quantized
 // tensor only reaches it after the alignment/contiguity asserts above, and the
 // native code would abort, so require the same non-quantized contract.
 ensure(tblk(t0)<=1,60);
 plan_nc(p,w,ne00,ne01,ne02,ne03,nbv(s0,0),nbv(s0,1),nbv(s0,2),nbv(s0,3),
  nbv(s1,0),nbv(s1,1),nbv(s1,2),nbv(s1,3),
  ne0,ne1,ne2,ne3,nbv(dst,0),nbv(dst,1),nbv(dst,2),nbv(dst,3),dim);
 put(p,5,0,0 as u64);
 ensure(launch(packet)==0,92);
 return true;
}
// Native passes ggml_row_size(src0->type, src0->ne[0])/sizeof(T) as the row
// length in copy units: row_bytes = ne[0]*type_size/block_size, divided by the
// copy unit w.
fn row_units(ty:i64,ne0:i64,w:i64)->i64{
 let bs:i64=tblk(ty);
 let tsz:i64=tsz(ty);
 let rb:i64=ne0*tsz/bs;
 ensure(rb>=0&&w>0,53);
 ensure(rb%w==0,54);
 return rb/w;
}

fn main(){run(0);}
