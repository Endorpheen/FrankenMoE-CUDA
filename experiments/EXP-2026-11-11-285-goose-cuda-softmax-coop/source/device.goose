// Apex CUDA cooperative softmax in Goose, from ggml-cuda/softmax.cu:
// soft_max_f32_parallelize_cols and its single-row body.
//
// This branch is taken by the native only when the shared-memory softmax does
// not fit, cooperative launch is supported, ncols/rows > 8192, and there is no
// mask, no sinks, scale == 1 and max_bias == 0.
//
// Everything algorithmic is Goose: the 4-values-per-thread grid-strided passes,
// block max/sum reduces, the CTA-level result exchange through global memory and
// the grid barrier between the two aggregations. Shared memory, __syncthreads,
// shuffles and the grid barrier are explicit primitives.
extern "fg285d_ld_f32" fn ld_f32(x:u64,o:u64)->f32;
extern "fg285d_st_f32" fn st_f32(dst:u64,o:u64,v:f32);
extern "fg285d_smem" fn smem_base()->u64;
extern "fg285d_sload" fn sload(a:u64)->f32;
extern "fg285d_sstore" fn sstore(a:u64,v:f32);
extern "fg285d_sync" fn sync();
extern "fg285d_gridsync" fn gridsync();
extern "fg285d_shfl_xor" fn shfl_xor(v:f32,m:i32)->f32;
extern "fg285d_fmaxf" fn fmaxf(a:f32,b:f32)->f32;
extern "fg285d_expf" fn expf(x:f32)->f32;
extern "fg285d_ninf" fn ninf()->f32;
extern "fg285d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn warp_max(x:f32)->f32{
 var v:f32=x;
 var off:i64=16 as i64;
 while off>=1 as i64{ let t:f32=shfl_xor(v,off as i32); v=fmaxf(v,t); off=off/2 as i64; }
 v
}
fn warp_sum(x:f32)->f32{
 var v:f32=x;
 var off:i64=16 as i64;
 while off>=1 as i64{ let t:f32=shfl_xor(v,off as i32); v=v+t; off=off/2 as i64; }
 v
}
fn breduce_max(v0:f32,blk:i64,tid:i64,buf:u64)->f32{
 var v:f32=warp_max(v0);
 if blk>32 as i64{
  if tid%32 as i64==0 as i64{ sstore(buf+((tid/32*4 as i64) as u64),v); }
  sync();
  var s:f32=ninf();
  if tid%32 as i64<(blk/32 as i64){ s=sload(buf+((tid%32*4 as i64) as u64)); }
  v=warp_max(s);
 }
 v
}
fn breduce_sum(v0:f32,blk:i64,tid:i64,buf:u64)->f32{
 var v:f32=warp_sum(v0);
 if blk>32 as i64{
  if tid%32 as i64==0 as i64{ sstore(buf+((tid/32*4 as i64) as u64),v); }
  sync();
  var s:f32=0.0 as f32;
  if tid%32 as i64<(blk/32 as i64){ s=sload(buf+((tid%32*4 as i64) as u64)); }
  v=warp_sum(s);
 }
 v
}

fn coop(x:u64,dst:u64,tmpmax:u64,tmpsum:u64,ncols:i64,nrows:i64,scale:f32,
        blk:i64,tid:i64,bx:i64,gx:i64){
 ensure(ncols>0 as i64&&nrows>0 as i64&&blk>0 as i64&&gx>0 as i64,20);
 let sbase:u64=smem_base();
 let smax:u64=sbase;
 let ssum:u64=sbase+((32 as i64*4 as i64) as u64);
 let step:i64=gx*blk;
 var row:i64=0 as i64;
 while row<nrows as i64{
  let xr:u64=x+(((row*ncols)*4 as i64) as u64);
  let dr:u64=dst+(((row*ncols)*4 as i64) as u64);
  var lm:f32=ninf();
  var c:i64=bx*blk+tid;
  while c<ncols as i64{
   var v0:f32=ninf();
   var v1:f32=ninf();
   var v2:f32=ninf();
   var v3:f32=ninf();
   let i0:i64=c;
   let i1:i64=c+step;
   let i2:i64=c+2*step as i64;
   let i3:i64=c+3*step as i64;
   if i0<ncols as i64{ v0=ld_f32(xr,((i0*4 as i64) as u64)); }
   if i1<ncols as i64{ v1=ld_f32(xr,((i1*4 as i64) as u64)); }
   if i2<ncols as i64{ v2=ld_f32(xr,((i2*4 as i64) as u64)); }
   if i3<ncols as i64{ v3=ld_f32(xr,((i3*4 as i64) as u64)); }
   lm=fmaxf(lm,v0);
   lm=fmaxf(lm,v1);
   lm=fmaxf(lm,v2);
   lm=fmaxf(lm,v3);
   c=c+step*4 as i64;
  }
  lm=breduce_max(lm,blk,tid,smax);
  if tid==0 as i64{ st_f32(tmpmax,((bx*4 as i64) as u64),lm); }
  gridsync();
  var gm:f32=ninf();
  if tid<gx as i64{ gm=ld_f32(tmpmax,((tid*4 as i64) as u64)); }
  gm=breduce_max(gm,blk,tid,smax);
  var se:f32=0.0 as f32;
  c=bx*blk+tid;
  while c<ncols as i64{
   var v0:f32=ninf();
   var v1:f32=ninf();
   var v2:f32=ninf();
   var v3:f32=ninf();
   let i0:i64=c;
   let i1:i64=c+step;
   let i2:i64=c+2*step as i64;
   let i3:i64=c+3*step as i64;
   if i0<ncols as i64{ v0=ld_f32(xr,((i0*4 as i64) as u64)); }
   if i1<ncols as i64{ v1=ld_f32(xr,((i1*4 as i64) as u64)); }
   if i2<ncols as i64{ v2=ld_f32(xr,((i2*4 as i64) as u64)); }
   if i3<ncols as i64{ v3=ld_f32(xr,((i3*4 as i64) as u64)); }
   if i0<ncols as i64{ let t:f32=expf(v0-gm); se=se+t; st_f32(dr,((i0*4 as i64) as u64),t); }
   if i1<ncols as i64{ let t:f32=expf(v1-gm); se=se+t; st_f32(dr,((i1*4 as i64) as u64),t); }
   if i2<ncols as i64{ let t:f32=expf(v2-gm); se=se+t; st_f32(dr,((i2*4 as i64) as u64),t); }
   if i3<ncols as i64{ let t:f32=expf(v3-gm); se=se+t; st_f32(dr,((i3*4 as i64) as u64),t); }
   c=c+step*4 as i64;
  }
  se=breduce_sum(se,blk,tid,ssum);
  if tid==0 as i64{ st_f32(tmpsum,((bx*4 as i64) as u64),se); }
  gridsync();
  var ts:f32=0.0 as f32;
  if tid<gx as i64{ ts=ld_f32(tmpsum,((tid*4 as i64) as u64)); }
  ts=breduce_sum(ts,blk,tid,ssum);
  c=bx*blk+tid;
  while c<ncols as i64{
   var v0:f32=ninf();
   var v1:f32=ninf();
   var v2:f32=ninf();
   var v3:f32=ninf();
   let i0:i64=c;
   let i1:i64=c+step;
   let i2:i64=c+2*step as i64;
   let i3:i64=c+3*step as i64;
   if i0<ncols as i64{ v0=ld_f32(dr,((i0*4 as i64) as u64)); }
   if i1<ncols as i64{ v1=ld_f32(dr,((i1*4 as i64) as u64)); }
   if i2<ncols as i64{ v2=ld_f32(dr,((i2*4 as i64) as u64)); }
   if i3<ncols as i64{ v3=ld_f32(dr,((i3*4 as i64) as u64)); }
   if i0<ncols as i64{ st_f32(dr,((i0*4 as i64) as u64),v0/ts); }
   if i1<ncols as i64{ st_f32(dr,((i1*4 as i64) as u64),v1/ts); }
   if i2<ncols as i64{ st_f32(dr,((i2*4 as i64) as u64),v2/ts); }
   if i3<ncols as i64{ st_f32(dr,((i3*4 as i64) as u64),v3/ts); }
   c=c+step*4 as i64;
  }
  row=row+1 as i64;
 }
}

// Retained entry; the parent never executes main.
fn main(){coop(0,0,0,0,1 as i64,1 as i64,1.0 as f32,1 as i64,0 as i64,0 as i64,1 as i64);}
