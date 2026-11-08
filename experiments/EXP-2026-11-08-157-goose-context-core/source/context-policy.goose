// Own context lifetime and metadata decisions;imports are typed storage/OS effects.
extern "fg157_arg" fn arg(p:i64,i:i64)->u64;
extern "fg157_slot" fn slot(p:i64,i:i64)->i64;
extern "fg157_constant" fn constant(i:i64)->u64;
extern "fg157_read" fn read(p:i64,k:i64)->u64;
extern "fg157_write" fn write(p:i64,k:i64,v:u64);
extern "fg157_ctx_alloc" fn ctx_alloc()->i64;
extern "fg157_ctx_release" fn ctx_release(p:i64);
extern "fg157_zero_ctx" fn zero_ctx(p:i64);
extern "fg157_malloc" fn raw_malloc(n:u64)->i64;
extern "fg157_calloc" fn raw_calloc(n:u64,s:u64)->i64;
extern "fg157_aligned_try" fn aligned_try(n:u64,error:i64)->i64;
extern "fg157_aligned_release" fn aligned_release(p:i64);
extern "fg157_load_u64" fn load(p:i64)->u64;
extern "fg157_lock" fn lock();
extern "fg157_unlock" fn unlock();
extern "fg157_clock_ready" fn clock_ready()->i64;
extern "fg157_clock_init" fn clock_init();
extern "fg157_clock_mark" fn clock_mark();
extern "fg157_fail" fn fail(code:i64);
extern "fg157_warn" fn warn(code:i64,n:u64);
extern "fg157_tget" fn tget(t:i64,k:i64)->u64;
extern "fg157_tflags" fn tflags(t:i64,f:u64);
extern "fg157_char_get" fn char_get(p:i64,i:i64)->u64;
extern "fg157_char_put" fn char_put(p:i64,i:i64,v:u64);
extern "fg157_type_begin" fn type_begin(i:i64);
extern "fg157_type_put" fn type_put(i:i64,k:i64,v:u64);
extern "fg157_type_byte" fn type_byte(i:i64,p:i64,v:u64);
extern "fg157_type_get" fn type_get(i:i64,k:i64)->u64;
extern "fg157_label_begin" fn label_begin(g:i64,i:i64);
extern "fg157_label_byte" fn label_byte(g:i64,i:i64,p:i64,v:u64);
extern "fg157_label_get" fn label_get(g:i64,i:i64)->i64;
extern "fg157_f64_bits" fn f64_bits(v:f64)->u64;
fn ensure(ok:bool,code:i64){if !ok{fail(code);}}
fn ip(x:u64)->i64{return x as! i64;}
fn umax()->u64{return 18446744073709551615 as u64;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=umax()/a,1);return a*b;}
fn aligned(n:u64,p:i64)->i64{
 if n==0{warn(0,n);return 0;}
 let result=aligned_try(n,slot(p,11));let error=load(slot(p,11));
 if error!=0{warn(if error==constant(18){1}else{if error==constant(19){2}else{3}},n);return 0;}
 return result;
}
fn initialize(p:i64)->i64{
 var size=arg(p,0);let supplied=ip(arg(p,1));let noalloc=arg(p,2);let align=constant(0);
 if size==0{size=align;}
 if supplied==0{ensure(size<=umax()-(align-1),2);size=(size+align-1)/align*align;}
 lock();if clock_ready()==0{clock_init();clock_mark();}unlock();
 let c=ctx_alloc();ensure(c!=0,3);var buffer=supplied;
 if buffer==0{buffer=aligned(size,p);}
 if buffer==0{ctx_release(c);fail(4);return 0;}
 if (buffer as u64)%align!=0{if supplied==0{aligned_release(buffer);}ctx_release(c);fail(5);return 0;}
 zero_ctx(c);write(c,0,size);write(c,1,buffer as u64);write(c,2,if supplied==0{1 as u64}else{0 as u64});write(c,3,noalloc);return c;
}
fn reset(c:i64){if c==0{return;}write(c,4,0 as u64);write(c,5,0 as u64);write(c,6,0 as u64);}
fn release(c:i64){if c==0{return;}if read(c,2)!=0{aligned_release(ip(read(c,1)));}ctx_release(c);}
fn type_valid(i:i64){ensure(i>=0&&(i as u64)<constant(3),10);}
fn label(g:i64,i:i64)->i64{
 var n=constant(4);if g==2{n=constant(5);}if g==3{n=constant(6);}ensure(g>=0&&g<4&&i>=0&&(i as u64)<n,11);return label_get(g,i);
}
fn descriptor(t:i64)->i64{
 let op=ip(tget(t,0));if (op as u64)==constant(8){return label(2,ip(tget(t,1)));}if (op as u64)==constant(9){return label(3,ip(tget(t,1)));}return label(0,op);
}
fn mark(t:i64,which:i64){
 ensure(which>=0&&which<4,12);if which==0||which==2{ensure(tget(t,0)==constant(7),13);}
 if which==3{ensure(tget(t,2)==constant(10)&&tget(t,3)==1&&tget(t,4)==1&&tget(t,5)==1&&tget(t,6)==1,14);}
 let bit=constant(14+which);var cur=t;
 while cur!=0{tflags(cur,tget(cur,7)|bit);if which!=1{return;}cur=ip(tget(cur,8));}
}
fn tensor_meta(t:i64,which:i64)->i64{
 if which==0{return ip(tget(t,9));}
 if which==1{ensure(tget(t,2)==constant(10),15);return ip(tget(t,9));}
 if which==2{return ip(tget(t,10));}
 if which==3{ensure(tget(t,0)==constant(8),16);return ip(tget(t,1));}
 if which==4{ensure(tget(t,0)==constant(9),17);return ip(tget(t,1));}
 if which==5{return if tget(t,8)!=0{1}else{0};}
 return 0;
}
fn set_name(t:i64,s:i64)->i64{
 let dst=ip(tget(t,10));let cap=constant(11) as i64;var i:i64=0;
 while i<cap-1{let ch=char_get(s,i);if ch==0{break;}char_put(dst,i,ch);i+=1;}
 char_put(dst,i,0 as u64);return t;
}
// FG157_REGISTRY_DATA
fn entry(mode:i64,p:i64)->i64{
 if mode==0{return initialize(p);}
 if mode==1{reset(ip(arg(p,0)));return 0;}
 if mode==2{release(ip(arg(p,0)));return 0;}
 if mode==3{let o=ip(read(ip(arg(p,0)),6));if o==0{return 0;}return (read(o,7)+read(o,8)) as! i64;}
 if mode==4{return read(ip(arg(p,0)),3) as! i64;}
 if mode==5{write(ip(arg(p,0)),3,arg(p,1));return 0;}
 if mode==6{return read(ip(arg(p,0)),1) as! i64;}
 if mode==7{return read(ip(arg(p,0)),0) as! i64;}
 if mode==8{return aligned(arg(p,0),p);}
 if mode==9{aligned_release(ip(arg(p,0)));return 0;}
 if mode==10{let n=arg(p,0);if n==0{warn(4,n);return 0;}let x=raw_malloc(n);ensure(x!=0,20);return x;}
 if mode==11{let n=arg(p,0);let s=arg(p,1);if n==0||s==0{warn(5,0 as u64);return 0;}let bytes=mul(n,s);let x=raw_calloc(n,s);ensure(x!=0,21);return x;}
 if mode==12{let i=ip(arg(p,0));type_valid(i);return type_get(i,ip(arg(p,1))) as! i64;}
 if mode==13{return label(0,ip(arg(p,0)));}
 if mode==14{return label(1,ip(arg(p,0)));}
 if mode==15{return label(2,ip(arg(p,0)));}
 if mode==16{return label(3,ip(arg(p,0)));}
 if mode==17{return descriptor(ip(arg(p,0)));}
 if mode==18{let i=ip(arg(p,0));type_valid(i);let value=(type_get(i,2) as f64)/(ip(type_get(i,0)) as f64);return f64_bits(value) as! i64;}
 if mode==19{return ftype_map(ip(arg(p,0)));}
 if mode==20{mark(ip(arg(p,0)),ip(arg(p,1)));return 0;}
 if mode==21{return tensor_meta(ip(arg(p,0)),ip(arg(p,1)));}
 if mode==22{return set_name(ip(arg(p,0)),ip(arg(p,1)));}
 if mode==24{return (constant(12)+constant(13)) as! i64;}
 if mode==27{let i=ip(arg(p,0));return label_get(4,if i>=-2&&i<=1{i+2}else{4});}
 if mode==26{initialize_registry();return 0;}
 return 0;
}
fn main(){print(entry(99,0));}
