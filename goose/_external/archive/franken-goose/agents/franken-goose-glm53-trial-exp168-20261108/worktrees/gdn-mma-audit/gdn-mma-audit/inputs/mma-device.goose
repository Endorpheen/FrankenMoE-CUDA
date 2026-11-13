// fg174m: gated-delta-net TensorCore path ported from gated-delta-net-mma.cu gdn_single<C=16,V>
// (CUDA AMPERE_MMA_AVAILABLE branch) using the actual ggml_cuda_mma primitives of mma.cuh.
// Block dim3(32,8): lane=threadIdx.x, warp=threadIdx.y. A block owns V value columns (V=32
// slice of D=128, or the whole head for V=128) and advances by C=16 tokens per chunk. FP32
// recurrent state lives in per-warp register tiles; q/k/v/P/inverse/delta operands are packed
// to bf16 hi + bf16 residual lo in swizzled dynamic shared memory via matrix<>::store2/index
// math; fragment loads use ldmatrix x2/x4(.trans) with the mma.cuh per-lane swizzled
// addresses; every product issues the source three-mma order mma(al,bh); mma(ah,bl); mma(ah,bh)
// (al*bl second-order term dropped by the original design). Only raw hardware instructions
// (bf16 convert, ldmatrix, mma.m16n8k16.bf16), global/shared data movement and warp shuffles
// are effects; address computation, layout selection, control flow and arithmetic stay here.
// HIP V64/RDNA3.5 and MUSA branches deferred (unsupported here, no scalar substitute).
// Shared word map (u32 words): Q_HI 0, Q_LO 1024, K_HI 2048, K_LO 3072, P_HI 4096, P_LO 4224,
// INV_HI 4352, INV_LO 4480, DELTA_HI 4608, DELTA_LO 4608+8V, SCR_HI 4608+16V (state/solved
// union; solved lo at SCR_HI+8V), LOWER SCR_HI+128V (f32), PREFIX +256, BETA +16, WORDS=BETA+16
// (9504 words/38016 B for V=32; 23328/93312 B for V=128) == C++ sizeof(shared<16,V>).
struct F4 { x:f32, y:f32, z:f32, w:f32 }
struct U4 { a:u32, b:u32, c:u32, d:u32 }
struct U2 { a:u32, b:u32 }
struct QK { q0:f32, q1:f32, q2:f32, q3:f32, k0:f32, k1:f32, k2:f32, k3:f32 }
extern "fg174m_load" fn load(p:u64,i:u64)->f32;
extern "fg174m_load4" fn load4(p:u64,i:u64)->F4;
extern "fg174m_store" fn store(p:u64,i:u64,v:f32);
extern "fg174m_expf" fn expf(x:f32)->f32;
extern "fg174m_barrier" fn barrier();
extern "fg174m_shfl_up" fn shfl_up(x:f32,o:i32)->f32;
extern "fg174m_shfl" fn shfl(x:f32,s:i32)->f32;
extern "fg174m_sm_u32" fn sm_u32(p:u64,i:u64)->u32;
extern "fg174m_sm_store_u32" fn sm_store_u32(p:u64,i:u64,v:u32);
extern "fg174m_sm_bf16" fn sm_bf16(p:u64,i:u64)->f32;
extern "fg174m_sm_store_bf16" fn sm_store_bf16(p:u64,i:u64,v:f32);
extern "fg174m_sm_f32" fn sm_f32(p:u64,i:u64)->f32;
extern "fg174m_sm_store_f32" fn sm_store_f32(p:u64,i:u64,v:f32);
extern "fg174m_cvt2" fn cvt2(a:f32,b:f32)->u32;
extern "fg174m_wlo" fn wlo(w:u32)->f32;
extern "fg174m_whi" fn whi(w:u32)->f32;
extern "fg174m_ldm4" fn ldm4(a:u64)->U4;
extern "fg174m_ldm2" fn ldm2(a:u64)->U2;
extern "fg174m_ldm4t" fn ldm4t(a:u64)->U4;
extern "fg174m_ldm2t" fn ldm2t(a:u64)->U2;
extern "fg174m_mma" fn mma(d:F4,a0:u32,a1:u32,a2:u32,a3:u32,b0:u32,b1:u32)->F4;
// Current full self-hosted producer supports the native bitwise XOR.
fn xor4(a:i32,b:i32)->i32{return a^b;}
// Exact ggml_cuda_mma::swizzle<j,nv_bfloat162>(p,i): XOR the 16-byte chunk index (pair units,
// shift=2) by the stride-bytes key: 32B (i/4)%2, 64B (i/2)%4, >=128B i%8, 16B none.
fn swz(p:i32,i:i32,j:i32)->i32{let nc=j/4;if nc==1{return p;}let pp=p%j;var key:i32=i%8;if nc==2{key=(i/4)%2;}else{if nc==4{key=(i/2)%4;}}return p-pp+xor4(pp/4,key)*4+pp%4;}
// tile<16,8,float> fragment mapping (mma.cuh): get_i(l)=(l/2)*8+lane/4, get_j(l)=2*(lane%4)+l%2.
fn gi(e:i32,lane:i32)->i32{return (e/2)*8+lane/4;}
fn gj(e:i32,lane:i32)->i32{return 2*(lane%4)+e%2;}
fn axe(x:F4,e:i32)->f32{return if e==0{x.x}else{if e==1{x.y}else{if e==2{x.z}else{x.w}}};}
// matrix<>::store2: bf16 hi pair + residual lo pair at the swizzled word (hi base hi, lo hi+hw).
fn st2(smem:u64,hi:i64,hw:i64,j:i32,r:i32,c:i32,a:f32,b:f32){let w=cvt2(a,b);let wd=swz(r*j+c/2,r,j) as u64;sm_store_u32(smem+(hi as u64)*(4 as u64),wd,w);sm_store_u32(smem+((hi+hw) as u64)*(4 as u64),wd,cvt2(a-wlo(w),b-whi(w)));}
// matrix<>::store: one bf16 element + residual via 2-byte stores (no pair RMW race).
fn st_bf(smem:u64,hi:i64,hw:i64,j:i32,r:i32,c:i32,v:f32){let ei=(2*swz(r*j+c/2,r,j)+c%2) as u64;sm_store_bf16(smem+(hi as u64)*(4 as u64),ei,v);sm_store_bf16(smem+((hi+hw) as u64)*(4 as u64),ei,v-wlo(cvt2(v,v)));}
// load_ldmatrix_swizzled/_trans for a_tile tile<16,8>: i=lane%16, jj=(lane/16)*4,
// offset=row*stride+col/2 (trans: col*stride+row/2), swizzle key i; x4 result in tile.x order.
fn ld_a(smem:u64,base:i64,j:i32,trans:bool,row:i32,col:i32,lane:i32)->U4{let i=lane%16;let jj=(lane/16)*4;let off=if trans{col*j+row/2}else{row*j+col/2};let ad=smem+((base+swz(off+i*j+jj,i,j)) as u64)*(4 as u64);if trans{let z=ldm4t(ad);return U4{z.a,z.c,z.b,z.d};}return ldm4(ad);}
// b_tile tile<8,8>: non-trans i=lane%8 jj=(lane%2)*4; trans i=lane%16 jj=0; x2 results.
fn ld_b(smem:u64,base:i64,j:i32,trans:bool,row:i32,col:i32,lane:i32)->U2{if trans{let i=lane%16;let off=col*j+row/2;return ldm2t(smem+((base+swz(off+i*j,i,j)) as u64)*(4 as u64));}let i=lane%8;let jj=((lane/8)%2)*4;let off=row*j+col/2;return ldm2(smem+((base+swz(off+i*j+jj,i,j)) as u64)*(4 as u64));}
// product<K,TA,TB>: per 16-K slice hi/lo tile loads + the source three-mma decomposition.
fn product(dp:F4,kk:i32,ta:bool,tb:bool,smem:u64,ahi:i64,ahw:i64,aj:i32,row:i32,col:i32,bhi:i64,bhw:i64,bj:i32,lane:i32)->F4{var d:F4=dp;var k:i32=0;while k<kk{let ah=ld_a(smem,ahi,aj,ta,row,k,lane);let al=ld_a(smem,ahi+ahw,aj,ta,row,k,lane);let bh=ld_b(smem,bhi,bj,tb,col,k,lane);let bl=ld_b(smem,bhi+bhw,bj,tb,col,k,lane);d=mma(d,al.a,al.b,al.c,al.d,bh.a,bh.b);d=mma(d,ah.a,ah.b,ah.c,ah.d,bl.a,bl.b);d=mma(d,ah.a,ah.b,ah.c,ah.d,bh.a,bh.b);k+=16;}return d;}
// read_qk: 4 floats per lane for row t0+warp; float4 iff both q and k addresses 16B aligned.
fn load_qk(t0:i64,warp:i32,lane:i32,nt:i64,qoff:i64,sq2:i64,q:u64,k:u64)->QK{let t=t0+(warp as i64);if t>=nt{return QK{0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};}let off=qoff+t*sq2+(4*lane) as i64;if (q+(off as u64)*(4 as u64))%16==0&&(k+(off as u64)*(4 as u64))%16==0{let qv=load4(q,off as u64);let kv=load4(k,off as u64);return QK{qv.x,qv.y,qv.z,qv.w,kv.x,kv.y,kv.z,kv.w};}return QK{load(q,off as u64),load(q,(off+1) as u64),load(q,(off+2) as u64),load(q,(off+3) as u64),load(k,off as u64),load(k,(off+1) as u64),load(k,(off+2) as u64),load(k,(off+3) as u64)};}
// warp0 inclusive shfl prefix scan of g with cross-32 carry; prefix/beta stored for t<16.
fn gate_scan(smem:u64,g:u64,beta:u64,goff:i64,sb2:i64,t0:i64,valid:i32,lane:i32,pre:i64,bet:i64){var carry:f32=0.0;var base:i32=0;while base<16{let t=base+lane;var gv:f32=0.0;var bv:f32=0.0;if t<valid{gv=load(g,(goff+(t0+(t as i64))*sb2) as u64);bv=load(beta,(goff+(t0+(t as i64))*sb2) as u64);}var o:i32=1;while o<32{let pv=shfl_up(gv,o);if lane>=o{gv+=pv;}o*=2;}gv+=carry;if t<16{sm_store_f32(smem+(pre as u64)*(4 as u64),t as u64,gv);sm_store_f32(smem+(bet as u64)*(4 as u64),t as u64,bv);}carry=shfl(gv,31);base+=32;}}
// (I+A)^-1 rows via backward substitution over strict-lower A=beta*tril(K Kt decay); descending
// pivots see final x[pivot]; pivot>0 is exact because A row 0 is zero.
fn substitute(smem:u64,lower:i64,inv:i64,warp:i32,lane:i32){var row=warp;while row<16{var x1:f32=0.0;if lane==row{x1=1.0;}else{if lane<row{x1=-sm_f32(smem+(lower as u64)*(4 as u64),(row*16+lane) as u64);}}var pv:i32=row-1;while pv>0{let xp=shfl(x1,pv);if lane<pv{x1-=xp*sm_f32(smem+(lower as u64)*(4 as u64),(pv*16+lane) as u64);}pv-=1;}if lane<16{st_bf(smem,inv,128,8,row,lane,x1);}row+=8;}}
// V=32 entry: PREFETCH and SPLIT_PROJECTION both on (source constexpr for C=16,V=32).
fn gdn_v32(q:u64,k:u64,v:u64,g:u64,beta:u64,state:u64,dst:u64,state_out:u64,smem:u64,H:i32,Hk:i32,rq3:i32,nt:i64,sq1:i64,sq2:i64,sq3:i64,sv1:i64,sv2:i64,sv3:i64,sb1:i64,sb2:i64,sb3:i64,lane:i32,warp:i32,bidx:i32,sc:f32){
 let slice=bidx%4;let rem=bidx/4;let h=rem%H;let seq=rem/H;
 let v0=slice*32;
 let soff=((seq as i64)*(H as i64)+(h as i64))*16384;
 let qoff=((seq as i64)/(rq3 as i64))*sq3+((h%Hk) as i64)*sq1;
 let voff=(seq as i64)*sv3+(h as i64)*sv1+(v0 as i64);
 let goff=(seq as i64)*sb3+(h as i64)*sb1;
 let obase=((seq as i64)*nt*(H as i64)+(h as i64))*128+(v0 as i64);
 var st:f32[16]=default<f32[16]>();
 var bo:f32[4]=default<f32[4]>();
 var j:i32=0;
 while j<4{
  let ti=warp+8*j;let r=(ti/4)*16;let c=(ti%4)*8;
  var e:i32=0;
  while e<4{st[4*j+e]=load(state,(soff+(((v0+c+gj(e,lane)) as i64)*128+((r+gi(e,lane)) as i64))) as u64);e+=1;}
  j+=1;
 }
 var nq:QK=load_qk(0,warp,lane,nt,qoff,sq2,q,k);
 var t0:i64=0;
 while t0<nt{
  let valid=if nt-t0<16{(nt-t0) as i32}else{16};
  let qa=nq;
  st2(smem,0,1024,64,warp,4*lane,qa.q0*sc,qa.q1*sc);
  st2(smem,0,1024,64,warp,4*lane+2,qa.q2*sc,qa.q3*sc);
  st2(smem,2048,1024,64,warp,4*lane,qa.k0,qa.k1);
  st2(smem,2048,1024,64,warp,4*lane+2,qa.k2,qa.k3);
  let qb=load_qk(t0+8,warp,lane,nt,qoff,sq2,q,k);
  st2(smem,0,1024,64,warp+8,4*lane,qb.q0*sc,qb.q1*sc);
  st2(smem,0,1024,64,warp+8,4*lane+2,qb.q2*sc,qb.q3*sc);
  st2(smem,2048,1024,64,warp+8,4*lane,qb.k0,qb.k1);
  st2(smem,2048,1024,64,warp+8,4*lane+2,qb.k2,qb.k3);
  if warp==0{gate_scan(smem,g,beta,goff,sb2,t0,valid,lane,9472,9488);}
  j=0;
  while j<4{
   let ti=warp+8*j;let r=(ti/4)*16;let c=(ti%4)*8;
   var e:i32=0;
   while e<4{st2(smem,5120,2048,16,r+gi(e,lane),c+gj(e,lane),st[4*j+e],st[4*j+e+1]);e+=2;}
   j+=1;
  }
  barrier();
  if t0+16<nt{nq=load_qk(t0+16,warp,lane,nt,qoff,sq2,q,k);}
  if warp<4{
   var ti=warp;
   while ti<4{
    let c=(ti%2)*8;
    var d0:F4=F4{0.0,0.0,0.0,0.0};
    if ti<2{d0=product(d0,128,false,false,smem,2048,1024,64,0,c,2048,1024,64,lane);}
    else{d0=product(d0,128,false,false,smem,0,1024,64,0,c,2048,1024,64,lane);}
    var e:i32=0;
    while e<4{
     let rr=gi(e,lane);let cc=c+gj(e,lane);
     var vf:f32=0.0;
     if rr<valid&&cc<=rr{vf=axe(d0,e)*expf(sm_f32(smem+(9472 as u64)*(4 as u64),rr as u64)-sm_f32(smem+(9472 as u64)*(4 as u64),cc as u64));}
     if ti<2{if cc<rr{sm_store_f32(smem+(9216 as u64)*(4 as u64),(rr*16+cc) as u64,vf*sm_f32(smem+(9488 as u64)*(4 as u64),rr as u64));}}
     else{st_bf(smem,4096,128,8,rr,cc,vf);}
     e+=1;
    }
    ti+=4;
   }
  }
  if warp>=4{
   let c=(warp-4)*8;
   var qs:F4=F4{0.0,0.0,0.0,0.0};
   var ks:F4=F4{0.0,0.0,0.0,0.0};
   qs=product(qs,128,false,true,smem,0,1024,64,0,c,5120,2048,16,lane);
   ks=product(ks,128,false,true,smem,2048,1024,64,0,c,5120,2048,16,lane);
   var e:i32=0;
   while e<4{
    let t=gi(e,lane);let vc=c+gj(e,lane);
    let dec=expf(sm_f32(smem+(9472 as u64)*(4 as u64),t as u64));
    var vf:f32=0.0;
    if t<valid{vf=load(v,(voff+(t0+(t as i64))*sv2+(vc as i64)) as u64);}
    st_bf(smem,4608,256,16,t,vc,sm_f32(smem+(9488 as u64)*(4 as u64),t as u64)*(vf-dec*axe(ks,e)));
    bo[e]=dec*axe(qs,e);
    e+=1;
   }
  }
  barrier();
  substitute(smem,9216,4352,warp,lane);
  barrier();
  var ti=warp;
  while ti<4{
   let c=ti*8;
   let x=product(F4{0.0,0.0,0.0,0.0},16,false,true,smem,4352,128,8,0,c,4608,256,16,lane);
   var e:i32=0;
   while e<4{st2(smem,5120,256,16,gi(e,lane),c+gj(e,lane),axe(x,e),axe(x,e+1));e+=2;}
   ti+=8;
  }
  barrier();
  if warp>=4{
   let c=(warp-4)*8;
   let x=product(F4{0.0,0.0,0.0,0.0},16,false,true,smem,4096,128,8,0,c,5120,256,16,lane);
   var e:i32=0;
   while e<4{
    let t=gi(e,lane);let vc=c+gj(e,lane);
    if t<valid{store(dst,(obase+(t0+(t as i64))*(H as i64)*128+(vc as i64)) as u64,bo[e]+axe(x,e));}
    e+=1;
   }
  }
  var ii:i32=warp*32+lane;
  while ii<512{
   let t=ii/32;let vc=ii%32;
   let ei=(2*swz(t*16+vc/2,t,16)+vc%2) as u64;
   let xs=sm_bf16(smem+(5120 as u64)*(4 as u64),ei)+sm_bf16(smem+(5376 as u64)*(4 as u64),ei);
   st_bf(smem,4608,256,16,t,vc,xs*expf(sm_f32(smem+(9472 as u64)*(4 as u64),(valid-1) as u64)-sm_f32(smem+(9472 as u64)*(4 as u64),t as u64)));
   ii+=256;
  }
  barrier();
  let dec2=expf(sm_f32(smem+(9472 as u64)*(4 as u64),(valid-1) as u64));
  j=0;
  while j<4{
   let ti=warp+8*j;let r=(ti/4)*16;let c=(ti%4)*8;
   var e:i32=0;
   while e<4{st[4*j+e]*=dec2;e+=1;}
   let x=product(F4{st[4*j],st[4*j+1],st[4*j+2],st[4*j+3]},16,true,true,smem,2048,1024,64,r,c,4608,256,16,lane);
   st[4*j]=x.x;st[4*j+1]=x.y;st[4*j+2]=x.z;st[4*j+3]=x.w;
   j+=1;
  }
  barrier();
  t0+=16;
 }
 j=0;
 while j<4{
  let ti=warp+8*j;let r=(ti/4)*16;let c=(ti%4)*8;
  var e:i32=0;
  while e<4{store(state_out,(soff+(((v0+c+gj(e,lane)) as i64)*128+((r+gi(e,lane)) as i64))) as u64,st[4*j+e]);e+=1;}
  j+=1;
 }
}
// V=128 entry: PREFETCH off, SPLIT_PROJECTION off (project after substitution, all warps).
fn gdn_v128(q:u64,k:u64,v:u64,g:u64,beta:u64,state:u64,dst:u64,state_out:u64,smem:u64,H:i32,Hk:i32,rq3:i32,nt:i64,sq1:i64,sq2:i64,sq3:i64,sv1:i64,sv2:i64,sv3:i64,sb1:i64,sb2:i64,sb3:i64,lane:i32,warp:i32,bidx:i32,sc:f32){
 let h=bidx%H;let seq=bidx/H;
 let soff=((seq as i64)*(H as i64)+(h as i64))*16384;
 let qoff=((seq as i64)/(rq3 as i64))*sq3+((h%Hk) as i64)*sq1;
 let voff=(seq as i64)*sv3+(h as i64)*sv1;
 let goff=(seq as i64)*sb3+(h as i64)*sb1;
 let obase=((seq as i64)*nt*(H as i64)+(h as i64))*128;
 var st:f32[64]=default<f32[64]>();
 var bo:f32[8]=default<f32[8]>();
 var j:i32=0;
 while j<16{
  let ti=warp+8*j;let r=(ti/16)*16;let c=(ti%16)*8;
  var e:i32=0;
  while e<4{st[4*j+e]=load(state,(soff+(((c+gj(e,lane)) as i64)*128+((r+gi(e,lane)) as i64))) as u64);e+=1;}
  j+=1;
 }
 var t0:i64=0;
 while t0<nt{
  let valid=if nt-t0<16{(nt-t0) as i32}else{16};
  let qa=load_qk(t0,warp,lane,nt,qoff,sq2,q,k);
  st2(smem,0,1024,64,warp,4*lane,qa.q0*sc,qa.q1*sc);
  st2(smem,0,1024,64,warp,4*lane+2,qa.q2*sc,qa.q3*sc);
  st2(smem,2048,1024,64,warp,4*lane,qa.k0,qa.k1);
  st2(smem,2048,1024,64,warp,4*lane+2,qa.k2,qa.k3);
  let qb=load_qk(t0+8,warp,lane,nt,qoff,sq2,q,k);
  st2(smem,0,1024,64,warp+8,4*lane,qb.q0*sc,qb.q1*sc);
  st2(smem,0,1024,64,warp+8,4*lane+2,qb.q2*sc,qb.q3*sc);
  st2(smem,2048,1024,64,warp+8,4*lane,qb.k0,qb.k1);
  st2(smem,2048,1024,64,warp+8,4*lane+2,qb.k2,qb.k3);
  if warp==0{gate_scan(smem,g,beta,goff,sb2,t0,valid,lane,23296,23312);}
  j=0;
  while j<16{
   let ti=warp+8*j;let r=(ti/16)*16;let c=(ti%16)*8;
   var e:i32=0;
   while e<4{st2(smem,6656,8192,64,r+gi(e,lane),c+gj(e,lane),st[4*j+e],st[4*j+e+1]);e+=2;}
   j+=1;
  }
  barrier();
  if warp<4{
   var ti=warp;
   while ti<4{
    let c=(ti%2)*8;
    var d0:F4=F4{0.0,0.0,0.0,0.0};
    if ti<2{d0=product(d0,128,false,false,smem,2048,1024,64,0,c,2048,1024,64,lane);}
    else{d0=product(d0,128,false,false,smem,0,1024,64,0,c,2048,1024,64,lane);}
    var e:i32=0;
    while e<4{
     let rr=gi(e,lane);let cc=c+gj(e,lane);
     var vf:f32=0.0;
     if rr<valid&&cc<=rr{vf=axe(d0,e)*expf(sm_f32(smem+(23296 as u64)*(4 as u64),rr as u64)-sm_f32(smem+(23296 as u64)*(4 as u64),cc as u64));}
     if ti<2{if cc<rr{sm_store_f32(smem+(23040 as u64)*(4 as u64),(rr*16+cc) as u64,vf*sm_f32(smem+(23312 as u64)*(4 as u64),rr as u64));}}
     else{st_bf(smem,4096,128,8,rr,cc,vf);}
     e+=1;
    }
    ti+=8;
   }
  }
  barrier();
  substitute(smem,23040,4352,warp,lane);
  var ti=warp;
  while ti<16{
   let c=ti*8;
   var qs:F4=F4{0.0,0.0,0.0,0.0};
   var ks:F4=F4{0.0,0.0,0.0,0.0};
   qs=product(qs,128,false,true,smem,0,1024,64,0,c,6656,8192,64,lane);
   ks=product(ks,128,false,true,smem,2048,1024,64,0,c,6656,8192,64,lane);
   var e:i32=0;
   while e<4{
    let t=gi(e,lane);let vc=c+gj(e,lane);
    let dec=expf(sm_f32(smem+(23296 as u64)*(4 as u64),t as u64));
    var vf:f32=0.0;
    if t<valid{vf=load(v,(voff+(t0+(t as i64))*sv2+(vc as i64)) as u64);}
    st_bf(smem,4608,1024,64,t,vc,sm_f32(smem+(23312 as u64)*(4 as u64),t as u64)*(vf-dec*axe(ks,e)));
    bo[4*(ti/8)+e]=dec*axe(qs,e);
    e+=1;
   }
   ti+=8;
  }
  barrier();
  ti=warp;
  while ti<16{
   let c=ti*8;
   let x=product(F4{0.0,0.0,0.0,0.0},16,false,true,smem,4352,128,8,0,c,4608,1024,64,lane);
   var e:i32=0;
   while e<4{st2(smem,6656,1024,64,gi(e,lane),c+gj(e,lane),axe(x,e),axe(x,e+1));e+=2;}
   ti+=8;
  }
  barrier();
  ti=warp;
  while ti<16{
   let c=ti*8;
   let x=product(F4{0.0,0.0,0.0,0.0},16,false,true,smem,4096,128,8,0,c,6656,1024,64,lane);
   var e:i32=0;
   while e<4{
    let t=gi(e,lane);let vc=c+gj(e,lane);
    if t<valid{store(dst,(obase+(t0+(t as i64))*(H as i64)*128+(vc as i64)) as u64,bo[4*(ti/8)+e]+axe(x,e));}
    e+=1;
   }
   ti+=8;
  }
  var ii:i32=warp*32+lane;
  while ii<2048{
   let t=ii/128;let vc=ii%128;
   let ei=(2*swz(t*64+vc/2,t,64)+vc%2) as u64;
   let xs=sm_bf16(smem+(6656 as u64)*(4 as u64),ei)+sm_bf16(smem+(7680 as u64)*(4 as u64),ei);
   st_bf(smem,4608,1024,64,t,vc,xs*expf(sm_f32(smem+(23296 as u64)*(4 as u64),(valid-1) as u64)-sm_f32(smem+(23296 as u64)*(4 as u64),t as u64)));
   ii+=256;
  }
  barrier();
  let dec2=expf(sm_f32(smem+(23296 as u64)*(4 as u64),(valid-1) as u64));
  j=0;
  while j<16{
   let ti=warp+8*j;let r=(ti/16)*16;let c=(ti%16)*8;
   var e:i32=0;
   while e<4{st[4*j+e]*=dec2;e+=1;}
   let x=product(F4{st[4*j],st[4*j+1],st[4*j+2],st[4*j+3]},16,true,true,smem,2048,1024,64,r,c,4608,1024,64,lane);
   st[4*j]=x.x;st[4*j+1]=x.y;st[4*j+2]=x.z;st[4*j+3]=x.w;
   j+=1;
  }
  barrier();
  t0+=16;
 }
 j=0;
 while j<16{
  let ti=warp+8*j;let r=(ti/16)*16;let c=(ti%16)*8;
  var e:i32=0;
  while e<4{store(state_out,(soff+(((c+gj(e,lane)) as i64)*128+((r+gi(e,lane)) as i64))) as u64,st[4*j+e]);e+=1;}
  j+=1;
 }
}
// Retained entries; parent never executes main.
fn main(){gdn_v32(0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0,0,0,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0,0,0,1.0);gdn_v128(0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0,0,0,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0,0,0,1.0);}
