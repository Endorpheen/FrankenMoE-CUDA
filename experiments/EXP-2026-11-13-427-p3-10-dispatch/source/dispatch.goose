// Dense CUDA dispatch predicates in Goose.
//
// Ports ggml_cuda_should_use_mmf and ggml_cuda_should_use_mmvf from
// ggml-cuda/mmf.cu and mmvf.cu. Device facts arrive as booleans computed by the
// caller from cc; type facts arrive as ty (0 F32, 1 F16, 2 BF16, 3 other),
// ts (ggml_type_size) and quantized. Every comparison and branch is Goose.

fn mmf(quant:i64,ty:i64,ts:i64,
       ne0:i64,ne1:i64,ne2:i64,ne3:i64,
       nb0:i64,nb1:i64,nb2:i64,nb3:i64,ne11:i64,
       nvidia:i64,amd:i64,ampere:i64,volta:i64,turing:i64,
       wmma:i64,mfma:i64,cdna1:i64,cdna2:i64,cdna3:i64,rdna3_0:i64,
       rpb:i64,warp:i64,ids:i64)->bool{
 if quant!=(0 as i64){ return false; }
 if ts<=(0 as i64){ return false; }
 let group:i64=warp*((4 as i64)/ts);
 if group<=(0 as i64){ return false; }
 if ne0%group!=(0 as i64){ return false; }
 if nb0!=ts{ return false; }
 if nb1%((2 as i64)*ts)!=(0 as i64){ return false; }
 if nb2%((2 as i64)*ts)!=(0 as i64){ return false; }
 if nb3%((2 as i64)*ts)!=(0 as i64){ return false; }
 if ne1%rpb!=(0 as i64){ return false; }
 if cdna3!=(0 as i64)&&ty==(2 as i64){ return false; }
 if ids!=(0 as i64){
  if ne1<=(1024 as i64)&&ne11>(512 as i64){ return false; }
  if ne1>(1024 as i64)&&ne11>(128 as i64){ return false; }
 } else {
  if rdna3_0!=(0 as i64)&&ne11>(8 as i64){ return false; }
  if cdna2!=(0 as i64)&&(ty==(1 as i64)||ty==(2 as i64)){ return false; }
  if cdna1!=(0 as i64)&&(ty==(1 as i64)||ty==(2 as i64)){ return false; }
  if ne11>(16 as i64){ return false; }
 }
 if ty==(0 as i64){ return ampere!=(0 as i64)||mfma!=(0 as i64); }
 if ty==(1 as i64){ return volta!=(0 as i64)||turing!=(0 as i64)||wmma!=(0 as i64)||mfma!=(0 as i64); }
 if ty==(2 as i64){ return ampere!=(0 as i64)||wmma!=(0 as i64)||mfma!=(0 as i64); }
 return false;
}

fn mmvf(quant:i64,ty:i64,ts:i64,
        ne0:i64,ne1:i64,ne2:i64,ne3:i64,
        nb0:i64,nb1:i64,nb2:i64,nb3:i64,ne11:i64,
        nvidia:i64,amd:i64,ampere:i64,volta:i64,turing:i64,ada:i64,
        wmma:i64,mfma:i64,cdna1:i64,cdna2:i64,cdna3:i64,rdna:i64,rdna3:i64,rdna4:i64,
        fp32mma:i64,fp16mma:i64,bf16mma:i64,rpb:i64,warp:i64)->bool{
 if ne0%(2 as i64)!=(0 as i64){ return false; }
 if ts<=(0 as i64){ return false; }
 if nb0!=ts{ return false; }
 if nb1%((2 as i64)*ts)!=(0 as i64){ return false; }
 if nb2%((2 as i64)*ts)!=(0 as i64){ return false; }
 if nb3%((2 as i64)*ts)!=(0 as i64){ return false; }
 var small:i64=0 as i64;
 if ne1<=(512 as i64){ small=1 as i64; }
 if ne2*ne3==(1 as i64){ small=1 as i64; }
 if ty==(0 as i64){
  if nvidia!=(0 as i64){
   if ampere!=(0 as i64){ return ne11<=(3 as i64); }
   if turing!=(0 as i64){ return ne11<=(4 as i64); }
   return ne11<=(3 as i64);
  }
  if amd!=(0 as i64){
   if fp32mma!=(0 as i64){ return ne11<=(3 as i64); }
   return ne11<=(8 as i64);
  }
  return ne11<=(8 as i64);
 }
 if ty==(1 as i64){
  if nvidia!=(0 as i64){
   if small!=(0 as i64)&&mmf(quant,ty,ts,ne0,ne1,ne2,ne3,nb0,nb1,nb2,nb3,ne11,nvidia,amd,ampere,volta,turing,wmma,mfma,cdna1,cdna2,cdna3,0 as i64,rpb,warp,0 as i64)==false{ return ne11<=(8 as i64); }
   if ampere!=(0 as i64){ return small!=(0 as i64)&&ne11==(1 as i64); }
   if ada!=(0 as i64){ return small!=(0 as i64)&&ne11<=(4 as i64); }
   if fp16mma!=(0 as i64){ return small!=(0 as i64)&&ne11<=(3 as i64); }
   return ne11<=(8 as i64);
  }
  if amd!=(0 as i64){
   if rdna!=(0 as i64)&&mmf(quant,ty,ts,ne0,ne1,ne2,ne3,nb0,nb1,nb2,nb3,ne11,nvidia,amd,ampere,volta,turing,wmma,mfma,cdna1,cdna2,cdna3,0 as i64,rpb,warp,0 as i64)==false{ return ne11<=(8 as i64); }
   if fp16mma!=(0 as i64){
    if rdna3!=(0 as i64){ return ne11<=(3 as i64); }
    if rdna4!=(0 as i64){ return ne11<=(5 as i64); }
    return ne11<=(2 as i64);
   }
   return ne11<=(8 as i64);
  }
  return ne11<=(8 as i64);
 }
 if ty==(2 as i64){
  if nvidia!=(0 as i64){
   if small!=(0 as i64)&&mmf(quant,ty,ts,ne0,ne1,ne2,ne3,nb0,nb1,nb2,nb3,ne11,nvidia,amd,ampere,volta,turing,wmma,mfma,cdna1,cdna2,cdna3,0 as i64,rpb,warp,0 as i64)==false{ return ne11<=(8 as i64); }
   if ampere!=(0 as i64){ return small!=(0 as i64)&&ne11==(1 as i64); }
   if ada!=(0 as i64){ return small!=(0 as i64)&&ne11<=(4 as i64); }
   if bf16mma!=(0 as i64){ return small!=(0 as i64)&&ne11<=(3 as i64); }
   return ne11<=(8 as i64);
  }
  if amd!=(0 as i64){
   if rdna!=(0 as i64)&&mmf(quant,ty,ts,ne0,ne1,ne2,ne3,nb0,nb1,nb2,nb3,ne11,nvidia,amd,ampere,volta,turing,wmma,mfma,cdna1,cdna2,cdna3,0 as i64,rpb,warp,0 as i64)==false{ return ne11<=(8 as i64); }
   if bf16mma!=(0 as i64){ return ne11<=(3 as i64); }
   return ne11<=(8 as i64);
  }
  return ne11<=(8 as i64);
 }
 return false;
}

fn main(){
 let a=mmf(0 as i64,0 as i64,4 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,32 as i64,32 as i64,0 as i64);
 let b=mmvf(0 as i64,0 as i64,4 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,32 as i64,32 as i64);
 if a&&b{ return; }
}
