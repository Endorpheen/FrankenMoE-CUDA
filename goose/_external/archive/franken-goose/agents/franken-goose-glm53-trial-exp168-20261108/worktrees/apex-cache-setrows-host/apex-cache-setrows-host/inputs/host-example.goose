// Goose host dispatch/support/scratch-allocation planning for the CUDA FlashAttention host path (EXP168, Apex).
// Mirrors the pinned snapshot: fattn.cu ggml_cuda_flash_attn_ext/_supported/_get_alloc_size,
// ggml_cuda_get_best_fattn_kernel, ggml_cuda_fattn_kv_type_supported, ggml_cuda_get_fattn_vec_case;
// fattn-common.cuh ggml_cuda_flash_attn_ext_get_f16_extra_data and launch_fattn; fattn-vec.cuh vec case
// selection; fattn-mma-f16.cuh head switch, ncols chain, may/shall_use_sparse, ampere config, case host
// planning; common.cuh arch predicates, occupancy/wave and stream-K policy. Routing, sizing, ownership
// and selection live here; raw CUDA, pool, f32->f16 staging and launches are parent bridge effects and
// numerical kernels belong to other Goose modules. Apex scope: DKQ=DV=256, Q F32, K/V Q4_0, mask F16,
// padded KV cache (K->ne[1]%256==0), GQA 16/2=8, CUDA Ada (cc 890). Other native configurations are
// still routed exactly but reported deferred (plan 3/28) for the parent's preserved native path.
extern "fg175h_lock" fn init_lock(d:i64,inst:i64);extern "fg175h_unlock" fn init_unlock(d:i64,inst:i64);
extern "fg175h_arg" fn arg(packet:i64,k:i64)->i64;extern "fg175h_plan" fn plan_ptr(packet:i64)->i64;
extern "fg175h_put" fn put(p:i64,k:i64,i:i64,v:u64);extern "fg175h_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg175h_putf" fn putf(p:i64,i:i64,v:f32);extern "fg175h_ti" fn ti(t:i64,k:i64,i:i64)->i64;
extern "fg175h_tpf" fn tpf(t:i64,i:i64)->f32;extern "fg175h_tpi" fn tpi(t:i64,i:i64)->i64;
extern "fg175h_tc" fn tc(k:i64)->i64;extern "fg175h_oc" fn oc(k:i64)->i64;extern "fg175h_ndev" fn ndev()->i64;
extern "fg175h_dev" fn dev(d:i64,k:i64)->i64;extern "fg175h_carch" fn carch(d:i64)->i64;
extern "fg175h_blck" fn blck(ty:i64)->i64;extern "fg175h_tsize" fn tsize(ty:i64)->i64;
extern "fg175h_nel" fn nel(t:i64)->i64;extern "fg175h_nrows" fn nrows(t:i64)->i64;
extern "fg175h_setdev" fn setdev(d:i64);extern "fg175h_occ" fn occ(p:i64)->i64;
extern "fg175h_setattr" fn setattr(p:i64)->i64;extern "fg175h_memo" fn memo(d:i64,inst:i64)->i64;
extern "fg175h_memo_set" fn memo_set(d:i64,inst:i64,v:i64);extern "fg175h_pool" fn pool(p:i64,slot:i64)->i64;
extern "fg175h_conv" fn conv(p:i64,which:i64)->i64;extern "fg175h_launch" fn launch(p:i64)->i64;
extern "fg175h_lfix" fn lfix(p:i64,kind:i64)->i64;extern "fg175h_powf" fn powf(a:f32,b:f32)->f32;
extern "fg175h_u2f" fn u2f(v:u64)->f32;extern "fg175h_fail" fn fail(code:i64);
// checked arithmetic; GGML_ASSERT/GGML_ABORT equivalents route through fail
fn ensure(v:bool,c:i64){if !v{fail(c);}}fn U()->u64{return 18446744073709551615 as u64;}
fn add(a:u64,b:u64)->u64{ensure(a<=U()-b,2);return a+b;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,1);return a*b;}fn itg(a:i64)->u64{ensure(a>=0,4);return a as u64;}
fn cld(a:u64,b:u64)->u64{ensure(b!=0,3);return a/b+if a%b!=0{1 as u64}else{0 as u64};}fn mnu(a:u64,b:u64)->u64{return if a<b{a}else{b};}
fn mxu(a:u64,b:u64)->u64{return if a>b{a}else{b};}fn pad128(a:u64)->u64{return add(a/128*128,if a%128!=0{128 as u64}else{0 as u64});}
fn gne(t:i64,i:i64)->i64{return ti(t,3,i);}fn gck(x:u64,y:u64,z:u64){ensure(x>0&&x<=2147483647&&y<=65535&&z<=65535,25);}
// real ggml enum values come from the bridge; never reconstructed here
fn is_quant(ty:i64)->bool{return !(ty==tc(0)||ty==tc(1)||ty==tc(2));}fn kv_sup(ty:i64)->bool{var k:i64=0;while k<8{if ty==tc(k){return true;}k+=1;}return false;}
fn isnv(cc:i64)->bool{return cc<65536;}fn isamd(cc:i64)->bool{return cc>=16777216;}
fn wmma(cc:i64)->bool{return isamd(cc)&&cc>=16779264;}fn mfma(cc:i64)->bool{return isamd(cc)&&cc>=16777480&&cc<16777744&&cc!=16777481&&cc!=16777484;}
fn tmma(d:i64,cc:i64)->bool{return isnv(cc)&&carch(d)>=750;}fn amma(d:i64,cc:i64)->bool{return isnv(cc)&&carch(d)>=800;}
fn vmma(d:i64,cc:i64)->bool{return isnv(cc)&&carch(d)==700;}fn cpasync(d:i64,cc:i64)->bool{return isnv(cc)&&carch(d)>=800;}
// vector case lookup, FATTN_VEC_CASE order; pair=typeK*7+typeV over F16,Q4_0,Q4_1,Q5_0,Q5_1,Q8_0,BF16;
// compiled set mirrors the Apex build macros GGML_CUDA_FA_F16_F16 (0) and GGML_CUDA_FA_Q4_0_Q4_0 (8);
// K/V F32 remap to F16 like ggml_cuda_get_fattn_vec_case; other pairs -> nullptr -> native f16-fallback
// behavior (with staging), never forced by this module.
fn vid(ty:i64)->i64{if ty==tc(0)||ty==tc(1){return 0;}if ty==tc(3){return 1;}if ty==tc(4){return 2;}
 if ty==tc(5){return 3;}if ty==tc(6){return 4;}if ty==tc(7){return 5;}if ty==tc(2){return 6;}fail(6);return 0;}
fn vec_pair(head:i64,kt:i64,vt:i64)->i64{if head!=64&&head!=128&&head!=256{return -1;}
 let pair:i64=vid(kt)*7+vid(vt);return if pair==0||pair==8{pair}else{-1};}
fn ltype(l:i64)->i64{return if l==0{tc(1)}else{if l==1{tc(3)}else{if l==2{tc(4)}else{if l==3{tc(5)}else{if l==4{tc(6)}else{if l==5{tc(7)}else{tc(2)}}}}}};}
// ggml_cuda_flash_attn_ext_mma_f16_may_use_sparse (exact table)
fn may_sparse(dkq:i64,dv:i64,c1:i64,c2:i64)->bool{return (dkq==512&&dv==512&&c1==1&&c2==8)||(dkq==576&&dv==512&&c1==1&&c2==16)||(dkq==256&&dv==256&&c1==1&&c2==8)||(dkq==256&&dv==256&&c1==8&&c2==8);}
// ggml_cuda_flash_attn_ext_mma_f16_shall_use_sparse
fn shall_sparse(d:i64,cc:i64,dst:i64,c1:i64,c2:i64)->bool{
 let q=ti(dst,0,0);let k=ti(dst,0,1);let m=ti(dst,0,3);
 let nkv=tpi(dst,4);
 var b0:i64=64/c2;if gne(q,1)<b0{b0=gne(q,1);}
 let base:i64=if c1==1{b0}else{c1};
 let ng:i64=base*nkv;
 let need:i64=if 2*ng>4096{2*ng}else{4096};
 return isnv(cc)&&tmma(d,cc)&&m!=0&&nkv>0&&tpf(dst,1)==0.0&&tpf(dst,2)==0.0&&
  gne(m,0)==gne(k,1)&&gne(m,1)>=gne(q,1)&&gne(m,2)==1&&gne(k,1)>=need;
}
// gqa-optimization predicate shared by best-kernel selection and switch_ncols2 (native computes it twice)
fn gopt_of(dst:i64)->bool{
 let q=ti(dst,0,0);let k=ti(dst,0,1);let v=ti(dst,0,2);let m=ti(dst,0,3);
 let mb=tpf(dst,1);
 var gopt:bool=m!=0&&mb==0.0&&ti(k,3,1)%256==0;
 var t:i64=0;
 while t<4{var x:i64=0;if t==0{x=q;}else{if t==1{x=k;}else{if t==2{x=v;}else{x=m;}}}
  if x!=0&&!is_quant(ti(x,2,0)){var i:i64=1;while i<4{if ti(x,4,i)%16!=0{gopt=false;}i+=1;}}
  t+=1;}
 return gopt;
}
// ggml_cuda_get_best_fattn_kernel: exact selection; route enums NONE=0 VEC=100 TILE=200 MMA_F16=400
fn best_kernel(d:i64,dst:i64)->i64{
 let q=ti(dst,0,0);let k=ti(dst,0,1);let v=ti(dst,0,2);let m=ti(dst,0,3);
 ensure(q!=0&&k!=0&&v!=0,7);
 ensure(ti(k,3,2)>0&&ti(q,3,2)%ti(k,3,2)==0,8);
 let gqa:i64=ti(q,3,2)/ti(k,3,2);
 let mb=tpf(dst,1);
 let gopt:bool=gqa>=2&&gopt_of(dst);
 let cc=dev(d,0);let kne0=ti(k,3,0);let vne0=ti(v,3,0);let qne0=ti(q,3,0);
 if !(kne0==40||kne0==64||kne0==72||kne0==80||kne0==96||kne0==112||kne0==128||kne0==192||kne0==256||kne0==320||kne0==512||kne0==576){return 0;}
 if (kne0==40||kne0==64||kne0==72||kne0==80||kne0==96||kne0==112||kne0==128||kne0==256)&&vne0!=kne0{return 0;}
 if kne0==192&&(vne0!=128||!gopt||gqa%8!=0){return 0;}
 if kne0==320&&(vne0!=256||!gopt||gqa%32!=0){return 0;}
 if kne0==512&&(vne0!=kne0||!gopt){return 0;}
 if kne0==576&&(vne0!=512||!gopt){return 0;}
 if !kv_sup(ti(k,2,0))||!kv_sup(ti(v,2,0)){return 0;}
 if m!=0&&ti(m,3,2)!=1{return 0;}
 let canvec:bool=qne0<=256&&qne0%64==0&&qne0!=192&&ti(k,3,1)%256==0;
 if tmma(d,cc)&&qne0!=40&&qne0!=72{
  if canvec{
   if !is_quant(ti(k,2,0))&&!is_quant(ti(v,2,0)){
    let spd:bool=gopt&&gqa>4&&may_sparse(kne0,vne0,1,8)&&shall_sparse(d,cc,dst,1,8);
    if !spd&&cc>=890&&ti(q,3,1)==1&&ti(q,3,3)==1&&!(gqa>4&&(qne0>=256||ti(k,3,1)>=8192)){return 100;}
   }else{
    if cc>=890{if ti(q,3,1)<=2{return 100;}}else{if ti(q,3,1)==1{return 100;}}
   }
   if !gopt&&ti(q,3,1)==1{return 100;}
  }
  return 400;
 }
 let c2m:i64=if qne0==320{32}else{if qne0==576||qne0==192{16}else{8}};
 var ge:i64=1;
 while mb==0.0&&gqa%(2*ge)==0&&ge<c2m{ge*=2;}
 if vmma(d,cc)&&qne0!=40&&qne0!=72{
  if canvec&&ti(q,3,1)*ge<=2{return 100;}
  if ti(q,3,1)*ge<=16{return 200;}
  return 400;
 }
 if mfma(cc)&&qne0!=40&&qne0!=72{
  let qn:i64=ti(q,3,1)*ge;
  if qne0<=64&&qn>8{return 400;}
  if qne0<=128&&qn>16{return 400;}
  if qne0<=256&&qn>64{return 400;}
  if qne0>256&&gopt&&qn>128{return 400;}
 }
 let thr:i64=if qne0<=128{8}else{16};
 if wmma(cc)&&gopt&&qne0<=256&&qne0!=40&&qne0!=72&&ti(q,3,1)*ge>thr{return 400;}
 if canvec{
  if !is_quant(ti(k,2,0))&&!is_quant(ti(v,2,0)){if ti(q,3,1)==1{if !gopt{return 100;}}}
  else{if ti(q,3,1)<=2{return 100;}}
 }
 return 200;
}
// ggml_cuda_flash_attn_ext_mma_f16_switch_ncols1
fn sw1(d:i64,dst:i64,dkq:i64,dv:i64,c2:i64)->i64{
 let cc=dev(d,0);let q=ti(dst,0,0);
 if may_sparse(dkq,dv,1,c2){
  let wide:bool=may_sparse(dkq,dv,64/c2,c2);
  let big:bool=gne(q,1)>32/c2;
  if !(wide&&big)&&shall_sparse(d,cc,dst,1,c2){return 1;}
 }
 if c2<=8&&tmma(d,cc)&&gne(q,1)<=8/c2{return 8/c2;}
 if c2<=16&&gne(q,1)<=16/c2{return 16/c2;}
 if gne(q,1)<=32/c2||(isnv(cc)&&carch(d)==750)||(isamd(cc)&&dkq>256){return 32/c2;}
 return 64/c2;
}
// Return ncols2; ncols1 is selected separately so GQA grouping is not lost.
// ggml_cuda_flash_attn_ext_mma_f16_switch_ncols2
fn sw2(d:i64,dst:i64,dkq:i64,dv:i64)->i64{
 let cc=dev(d,0);let k=ti(dst,0,1);let q=ti(dst,0,0);
 let gopt:bool=gopt_of(dst);
 ensure(ti(k,3,2)>0&&ti(q,3,2)%ti(k,3,2)==0,8);
 let gqa:i64=ti(q,3,2)/ti(k,3,2);
 if cc==700{
  if gopt&&gqa%8==0{return 8;}
  if gopt&&gqa%4==0{return 4;}
  if dkq<=256{if gopt&&gqa%2==0{return 2;}return 1;}
  fail(9);return 0;
 }
 if wmma(cc){
  if gopt&&gqa%8==0{return 8;}
  if gopt&&gqa%4==0{return 4;}
  if gopt&&gqa%2==0{return 2;}
 }
 if gopt&&gqa>4{return 8;}
 if gopt&&gqa>2{return 4;}
 if gopt&&gqa>1{return 2;}
 if dkq<=256{return 1;}
 fail(9);return 0;
}
// ggml_cuda_flash_attn_ext_mma_f16 head-size switch; writes ncols1/ncols2/DV/DKQ to plan 3/2..5
fn mma_pick(d:i64,dst:i64,p:i64){
 let cc=dev(d,0);let q=ti(dst,0,0);let k=ti(dst,0,1);let v=ti(dst,0,2);let m=ti(dst,0,3);
 let qn=gne(q,0);
 if qn==64||qn==80||qn==96||qn==112||qn==128||qn==256{
  ensure(gne(v,0)==qn,10);
  let c2=sw2(d,dst,qn,qn);let c1=sw1(d,dst,qn,qn,c2);
  put(p,3,2,itg(c1));put(p,3,3,itg(c2));put(p,3,4,itg(qn));put(p,3,5,itg(qn));return;
 }
 if qn==192{
  ensure(gne(v,0)==128,10);
  ensure(m!=0&&tpf(dst,1)==0.0,11);
  ensure(gne(k,2)>0&&gne(q,2)%gne(k,2)==0,8);
  let gqa:i64=gne(q,2)/gne(k,2);
  var c2:i64=8;if gqa%16==0{c2=16;}else{ensure(gqa%8==0,12);}
  let c1=sw1(d,dst,192,128,c2);
  put(p,3,2,itg(c1));put(p,3,3,itg(c2));put(p,3,4,128 as u64);put(p,3,5,192 as u64);return;
 }
 if qn==320{
  ensure(gne(v,0)==256,10);
  ensure(m!=0&&tpf(dst,1)==0.0,11);
  ensure(gne(k,2)>0&&gne(q,2)%gne(k,2)==0,8);
  ensure(gne(q,2)/gne(k,2)%32==0,12);
  let c1=sw1(d,dst,320,256,32);
  put(p,3,2,itg(c1));put(p,3,3,32 as u64);put(p,3,4,256 as u64);put(p,3,5,320 as u64);return;
 }
 if qn==512{
  ensure(gne(v,0)==512,10);
  let c2=sw2(d,dst,512,512);let c1=sw1(d,dst,512,512,c2);
  put(p,3,2,itg(c1));put(p,3,3,itg(c2));put(p,3,4,512 as u64);put(p,3,5,512 as u64);return;
 }
 if qn==576{
  ensure(gne(v,0)==512,10);
  ensure(m!=0&&tpf(dst,1)==0.0,11);
  ensure(gne(k,2)>0&&gne(q,2)%gne(k,2)==0,8);
  let gqa:i64=gne(q,2)/gne(k,2);
  var c2:i64=4;
  if gqa==20&&isnv(cc){
   if cc>=1210{c2=if gne(q,1)<=8{16}else{4};}
   else{if cc>=1200{c2=if gne(q,1)<=4&&gne(k,1)>=65536{16}else{4};}
   else{if cc>=890{c2=if gne(q,1)<=4{16}else{4};}
   else{if cc>=750{if gne(q,1)<=4{c2=if gne(k,1)<=16384{16}else{32};}else{c2=4;}}
   else{c2=4;}}}}
  }else{if gqa%16==0{c2=16;}}
  let c1=sw1(d,dst,576,512,c2);
  put(p,3,2,itg(c1));put(p,3,3,itg(c2));put(p,3,4,512 as u64);put(p,3,5,576 as u64);return;
 }
 fail(9);
}
// ggml_cuda_fattn_mma_get_config_ampere; field: 0 nthreads 1 occupancy 2 nbatch_fa 3 nbatch_K2
// 4 nbatch_V2 5 nbatch_combine 6 nstages_target 7 Q_in_reg; unreachable combos fail (13)
fn mma_cfg(dkq:i64,dv:i64,ncols:i64,f:i64)->i64{
 var a:i64=0;var b:i64=0;var c:i64=0;var e:i64=0;var g:i64=0;var h:i64=0;var s:i64=0;var r:i64=0;
 if dkq==64&&dv==64&&ncols==8{a=128;b=2;c=128;e=32;g=32;h=32;s=2;r=1;}
 if dkq==64&&dv==64&&ncols==16{a=128;b=2;c=64;e=32;g=32;h=32;s=2;r=1;}
 if dkq==64&&dv==64&&ncols==32{a=128;b=2;c=64;e=32;g=32;h=32;s=2;r=1;}
 if dkq==64&&dv==64&&ncols==64{a=128;b=2;c=64;e=32;g=32;h=32;s=2;r=1;}
 if dkq==80&&dv==80&&ncols==8{a=128;b=2;c=128;e=40;g=40;h=40;s=2;r=1;}
 if dkq==80&&dv==80&&ncols==16{a=128;b=2;c=64;e=40;g=40;h=40;s=2;r=1;}
 if dkq==80&&dv==80&&ncols==32{a=128;b=2;c=64;e=40;g=40;h=40;s=2;r=1;}
 if dkq==80&&dv==80&&ncols==64{a=128;b=2;c=64;e=40;g=40;h=40;s=2;r=1;}
 if dkq==96&&dv==96&&ncols==8{a=128;b=2;c=128;e=48;g=48;h=48;s=2;r=1;}
 if dkq==96&&dv==96&&ncols==16{a=128;b=2;c=64;e=48;g=48;h=48;s=2;r=1;}
 if dkq==96&&dv==96&&ncols==32{a=128;b=2;c=64;e=48;g=48;h=48;s=2;r=1;}
 if dkq==96&&dv==96&&ncols==64{a=128;b=2;c=64;e=48;g=48;h=48;s=2;r=1;}
 if dkq==112&&dv==112&&ncols==8{a=128;b=2;c=128;e=56;g=56;h=56;s=2;r=1;}
 if dkq==112&&dv==112&&ncols==16{a=128;b=2;c=64;e=56;g=56;h=56;s=2;r=1;}
 if dkq==112&&dv==112&&ncols==32{a=128;b=2;c=64;e=56;g=56;h=56;s=2;r=1;}
 if dkq==112&&dv==112&&ncols==64{a=128;b=2;c=64;e=56;g=56;h=56;s=2;r=1;}
 if dkq==128&&dv==128&&ncols==8{a=128;b=2;c=128;e=64;g=64;h=64;s=2;r=1;}
 if dkq==128&&dv==128&&ncols==16{a=128;b=2;c=64;e=64;g=64;h=64;s=2;r=1;}
 if dkq==128&&dv==128&&ncols==32{a=128;b=2;c=64;e=64;g=64;h=64;s=2;r=1;}
 if dkq==128&&dv==128&&ncols==64{a=128;b=2;c=64;e=64;g=64;h=64;s=2;r=1;}
 if dkq==192&&dv==128&&ncols==8{a=64;b=4;c=64;e=96;g=64;h=64;s=2;r=1;}
 if dkq==192&&dv==128&&ncols==16{a=64;b=4;c=32;e=96;g=64;h=64;s=2;r=1;}
 if dkq==192&&dv==128&&ncols==32{a=128;b=2;c=32;e=96;g=64;h=64;s=2;r=1;}
 if dkq==192&&dv==128&&ncols==64{a=128;b=2;c=32;e=96;g=64;h=64;s=2;r=1;}
 if dkq==256&&dv==256&&ncols==8{a=128;b=2;c=64;e=128;g=128;h=128;s=2;r=1;}
 if dkq==256&&dv==256&&ncols==16{a=256;b=1;c=64;e=128;g=128;h=128;s=2;r=1;}
 if dkq==256&&dv==256&&ncols==32{a=128;b=2;c=32;e=128;g=128;h=128;s=2;r=1;}
 if dkq==256&&dv==256&&ncols==64{a=128;b=2;c=32;e=128;g=128;h=128;s=2;r=1;}
 if dkq==320&&dv==256&&ncols==32{a=128;b=2;c=32;e=128;g=128;h=128;s=1;r=0;}
 if dkq==320&&dv==256&&ncols==64{a=256;b=1;c=32;e=128;g=128;h=128;s=1;r=0;}
 if dkq==512&&dv==512&&ncols==8{a=128;b=2;c=64;e=128;g=128;h=128;s=1;r=0;}
 if dkq==512&&dv==512&&ncols==16{a=256;b=1;c=64;e=128;g=128;h=128;s=1;r=0;}
 if dkq==512&&dv==512&&ncols==32{a=256;b=1;c=32;e=128;g=128;h=128;s=1;r=0;}
 if dkq==512&&dv==512&&ncols==64{a=256;b=1;c=32;e=128;g=128;h=128;s=1;r=0;}
 if dkq==576&&dv==512&&ncols==8{a=64;b=4;c=32;e=288;g=256;h=128;s=1;r=0;}
 if dkq==576&&dv==512&&ncols==16{a=64;b=4;c=32;e=288;g=256;h=128;s=1;r=0;}
 if dkq==576&&dv==512&&ncols==32{a=128;b=2;c=32;e=160;g=128;h=128;s=1;r=0;}
 if dkq==576&&dv==512&&ncols==64{a=256;b=1;c=32;e=160;g=128;h=128;s=1;r=0;}
 if a==0{fail(13);}
 if f==0{return a;}if f==1{return b;}if f==2{return c;}if f==3{return e;}if f==4{return g;}if f==5{return h;}if f==6{return s;}return r;
}
fn contig(t:i64)->bool{let bs=itg(blck(ti(t,2,0)));ensure(bs>0,3);let packed=mul(itg(nel(t)),itg(tsize(ti(t,2,0))))/bs;return itg(ti(t,7,0))==packed;}
// ggml_cuda_flash_attn_ext_get_f16_extra_data: K/V stage ptrs and end -> plan 0/5, 0/6, 0/15
fn f16_extra(dst:i64,p:i64,needk:bool,needv:bool,vkv:bool)->u64{
 let k=ti(dst,0,1);let v=ti(dst,0,2);ensure(k!=0&&v!=0,7);
 var end:u64=add(ti(dst,1,0) as u64,itg(ti(dst,7,0)));
 var ks:u64=0;var vs:u64=0;
 if needk&&ti(k,2,0)!=tc(1){end=pad128(end);ks=end;end=add(end,mul(itg(nel(k)),2));}
 if needv&&ti(v,2,0)!=tc(1){if vkv{vs=ks;}else{end=pad128(end);vs=end;end=add(end,mul(itg(nel(v)),2));}}
 put(p,0,5,ks);put(p,0,6,vs);put(p,0,15,end);
 return end;
}
// ggml_cuda_flash_attn_ext_get_alloc_size: staged span minus dst->data -> plan 3/0
fn alloc_size(dst:i64,best:i64,p:i64){
 let k=ti(dst,0,1);let v=ti(dst,0,2);ensure(k!=0&&v!=0,7);
 var needk:bool=false;var needv:bool=false;
 if best==200||best==400{needk=true;needv=true;}
 else{if best==100{
  let fb:bool=vec_pair(gne(ti(dst,0,0),0),ti(k,2,0),ti(v,2,0))<0;
  needk=ti(k,2,0)==tc(0)||fb;needv=ti(v,2,0)==tc(0)||fb;}}
 let vkv:bool=ti(v,8,0)!=0&&(ti(v,8,0)==k||(ti(v,8,0)==ti(k,8,0)&&ti(v,9,0)==ti(k,9,0)));
 let end=f16_extra(dst,p,needk,needv,vkv);
 put(p,3,0,end-(ti(dst,1,0) as u64));
}

// Native cases outside this active Apex geometry remain explicit deferred compatibility paths.
fn apex_scope(d:i64,dst:i64)->bool{
 let q=ti(dst,0,0);let k=ti(dst,0,1);let v=ti(dst,0,2);let m=ti(dst,0,3);
 if q==0||k==0||v==0{return false;}
 if !amma(d,dev(d,0))||dev(d,4)!=32{return false;}
 if ti(q,2,0)!=tc(0)||ti(k,2,0)!=tc(3)||ti(v,2,0)!=tc(3)||ti(dst,2,0)!=tc(0){return false;}
 if gne(q,0)!=256||gne(k,0)!=256||gne(v,0)!=256||gne(q,2)!=16||gne(k,2)!=2||gne(v,2)!=2{return false;}
 if tpf(dst,1)!=0.0||tpf(dst,2)!=0.0||tpi(dst,4)>0{return false;}
 if gne(k,1)%256!=0{return false;}
 if gne(q,1)>2&&(m==0||!gopt_of(dst)){return false;}
 return true;
}
// Checked reachable-byte span; allocation capacity remains a caller/backend contract.
fn tensor_span(t:i64)->u64{
 ensure(t!=0,40);let ty=ti(t,2,0);let bs=itg(blck(ty));let ts=itg(tsize(ty));ensure(bs>0&&ts>0,41);
 var i:i64=0;var count:u64=1;
 while i<4{ensure(gne(t,i)>0&&ti(t,4,i)>=0,42);count=mul(count,itg(gne(t,i)));i+=1;}
 let n0=itg(gne(t,0));ensure(n0%bs==0&&itg(ti(t,4,0))==ts,43);
 var span=mul(n0/bs,ts);i=1;
 while i<4{span=add(span,mul(itg(gne(t,i)-1),itg(ti(t,4,i))));i+=1;}
 ensure(span<=9223372036854775807,44);ensure(itg(ti(t,7,0))>=span,45);
 add(ti(t,1,0) as u64,span);return span;
}
fn disjoint(a:u64,na:u64,b:u64,nb:u64){ensure(add(a,na)<=b||add(b,nb)<=a,46);}
fn validate_apex(dst:i64,dispatch:bool,p:i64){
 let q=ti(dst,0,0);let k=ti(dst,0,1);let v=ti(dst,0,2);let m=ti(dst,0,3);let sk=ti(dst,0,4);
 tensor_span(q);tensor_span(k);tensor_span(v);tensor_span(dst);
 ensure(gne(q,1)<=2147483647&&gne(q,3)<=65535,47);
 ensure(gne(k,1)==gne(v,1)&&gne(k,3)==gne(q,3)&&gne(v,3)==gne(q,3),48);
 ensure(gne(dst,0)==256&&gne(dst,1)==16&&gne(dst,2)==gne(q,1)&&gne(dst,3)==gne(q,3),49);
 ensure(ti(dst,4,1)==1024&&ti(dst,4,2)==16384&&itg(ti(dst,4,3))==mul(itg(gne(q,1)),16384),50);
 var i:i64=1;while i<4{ensure(ti(q,4,i)<=2147483647,51);if i<3{ensure(ti(k,4,i)<=2147483647&&ti(v,4,i)<=2147483647,51);}i+=1;}
 ensure(gne(k,1)<=2147483616,47);
 ensure(mul(mul(cld(itg(gne(k,1)),32 as u64),cld(itg(gne(q,1)),4 as u64)),mul(2 as u64,itg(gne(q,3))))<=2147483647,52);
 if m!=0{tensor_span(m);ensure(ti(m,2,0)==tc(1)&&gne(m,0)==gne(k,1)&&gne(m,1)>=gne(q,1)&&gne(m,2)==1&&gne(m,3)>0,53);ensure(ti(m,4,1)<=2147483647&&ti(m,4,2)<=2147483647,51);}
 if sk!=0{tensor_span(sk);ensure(ti(sk,2,0)==tc(0)&&nel(sk)>=16&&contig(sk),54);}
 if dispatch{
  ensure(ti(dst,1,0)!=0&&ti(q,1,0)!=0&&ti(k,1,0)!=0&&ti(v,1,0)!=0,55);
  let end=f16_extra(dst,p,gne(q,1)>2,gne(q,1)>2,ti(v,8,0)!=0&&(ti(v,8,0)==k||(ti(v,8,0)==ti(k,8,0)&&ti(v,9,0)==ti(k,9,0))));
  let out=ti(dst,1,0) as u64;let outspan=end-out;disjoint(out,outspan,ti(q,1,0) as u64,tensor_span(q));disjoint(out,outspan,ti(k,1,0) as u64,tensor_span(k));disjoint(out,outspan,ti(v,1,0) as u64,tensor_span(v));
  if m!=0{ensure(ti(m,1,0)!=0,55);disjoint(out,outspan,ti(m,1,0) as u64,tensor_span(m));}
  if sk!=0{ensure(ti(sk,1,0)!=0,55);disjoint(out,outspan,ti(sk,1,0) as u64,tensor_span(sk));}
 }
}
// Raw staging launch dimensions and byte/element metadata are computed by Goose.
fn stage_plan(p:i64,t:i64,out:u64,kind:u64){
 put(p,6,0,ti(t,1,0) as u64);put(p,6,1,out);var i:i64=0;
 while i<4{put(p,6,2+i,itg(gne(t,i)));if i>0{put(p,6,5+i,itg(ti(t,4,i)));}i+=1;}
 put(p,6,14,kind);put(p,6,13,itg(nel(t))/32);
 if kind==1{put(p,6,9,cld(itg(nel(t)),256 as u64));put(p,6,10,1 as u64);put(p,6,11,1 as u64);put(p,6,12,32 as u64);}
 else{put(p,6,9,cld(itg(gne(t,0)),512 as u64));put(p,6,10,mnu(itg(gne(t,1)),65535 as u64));put(p,6,11,mnu(mul(itg(gne(t,2)),itg(gne(t,3))),65535 as u64));put(p,6,12,256 as u64);}
 gck(get(p,6,9),get(p,6,10),get(p,6,11));
}


// SDK adapters receive a fully planned launch; no scheduling or sizing is hidden in C++.
fn fix_effect(p:i64,kind:i64){
 put(p,7,0,itg(kind));put(p,7,4,256 as u64);put(p,7,5,0 as u64);
 put(p,7,6,get(p,1,1));put(p,7,7,get(p,1,2));put(p,7,8,get(p,3,31));put(p,7,9,get(p,3,14));
 put(p,7,10,get(p,3,33));put(p,7,12,get(p,3,37));
 if kind==0{
  put(p,7,1,get(p,3,19));put(p,7,2,get(p,1,3));put(p,7,3,1 as u64);put(p,7,4,128 as u64);
  put(p,7,11,get(p,1,5)/256);put(p,7,17,get(p,2,9)/4);put(p,7,18,get(p,2,11)/4);
 }else{if kind==2{
  put(p,7,1,get(p,3,18));put(p,7,2,get(p,3,2));put(p,7,3,get(p,3,3));
  put(p,7,13,get(p,5,12));put(p,7,14,get(p,5,13));put(p,7,15,get(p,5,14));
 }else{if kind==3{
  put(p,7,1,get(p,3,14));put(p,7,2,get(p,3,2));put(p,7,3,get(p,3,3));
  put(p,7,13,get(p,5,15));put(p,7,14,get(p,5,16));put(p,7,15,get(p,5,17));put(p,7,16,get(p,5,18));
 }else{ensure(kind==4,57);put(p,7,1,get(p,1,1));put(p,7,2,get(p,1,2));put(p,7,3,get(p,1,3));put(p,7,5,mul(get(p,3,13),8 as u64));}}}
 gck(get(p,7,1),get(p,7,2),get(p,7,3));ensure(lfix(p,kind)==0,26);
}

// launch_fattn: staging, tiles, sparse/mask-scan scratch, occupancy/waves, stream-K, grids, fixups
fn launch_fattn(packet:i64,d:i64,dst:i64,p:i64,c1:u64,c2:u64,dv:u64,needk:bool,needv:bool,streamk:bool,sparse:bool,nwarps:u64,smem:u64,nfa:u64,ws:u64,apre:bool){
 let q=ti(dst,0,0);let k=ti(dst,0,1);let v=ti(dst,0,2);let m=ti(dst,0,3);let sk=ti(dst,0,4);
 ensure(ti(q,2,0)==tc(0)&&ti(dst,2,0)==tc(0),15);
 ensure(ti(q,4,0)==tsize(tc(0))&&ti(k,4,0)==tsize(ti(k,2,0))&&ti(v,4,0)==tsize(ti(v,2,0)),16);
 ensure(m==0||ti(m,2,0)==tc(1),17);
 put(p,0,0,ti(q,1,0) as u64);put(p,0,1,ti(k,1,0) as u64);put(p,0,2,ti(v,1,0) as u64);
 put(p,0,3,if m!=0{ti(m,1,0) as u64}else{0 as u64});put(p,0,4,if sk!=0{ti(sk,1,0) as u64}else{0 as u64});
 put(p,0,12,ti(dst,1,0) as u64);put(p,3,2,c1);put(p,3,3,c2);put(p,3,4,dv);put(p,3,35,mul(c1,c2));
 put(p,3,10,if streamk{1 as u64}else{0 as u64});put(p,3,11,if sparse{1 as u64}else{0 as u64});
 put(p,3,12,if apre{1 as u64}else{0 as u64});
 var i:i64=1;
 while i<4{put(p,1,i,ti(q,3,i) as u64);put(p,1,4+i,ti(k,3,i) as u64);put(p,1,8+i,ti(v,3,i) as u64);
  put(p,2,i-1,ti(q,4,i) as u64);
  if m!=0{put(p,1,12+i,ti(m,3,i) as u64);put(p,2,8+i,ti(m,4,i) as u64);}
  else{put(p,1,12+i,0 as u64);put(p,2,8+i,0 as u64);}
  i+=1;}
 put(p,1,0,ti(q,3,0) as u64);put(p,1,4,ti(k,3,0) as u64);put(p,1,8,ti(v,3,0) as u64);
 put(p,1,12,if m!=0{ti(m,3,0) as u64}else{0 as u64});
 let vkv:bool=ti(v,8,0)!=0&&(ti(v,8,0)==k||(ti(v,8,0)==ti(k,8,0)&&ti(v,9,0)==ti(k,9,0)));
 f16_extra(dst,p,needk,needv,vkv);
 var kd=ti(k,1,0) as u64;var vd=ti(v,1,0) as u64;
 var nb1=ti(k,4,1) as u64;var nb2=ti(k,4,2) as u64;var nb3=ti(k,4,3) as u64;
 var wb1=ti(v,4,1) as u64;var wb2=ti(v,4,2) as u64;var wb3=ti(v,4,3) as u64;
 if needk&&ti(k,2,0)!=tc(1){
  let ks=get(p,0,5);ensure(ks!=0,18);
  let bs=itg(blck(ti(k,2,0)));let ts=itg(tsize(ti(k,2,0)));
  if contig(k){put(p,5,9,1 as u64);put(p,5,1,nel(k) as u64);
   nb1=mul(mul(nb1,bs),2)/ts;nb2=mul(mul(nb2,bs),2)/ts;nb3=mul(mul(nb3,bs),2)/ts;}
  else{ensure(ti(k,4,0) as u64==ts,16);put(p,5,9,2 as u64);put(p,5,1,nel(k) as u64);
   put(p,5,3,(ti(k,4,1) as u64)/ts);put(p,5,4,(ti(k,4,2) as u64)/ts);put(p,5,5,(ti(k,4,3) as u64)/ts);
   nb1=mul(itg(ti(k,3,0)),2);nb2=mul(itg(ti(k,3,1)),nb1);nb3=mul(itg(ti(k,3,2)),nb2);}
  stage_plan(p,k,ks,get(p,5,9));ensure(conv(p,0)==0,31);kd=ks;
 }
 if needv&&ti(v,2,0)!=tc(1){
  if vkv{vd=kd;wb1=nb1;wb2=nb2;wb3=nb3;}
  else{let vs=get(p,0,6);ensure(vs!=0,18);
   let bs=itg(blck(ti(v,2,0)));let ts=itg(tsize(ti(v,2,0)));
   if contig(v){put(p,5,10,1 as u64);put(p,5,2,nel(v) as u64);
    wb1=mul(mul(wb1,bs),2)/ts;wb2=mul(mul(wb2,bs),2)/ts;wb3=mul(mul(wb3,bs),2)/ts;}
   else{ensure(ti(v,4,0) as u64==ts,16);put(p,5,10,2 as u64);put(p,5,2,nel(v) as u64);
    put(p,5,6,(ti(v,4,1) as u64)/ts);put(p,5,7,(ti(v,4,2) as u64)/ts);put(p,5,8,(ti(v,4,3) as u64)/ts);
    wb1=mul(itg(ti(v,3,0)),2);wb2=mul(itg(ti(v,3,1)),wb1);wb3=mul(itg(ti(v,3,2)),wb2);}
   stage_plan(p,v,vs,get(p,5,10));ensure(conv(p,1)==0,31);vd=vs;}
 }
 put(p,0,7,kd);put(p,0,8,vd);put(p,2,3,nb1);put(p,2,4,nb2);put(p,2,5,nb3);
 put(p,2,6,wb1);put(p,2,7,wb2);put(p,2,8,wb3);
 let qne1=itg(ti(q,3,1));let qne2=itg(ti(q,3,2));let qne3=itg(ti(q,3,3));
 let kne1=itg(ti(k,3,1));let kne2=itg(ti(k,3,2));
 ensure(kne2>0&&qne2%kne2==0,8);let gqa=qne2/kne2;
 let ntx=cld(qne1,c1);let ntzg=cld(gqa,c2);
 let ntdst=mul(mul(mul(ntx,ntzg),kne2),qne3);
 put(p,3,19,ntx);put(p,3,31,gqa);
 var nkv:u64=kne1;
 if sparse{
  ensure(m!=0,19);ensure(c1==1||c1==8,20);
  let nkvq=tpi(dst,4);ensure(nkvq>0,21);
  let nkvm=mnu(kne1,mul(c1,itg(nkvq)));
  let nlists=mul(ntx,itg(ti(m,3,3)));
  put(p,3,23,nkvm);put(p,3,24,nlists);put(p,3,27,mul(nkvm,nlists)+nlists);put(p,5,11,mul(nkvm,nlists));
  let kp=pool(p,0);ensure(kp!=0,22);put(p,0,10,kp as u64);
  nkv=nkvm;
  gck(cld(qne1,c1),itg(ti(m,3,3)),1 as u64);
  fix_effect(p,1);
 }
 let scan:bool=!sparse&&m!=0&&kne1%256==0&&(qne1>=1024||qne3>1);
 if scan{put(p,3,27,mul(ntx,qne3));put(p,5,11,0 as u64);
  let kp=pool(p,0);ensure(kp!=0,22);put(p,0,10,kp as u64);
  gck(ntx,qne3,1 as u64);fix_effect(p,0);}
 put(p,3,20,if scan{1 as u64}else{0 as u64});
 let cc=dev(d,0);let nsm=itg(dev(d,1));
 put(p,3,30,ws);put(p,3,9,smem);put(p,3,7,nwarps);put(p,3,8,nfa);put(p,3,6,mul(ws,nwarps));
 let mbps=occ(p);ensure(mbps>0,23);
 var pb=itg(mbps);
 let ntkv=cld(nkv,nfa);
 var gx:u64=0;var gy:u64=0;var gz:u64=0;var dse:u64=0;var dme:u64=0;
 if streamk{
  let pref:bool=isnv(cc)&&cc==1210&&apre&&scan;
  let maxb=mul(pb,nsm);
  let nw=cld(ntdst,maxb);
  let eff=mul(100 as u64,ntdst)/mul(nw,maxb);
  var usk:bool=false;
  if pref&&eff>=75{usk=false;}
  else{if isnv(cc)&&cc>=890{usk=true;}else{if wmma(cc)&&itg(ti(q,3,0))==64{usk=true;}else{usk=eff<75;}}}
  gx=ntdst;gy=1 as u64;gz=1 as u64;
  if usk{let raw=mnu(maxb,mul(ntkv,ntdst));
   let rounded=(raw/ntdst)*ntdst;
   let loss=if raw>0{mul(100 as u64,raw-rounded)/raw}else{100 as u64};
   gx=if loss<=5{rounded}else{raw};}
  if ntdst%gx!=0{dme=mul(mul(gx,mul(c1,c2)),2+dv/2);}
 }else{
  pb=mnu(pb,ntkv);
  let bpw=mul(itg(mbps),nsm);
  var nwb:u64=0;var eb:u64=0;var pbt=pb;var stop:bool=false;
  while pbt<=ntkv&&!stop{
   let nbt=mul(ntdst,pbt);
   let nwi=cld(nbt,bpw);
   let eff=mul(100 as u64,nbt)/mul(nwi,bpw);
   if eb>=95&&nwi>nwb{stop=true;}
   else{if eff>eb{nwb=nwi;eb=eff;pb=pbt;}pbt+=1;}
  }
  gx=ntx;gy=pb;gz=mul(mul(ntzg,kne2),qne3);
  if pb>1{dse=mul(pb,itg(nel(dst)));dme=mul(pb,itg(nrows(dst)));}
 }
 put(p,3,25,dse);put(p,3,26,dme);
 if dse>0{let dp=pool(p,1);ensure(dp!=0,22);put(p,0,13,dp as u64);}
 if dme>0{let mp=pool(p,2);ensure(mp!=0,22);put(p,0,11,mp as u64);}
 put(p,3,25,dse);put(p,3,26,dme);
 put(p,0,9,if !streamk&&pb>1{get(p,0,13)}else{ti(dst,1,0) as u64});
 var scale=tpf(dst,0);let mb=tpf(dst,1);let soft=tpf(dst,2);
 if soft!=0.0{scale=scale/soft;}
 ensure(ti(q,3,2)>0&&ti(q,3,2)<2147483648,24);
 var nhl2:u64=1;while nhl2*2<=qne2{nhl2*=2;}
 let nhf=u2f(nhl2);
 putf(p,0,scale);putf(p,1,mb);putf(p,2,powf(2.0,(0.0-mb)/nhf));putf(p,3,powf(2.0,(0.0-(mb/2.0))/nhf));putf(p,4,soft);
 put(p,5,0,nhl2);gck(gx,gy,gz);
 put(p,3,14,gx);put(p,3,15,gy);put(p,3,16,gz);put(p,3,17,nkv);put(p,3,18,ntdst);put(p,3,13,pb);
 ensure(launch(p)==0,26);
 put(p,5,12,mul(mul(ntx,ntzg),kne2));put(p,5,13,mul(ntx,ntzg));put(p,5,14,ntx);
 put(p,5,15,mul(mul(mul(ntkv,ntx),ntzg),kne2));put(p,5,16,mul(mul(ntkv,ntx),ntzg));
 put(p,5,17,mul(ntkv,ntx));put(p,5,18,ntkv);
 if streamk{
  if gx%ntdst==0&&gx>ntdst{put(p,3,33,gx/ntdst);gck(ntdst,c1,c2);fix_effect(p,2);}
  else{if ntdst%gx!=0{put(p,3,37,mul(ntkv,ntdst));gck(gx,c1,c2);fix_effect(p,3);}}
 }else{if pb>1{gck(qne1,qne2,qne3);fix_effect(p,4);}}
}
// ggml_cuda_flash_attn_ext_vec_case[_impl]: small-query vector route
fn vec_plan(packet:i64,d:i64,dst:i64,p:i64)->bool{
 let q=ti(dst,0,0);let k=ti(dst,0,1);let v=ti(dst,0,2);
 let qn=gne(q,0);
 ensure(qn==64||qn==128||qn==256,14);
 let soft=tpf(dst,2);
 let c1:u64=if gne(q,1)==1{1 as u64}else{2 as u64};
 let pair=vec_pair(qn,ti(k,2,0),ti(v,2,0));
 put(p,3,21,if pair<0{U()}else{itg(pair)});
 let kt2:i64=if pair<0{0}else{pair/7};
 let vt2:i64=if pair<0{0}else{pair%7};
 let needk:bool=ltype(kt2)==tc(1);let needv:bool=ltype(vt2)==tc(1);
 put(p,3,1,100 as u64);put(p,3,34,0 as u64);put(p,3,36,if c1==1{0 as u64}else{1 as u64});
 put(p,3,22,if soft!=0.0{1 as u64}else{0 as u64});
 launch_fattn(packet,d,dst,p,c1,1 as u64,itg(qn),needk,needv,false,false,4 as u64,4608 as u64,itg(qn),32 as u64,false);
 return true;
}
// ggml_cuda_flash_attn_ext_mma_f16_case host planning; ampere-family table only, other GPU
// families stay deferred to the preserved native path
fn mma_plan(packet:i64,d:i64,dst:i64,p:i64)->bool{
 let cc=dev(d,0);
 if !amma(d,cc){return false;}
 mma_pick(d,dst,p);
 let dkq=get(p,3,5) as i64;let dvv=get(p,3,4) as i64;
 let c1=get(p,3,2) as i64;let c2=get(p,3,3) as i64;
 let ncols=c1*c2;
 let soft=tpf(dst,2);
 var sparse:bool=false;
 if soft==0.0&&may_sparse(dkq,dvv,c1,c2)&&shall_sparse(d,cc,dst,c1,c2){sparse=true;}
 let ntt=mma_cfg(dkq,dvv,ncols,0);
 let nfa=mma_cfg(dkq,dvv,ncols,2);
 let nk2=mma_cfg(dkq,dvv,ncols,3);
 let nv2=mma_cfg(dkq,dvv,ncols,4);
 let ncb=mma_cfg(dkq,dvv,ncols,5);
 let nstt=mma_cfg(dkq,dvv,ncols,6);
 let qir=mma_cfg(dkq,dvv,ncols,7);
 let nst:i64=if cpasync(d,cc)&&c2>=2{nstt}else{0};
 let swz:bool=tmma(d,cc)&&nk2%32==0&&nv2%32==0;
 let cpw:i64=if tmma(d,cc)||wmma(cc)||mfma(cc){if ncols<16{ncols}else{16}}else{if ncols<32{ncols}else{32}};
 let wsh:i64=dev(d,4);ensure(wsh>0&&ntt%wsh==0,33);
 let nwarps:u64=itg(ntt/wsh);
 let stk:u64=if swz{itg(nk2)}else{itg(nk2+4)};
 let stv:u64=if dkq==576{stk}else{if swz{itg(nv2)}else{itg(nv2+4)}};
 let kv:u64=if nst<=1{mul(itg(nfa),mxu(stk,stv))*4}else{mul(itg(nfa),stk+stv)*4};
 let smq:u64=mul(itg(ncols*(dkq/2+4)),4);
 let smm:u64=mul(itg(c1*(nfa/2+4)),4);
 let smc:u64=mul(mul(nwarps,itg(cpw)),itg(ncb+4))*4;
 let smt:u64=mxu(smc,if qir==1{mxu(smq,kv+smm)}else{smq+kv+smm});
 let sb:i64=if soft!=0.0{1}else{0};let sp:i64=if sparse{1}else{0};
 ensure(dkq==256&&dvv==256&&c2==8&&(c1==4||c1==8)&&soft==0.0&&!sparse,56);
 let inst:i64=if c1==4{1}else{2};put(p,3,34,itg(inst));put(p,3,36,itg(inst+1));
 var init_status:i64=0;init_lock(d,inst);
 if memo(d,inst)==0{put(p,3,9,smt);init_status=setattr(p);if init_status==0{memo_set(d,inst,1);}}
 init_unlock(d,inst);ensure(init_status==0,30);
 let apre:bool=nst==2&&!sparse;
 put(p,3,1,400 as u64);put(p,3,22,if soft!=0.0{1 as u64}else{0 as u64});
 launch_fattn(packet,d,dst,p,itg(c1),itg(c2),itg(dvv),true,true,true,sparse,nwarps,smt,itg(nfa),itg(wsh),apre);
 return true;
}
// packet args: 0 role (0 alloc size, 1 supported, 2 dispatch+launch), 1 device, 2 dst tensor
fn run(packet:i64){
 let role=arg(packet,0);let d=arg(packet,1);let dst=arg(packet,2);
 ensure(role>=0&&role<=2&&dst!=0&&d>=0&&d<ndev(),27);
 ensure(ti(dst,5,0)==oc(0),28);
 let p=plan_ptr(packet);
 if !apex_scope(d,dst){put(p,3,28,1 as u64);return;}
 validate_apex(dst,role==2,p);
 if role==2{setdev(d);}
 let best=best_kernel(d,dst);
 put(p,3,1,itg(best));put(p,3,32,itg(d));
 if role==0{alloc_size(dst,best,p);return;}
 let cc=dev(d,0);
 let defer:bool=!(best==100||(best==400&&amma(d,cc)));
 if role==1{put(p,3,29,if best!=0{1 as u64}else{0 as u64});put(p,3,28,if defer{1 as u64}else{0 as u64});return;}
 if best==0{fail(29);}
 var done:bool=false;
 if best==100{done=vec_plan(packet,d,dst,p);}
 if best==400{done=mma_plan(packet,d,dst,p);}
 if !done{put(p,3,28,1 as u64);}
}
fn main(){run(0);}