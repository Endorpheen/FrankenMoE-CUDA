// EXP170s SSM convolution device paths;imports are raw F32 access,expf ISA,barrier and PDL effects.
// Mirrors ssm-conv.cu ssm_conv_f32 (serial rolling window) and ssm_conv_long_token_f32 (tiled shared)
// for threads=128,all native d_conv cases in {3,4,5,7,9,15},optional fused bias and SiLU.
// Ring indexing,shared load schedule,SiLU formula and accumulation order run here;no original
// C++ numerical helper is called. Preconditions (d_conv set,nr%128==0,window coverage,grid caps,
// i32 offset domain) are enforced by policy.goose before any launch;u64 byte offsets are address
// identical to native on every policy-accepted shape. src0_nb0 is accepted but unused in the
// serial entry exactly like native GGML_UNUSED;long entry uses it for the bidz token offset.
extern "fg170s_load" fn load(p:u64,i:u64)->f32;
extern "fg170s_store" fn store(p:u64,i:u64,v:f32);
extern "fg170s_expf" fn expf(x:f32)->f32;
extern "fg170s_barrier" fn barrier();
extern "fg170s_lc" fn lc();
extern "fg170s_sync" fn sync();
fn silu(x:f32)->f32{return x/(1.0+expf(-x));}
fn ssm_conv_serial(src0:u64,src1:u64,bias:u64,dst:u64,src0_nb0:i32,src0_nb1:i32,src0_nb2:i32,src1_nb1:i32,dst_nb0:i32,dst_nb1:i32,dst_nb2:i32,n_t:i32,d_conv:i32,apply_silu:bool,use_bias:bool,tid:i32,bidx:i32,bidy:i32){
 lc();
 let xb=src0+(bidx as u64)*(src0_nb2 as u64)+(bidy as u64)*(128 as u64)*(src0_nb1 as u64);
 let wb=src1+(bidy as u64)*(128 as u64)*(src1_nb1 as u64);
 let yb=dst+(bidx as u64)*(dst_nb2 as u64)+(bidy as u64)*(128 as u64)*(dst_nb0 as u64);
 let sx=(src0_nb1 as u64)/4;
 let sw=(src1_nb1 as u64)/4;
 let sy=(dst_nb1 as u64)/4;
 var x:f32[5]=default<f32[5]>();
 var w:f32[5]=default<f32[5]>();
 sync();
 var j:i32=0;
 while j<d_conv{w[j]=load(wb,(tid as u64)*sw+(j as u64));j+=1;}
 var b:f32=0.0;
 if use_bias{b=load(bias,(bidy as u64)*(128 as u64)+(tid as u64));}
 var i:i32=0;
 while i<n_t{
  var sumf:f32=0.0;
  if i==0{j=0;while j<d_conv{x[j]=load(xb,(tid as u64)*sx+(j as u64));j+=1;}}
  else{x[(i-1)%d_conv]=load(xb,(tid as u64)*sx+((i+d_conv-1) as u64));}
  j=0;
  while j<d_conv{sumf+=x[(i+j)%d_conv]*w[j];j+=1;}
  sumf+=b;
  let out=if apply_silu{silu(sumf)}else{sumf};
  store(yb,(i as u64)*sy+(tid as u64),out);
  i+=1;
 }
}
fn ssm_conv_long(src0:u64,src1:u64,bias:u64,dst:u64,smem:u64,src0_nb0:i32,src0_nb1:i32,src0_nb2:i32,src1_nb1:i32,dst_nb0:i32,dst_nb1:i32,dst_nb2:i32,n_t:i32,d_conv:i32,apply_silu:bool,use_bias:bool,bounded_tail:bool,tid:i32,bidx:i32,bidy:i32,bidz:i32){
 let xb=src0+(bidx as u64)*(src0_nb2 as u64)+(bidy as u64)*(128 as u64)*(src0_nb1 as u64)+(bidz as u64)*(32 as u64)*(src0_nb0 as u64);
 let wb=src1+(bidy as u64)*(128 as u64)*(src1_nb1 as u64);
 let yb=dst+(bidx as u64)*(dst_nb2 as u64)+(bidz as u64)*(32 as u64)*(dst_nb1 as u64)+(bidy as u64)*(128 as u64)*(dst_nb0 as u64);
 let sx=(src0_nb1 as u64)/4;
 let sw=(src1_nb1 as u64)/4;
 let sy=(dst_nb1 as u64)/4;
 let local_n_t=if n_t-bidz*32<32{n_t-bidz*32}else{32};
 let ncols=d_conv-1+32;
 let total=128*ncols;
 let cut=local_n_t+d_conv-1;
 var row=tid/ncols;
 var col=tid%ncols;
 var idx:i32=0;
 while idx<total{
  if row<128{
   if bounded_tail{
    if col<cut{store(smem,(row as u64)*(ncols as u64)+(col as u64),load(xb,(row as u64)*sx+(col as u64)));}
   }else{
    store(smem,(row as u64)*(ncols as u64)+(col as u64),load(xb,(row as u64)*sx+(col as u64)));
   }
  }
  col+=128;row+=col/ncols;col=col%ncols;
  idx+=128;
 }
 barrier();
 var w:f32[5]=default<f32[5]>();
 var j:i32=0;
 while j<d_conv{w[j]=load(wb,(tid as u64)*sw+(j as u64));j+=1;}
 var b:f32=0.0;
 if use_bias{b=load(bias,(bidy as u64)*(128 as u64)+(tid as u64));}
 var i:i32=0;
 while i<local_n_t{
  var sumf:f32=0.0;
  j=0;
  while j<d_conv{sumf+=load(smem,(tid as u64)*(ncols as u64)+((i+j) as u64))*w[j];j+=1;}
  sumf+=b;
  let out=if apply_silu{silu(sumf)}else{sumf};
  store(yb,(i as u64)*sy+(tid as u64),out);
  i+=1;
 }
}
fn main(){ssm_conv_serial(0 as u64,0 as u64,0 as u64,0 as u64,4,4,4,4,4,4,4,0,3,false,false,0,0,0);ssm_conv_long(0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,4,4,4,4,4,4,4,0,3,false,false,false,0,0,0,0);}
