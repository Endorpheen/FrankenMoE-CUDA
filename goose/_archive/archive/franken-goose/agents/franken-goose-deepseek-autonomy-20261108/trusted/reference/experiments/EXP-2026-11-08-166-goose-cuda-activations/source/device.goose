// Device computation;foreign calls are individual CUDA math/storage/PDL effects.
extern "fg166_di" fn di(p:i64,k:i64)->i64;
extern "fg166_df" fn df(p:i64,k:i64)->f32;
extern "fg166_load" fn load(kind:i64,p:i64,byteoff:i64)->f32;
extern "fg166_store" fn store(kind:i64,p:i64,byteoff:i64,value:f32);
extern "fg166_pdl_lc" fn pdl_lc();
extern "fg166_pdl_sync" fn pdl_sync();
extern "fmaf" fn fma32(x:f32,y:f32,z:f32)->f32;
extern "expf" fn exp32(x:f32)->f32;
extern "expm1f" fn expm132(x:f32)->f32;
extern "logf" fn log32(x:f32)->f32;
extern "tanhf" fn tanh32(x:f32)->f32;
extern "erff" fn erf32(x:f32)->f32;
extern "fabsf" fn abs32(x:f32)->f32;
extern "fminf" fn min32(x:f32,y:f32)->f32;
extern "fmaxf" fn max32(x:f32,y:f32)->f32;
extern "sqrtf" fn sqrt32(x:f32)->f32;
extern "sinf" fn sin32(x:f32)->f32;
extern "cosf" fn cos32(x:f32)->f32;
extern "floorf" fn floor32(x:f32)->f32;
extern "ceilf" fn ceil32(x:f32)->f32;
extern "roundf" fn round32(x:f32)->f32;
extern "trunc" fn trunc64(x:f64)->f64;
fn unary(op:i64,x:f32)->f32{
 if op==0{return abs32(x);}if op==1{return if x>0.0{1.0 as f32}else{if x<0.0{-1.0 as f32}else{0.0 as f32}};}if op==2{return -x;}if op==3{return if x>0.0{1.0 as f32}else{0.0 as f32};}
 if op==4{return 0.5*x*(1.0+tanh32((0.79788456080286535587989211986876 as f32)*x*(1.0+(0.044715 as f32)*x*x)));}
 if op==5{return 0.5*x*(1.0+erf32(x*(0.70710678118654752440084436210484 as f32)));}
 if op==6{return x*(1.0/(1.0+exp32((-1.702 as f32)*x)));}
 if op==7{return x/(1.0+exp32(-x));}if op==8{return tanh32(x);}if op==9{return max32(x,0.0);}
 if op==10{return 1.0/(1.0+exp32(-x));}if op==11{return min32(1.0,max32(0.0,(x+3.0)/6.0));}if op==12{return x*min32(1.0,max32(0.0,(x+3.0)/6.0));}
 if op==13{return exp32(x);}if op==14{return x*x;}if op==15{return sqrt32(x);}if op==16{return sin32(x);}if op==17{return cos32(x);}if op==18{return log32(x);}
 if op==19{return if x>0.0{x}else{expm132(x)};}if op==20{return floor32(x);}if op==21{return ceil32(x);}if op==22{return round32(x);}if op==23{return trunc64(x as f64) as f32;}if op==24{return expm132(x);}if op==25{return if x>20.0{x}else{log32(1.0+exp32(x))};}let r=max32(x,0.0);return r*r;
}
fn row_offset(p:i64,i:i64,dst:bool)->i64{let n0=di(p,7);let n1=di(p,8);let n2=di(p,9);let i0=i%n0;let q=i/n0;let i1=q%n1;let r=q/n1;let i2=r%n2;let i3=r/n2;if dst{return i0*di(p,17)+i1*di(p,14)+i2*di(p,15)+i3*di(p,16);}return i0*di(p,10)+i1*di(p,11)+i2*di(p,12)+i3*di(p,13);}
fn lane(p:i64,mode:i64,op:i64,kind:i64,tid:i64,stride:i64){let x=di(p,0);let g=di(p,1);let d=di(p,2);let k=di(p,3);let ts=di(p,17);
 if mode==7{pdl_lc();pdl_sync();var i=tid;while i<k{store(kind,d,i*ts,df(p,0)*load(kind,x,i*ts)+df(p,1));i+=stride;}return;}
 if mode<=2{pdl_lc();}if tid>=k{return;}var off=tid*ts;var goff=off;var dout=off;
 if mode==1{off=row_offset(p,tid,false);}if mode==2||mode==3||mode==4{let n=di(p,4);let o0=di(p,5);let o1=di(p,6);let j0=(tid/n)*o0+(tid%n);let j1=if o0==o1{j0}else{(tid/n)*o1+(tid%n)};off=j0*ts;goff=j1*ts;}
 if mode==8{off=row_offset(p,tid,false);dout=row_offset(p,tid,true);}if mode<=2{pdl_sync();}let v=load(kind,x,off);var value:f32=0.0;
 if mode==0||mode==1{value=unary(op,v);}if mode==2{value=unary(op,v)*load(kind,g,goff);}
 if mode==3{let a=min32(v,df(p,5));let b=max32(min32(load(kind,g,goff),df(p,5)),-df(p,5));value=(a/(1.0+exp32(-a*df(p,4))))*(1.0+b);}
 if mode==4{let a=min32(v,df(p,5));let b=max32(min32(load(kind,g,goff),df(p,5)),-df(p,5));value=unary(7,a)*b;}
 if mode==5{let gate=if v>0.0{1.0 as f32}else{0.0 as f32};let pos=fma32(v,df(p,2),(df(p,1)*v)*v);let neg=fma32(v,df(p,2),(expm132(min32(v,df(p,3)))-v)*df(p,0));value=fma32(pos,gate,(1.0-gate)*neg);}
 if mode==6{let z=load(kind,g,goff);let sig=1.0/(1.0+exp32(-z));value=v*sig*(1.0+z*(1.0-sig));}
 if mode==8{value=min32(max32(v,df(p,4)),df(p,5));}if mode==9{value=max32(v,0.0)+min32(v,0.0)*df(p,6);}store(kind,d,dout,value);
}
fn main(){lane(0,0,0,0,0,1);print(unary(7,1.0));}
