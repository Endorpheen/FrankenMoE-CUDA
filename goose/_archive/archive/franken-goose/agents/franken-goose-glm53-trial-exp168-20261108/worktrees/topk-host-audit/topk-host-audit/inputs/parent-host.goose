// Actual topk-moe public-argument projection,validation and eligibility in Goose.
extern "fg169t_arg" fn arg(packet:i64,k:i64)->i64;
extern "fg169t_plan_ptr" fn plan_ptr(packet:i64)->i64;
extern "fg169t_hget" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg169t_hput" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg169t_hfloat" fn put_float(p:i64,i:i64,v:f32);
extern "fg169t_ti" fn ti(t:i64,k:i64,i:i64)->i64;
extern "fg169t_tf" fn tf(t:i64,i:i64)->f32;
extern "fg169t_contiguous" fn contiguous(t:i64)->bool;
extern "fg169t_cfg" fn cfg(packet:i64,k:i64)->i64;
extern "fg169t_constant" fn constant(k:i64)->i64;
extern "fg169t_launch" fn launch(packet:i64);
extern "fg169t_fail" fn fail(code:i64);
fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn U()->u64{return 18446744073709551615 as u64;}
fn add(a:u64,b:u64)->u64{ensure(a<=U()-b,1);return a+b;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,2);return a*b;}
fn supported(n:i64)->bool{return n==1||n==2||n==4||n==8||n==16||n==32||n==64||n==128||n==256||n==288||n==512||n==576;}
fn span(t:i64,kind:i64)->u64{ensure(t!=0&&ti(t,2,0)==kind&&ti(t,1,0)!=0&&ti(t,4,0)==4,3);var bytes:u64=4;var i:i64=0;while i<4{let n=ti(t,3,i);let nb=ti(t,4,i);ensure(n>0&&nb>=0&&(nb as u64)%4==0,4);bytes=add(bytes,mul((n-1) as u64,nb as u64));i+=1;}let ptr=ti(t,1,0) as u64;ensure(bytes<=9223372036854775807&&ptr<=U()-bytes,5);let declared=ti(t,9,0);ensure(declared>0&&bytes<=declared as u64,6);return bytes;}
fn overlaps(a:u64,an:u64,b:u64,bn:u64)->bool{return a<b+bn&&b<a+an;}
fn eligible(packet:i64)->bool{let gate=arg(packet,9);let weights=arg(packet,3);let logits=arg(packet,2);let ids=arg(packet,4);if gate==0||weights==0||logits==0||ids==0{return false;}let nb0=ti(ids,4,0);let nb1=ti(ids,4,1);if nb0<=0||nb1<0{return false;}let e=nb1/nb0;if !supported(e){return false;}if !contiguous(weights)||!contiguous(logits){return false;}let op=ti(gate,6,0);if op==constant(2){let source=ti(gate,0,0);if source==0||!contiguous(source){return false;}if tf(gate,0)!=1.0||tf(gate,1)!=0.0{return false;}if ti(gate,0,1)!=0||ti(gate,0,2)!=0{return false;}}else{if op==constant(3){let u=ti(gate,5,0);if u!=constant(4)&&u!=constant(5){return false;}}}return true;}
fn compute(packet:i64){let p=plan_ptr(packet);let logits=arg(packet,2);let weights=arg(packet,3);let ids=arg(packet,4);let clamp_t=arg(packet,5);let scale_t=arg(packet,6);let bias=arg(packet,7);ensure(arg(packet,1)!=0,7);let f32=constant(0);let i32=constant(1);let xs=span(logits,f32);let ws=span(weights,f32);let ids_span=span(ids,i32);ensure(contiguous(logits)&&contiguous(weights),8);let e=ti(logits,3,0);let rows=ti(logits,3,1);let used=ti(weights,3,1);ensure(supported(e)&&rows>0&&rows<=2147483647&&used>0&&used<=e,9);ensure(ti(ids,4,1)==e*4,10);let xneed=mul(mul(e as u64,rows as u64),4 as u64);let wneed=mul(mul(used as u64,rows as u64),4 as u64);let ineed=mul(add(mul((rows-1) as u64,e as u64),used as u64),4 as u64);ensure(xs>=xneed&&ws>=wneed&&ids_span>=ineed,11);let xp=ti(logits,1,0) as u64;let wp=ti(weights,1,0) as u64;let ip=ti(ids,1,0) as u64;ensure(!overlaps(wp,wneed,ip,ineed),12);let ng=cfg(packet,3);let gu=cfg(packet,4);ensure(ng>=0&&ng<=32&&ng<=e,13);if ng>0{ensure(gu>=1,14);let kept=if gu<ng{gu}else{ng};ensure(used<=kept*(e/ng),15);}let delayed=cfg(packet,2)!=0;ensure(!(clamp_t!=0&&delayed),16);
 var flags:u64=0;if cfg(packet,0)!=0{flags+=1;}if cfg(packet,1)!=0{flags+=2;}if clamp_t!=0{flags+=4;ensure(ti(clamp_t,2,0)==f32,17);put_float(p,0,tf(clamp_t,0));}if delayed{flags+=8;}var scale:f32=1.0;if scale_t!=0{ensure(ti(scale_t,2,0)==f32,18);scale=tf(scale_t,0);}put_float(p,1,scale);if bias!=0{let bs=span(bias,f32);ensure(contiguous(bias)&&bs>=mul(e as u64,4 as u64),19);let bp=ti(bias,1,0) as u64;ensure(!overlaps(bp,bs,wp,wneed)&&!overlaps(bp,bs,ip,ineed),20);flags+=16;put(p,0,3,bp);}put(p,0,0,xp);put(p,0,1,wp);put(p,0,2,ip);put(p,3,0,e as u64);put(p,3,1,rows as u64);put(p,3,2,used as u64);put(p,3,3,flags);put(p,3,4,ng as u64);put(p,3,5,gu as u64);let ept=if e>32{e/32}else{1};put(p,3,6,ept as u64);put(p,3,7,8 as u64);put(p,3,8,(rows as u64)/8+if rows%8!=0{1 as u64}else{0 as u64});launch(packet);
}
fn entry(packet:i64)->bool{let mode=arg(packet,0);if mode==1{return eligible(packet);}ensure(mode==0,21);compute(packet);return true;}
fn main(){entry(0);}
