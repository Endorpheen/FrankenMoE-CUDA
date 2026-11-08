// Goose owns validation,fallthrough ordering,loops and copy routing;imports are effects.
extern "fg163_arg" fn arg(p:i64,i:i64)->u64;
extern "fg163_get" fn get(p:i64,kind:i64,k:i64,i:i64)->u64;
extern "fg163_callback" fn callback(p:i64,kind:i64,k:i64)->u64;
extern "fg163_constant" fn constant(k:i64)->u64;
extern "fg163_io" fn io_call(kind:i64,which:i64,owner:i64,t:i64,data:i64,off:u64,n:u64,copies:u64,st:u64,sd:u64)->i64;
extern "fg163_backend_call" fn backend_call(which:i64,p:i64,a:i64,b:i64,c:i64)->u64;
extern "fg163_device_call" fn device_call(which:i64,p:i64,a:i64,b:u64,c:u64)->u64;
extern "fg163_reg_call" fn reg_call(which:i64,p:i64,a:u64)->u64;
extern "fg163_zero" fn zero(p:i64,n:u64);
extern "fg163_nbytes" fn nbytes(t:i64)->u64;
extern "fg163_is_host" fn is_host(buffer:i64)->i64;
extern "fg163_buffer_copy" fn buffer_copy(src:i64,dst:i64)->i64;
extern "fg163_heap" fn heap(n:u64)->i64;
extern "fg163_drop" fn drop(p:i64);
extern "fg163_fail" fn fail(code:i64);
extern "fg163_mark" fn mark(mode:i64);
fn ip(x:u64)->i64{return x as! i64;}
fn U()->u64{return 18446744073709551615 as u64;}
fn ensure(v:bool,code:i64){if !v{fail(code);}}
fn add(a:u64,b:u64)->u64{ensure(a<=U()-b,1);return a+b;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,2);return a*b;}
fn t(tensor:i64,k:i64,i:i64)->u64{return get(tensor,0,k,i);}
fn owner(tensor:i64)->i64{let view=ip(t(tensor,2,0));return ip(t(if view!=0{view}else{tensor},1,0));}
fn span(tensor:i64,off:u64,n:u64,copies:u64,st:u64){if copies==0{return;}let start=add(off,mul(copies-1,st));let bytes=nbytes(tensor);ensure(start<=bytes&&n<=bytes-start,3);}
fn sync(backend:i64){ensure(backend!=0,4);if callback(backend,1,7)!=0{backend_call(7,backend,0,0,0);}}
fn io_status(status:i64,scratch:i64){if status!=(constant(1) as i64){drop(scratch);fail(5);}}
fn validate_sync(tensor:i64,data:i64,off:u64,n:u64,write:bool)->i64{ensure(tensor!=0,6);let buffer=owner(tensor);ensure(buffer!=0,7);if n==0{return buffer;}ensure(t(tensor,0,0)!=0&&data!=0,8);span(tensor,off,n,1 as u64,0 as u64);ensure(callback(buffer,0,if write{0}else{1})!=0,9);return buffer;}
fn io_sync(tensor:i64,data:i64,off:u64,n:u64,write:bool,scratch:i64){let buffer=validate_sync(tensor,data,off,n,write);if n==0{return;}io_status(io_call(0,if write{0}else{1},buffer,tensor,data,off,n,1 as u64,0 as u64,0 as u64),scratch);}
fn io_async(backend:i64,tensor:i64,data:i64,off:u64,n:u64,write:bool){ensure(backend!=0&&tensor!=0&&t(tensor,0,0)!=0,10);ensure(n==0||data!=0,11);span(tensor,off,n,1 as u64,0 as u64);let which=if write{2}else{3};if callback(backend,1,which)==0{sync(backend);io_sync(tensor,data,off,n,write,0);}else{io_status(io_call(1,which,backend,tensor,data,off,n,1 as u64,0 as u64,0 as u64),0);}}
fn io_2d(backend:i64,tensor:i64,data:i64,off:u64,n:u64,copies:u64,st:u64,sd:u64,write:bool,async:bool){
 ensure(tensor!=0,12);var buffer:i64=0;if async{ensure(backend!=0&&t(tensor,0,0)!=0,13);}else{buffer=owner(tensor);ensure(buffer!=0,14);}let which=if async{if write{4}else{5}}else{if write{2}else{3}};let target=if async{backend}else{buffer};let kind=if async{1}else{0};
 if copies>1&&callback(target,kind,which)!=0{if n==0{return;}ensure(data!=0&&t(tensor,0,0)!=0,15);span(tensor,off,n,copies,st);add(data as u64,mul(copies-1,sd));io_status(io_call(kind,which,target,tensor,data,off,n,copies,st,sd),0);return;}
 if copies==0{return;}if !async&&n==0{return;}if n>0||async{span(tensor,off,n,copies,st);add(data as u64,mul(copies-1,sd));}var i:u64=0;while i<copies{let pointer=ip(add(data as u64,mul(i,sd)));let offset=add(off,mul(i,st));if async{io_async(backend,tensor,pointer,offset,n,write);}else{io_sync(tensor,pointer,offset,n,write,0);}i+=1;}
}
fn layout(a:i64,b:i64)->bool{if t(a,3,0)!=t(b,3,0){return false;}var i:i64=0;while i<4{if t(a,4,i)!=t(b,4,i)||t(a,5,i)!=t(b,5,i){return false;}i+=1;}return true;}
fn copy_sync(src:i64,dst:i64){ensure(src!=0&&dst!=0&&layout(src,dst),16);if src==dst{return;}let sb=ip(t(src,1,0));ensure(sb!=0,17);let bytes=nbytes(src);if is_host(sb)!=0{io_sync(dst,ip(t(src,0,0)),0 as u64,bytes,true,0);return;}let db=ip(t(dst,1,0));ensure(db!=0,17);if is_host(db)!=0{io_sync(src,ip(t(dst,0,0)),0 as u64,bytes,false,0);return;}if buffer_copy(src,dst)!=0{return;}
 // Validate all fixed metadata before taking ownership of staging storage.
 validate_sync(src,ip(t(src,0,0)),0 as u64,bytes,false);validate_sync(dst,ip(t(dst,0,0)),0 as u64,bytes,true);mark(50);let staging=if bytes!=0{heap(bytes)}else{0};ensure(bytes==0||staging!=0,18);io_sync(src,staging,0 as u64,bytes,false,staging);io_sync(dst,staging,0 as u64,bytes,true,staging);drop(staging);
}
fn copy_async(source:i64,dest:i64,src:i64,dst:i64){ensure(src!=0&&dst!=0&&layout(src,dst),19);if src==dst{return;}ensure(dest!=0,20);if callback(dest,1,6)!=0{if backend_call(6,dest,source,src,dst)!=0{return;}}mark(51);sync(source);sync(dest);copy_sync(src,dst);}
fn device(which:i64,p:i64,a:i64,b:u64,c:u64)->u64{if which==12{if p==0||callback(p,2,12)==0{return 0 as u64;}}else{ensure(p!=0,21);if which==7||which==11{if callback(p,2,which)==0{return 0 as u64;}}ensure(callback(p,2,which)!=0,22);}if which==4{ensure(a!=0,23);zero(a,constant(3));}return device_call(which,p,a,b,c);}
fn dispatch(p:i64)->u64{
 let mode=ip(arg(p,0));let a=ip(arg(p,1));let b=ip(arg(p,2));let c=ip(arg(p,3));let d=arg(p,4);let e=arg(p,5);let f=arg(p,6);let g=arg(p,7);let h=arg(p,8);
 if mode==0{return if a!=0{get(a,1,0,0)}else{0 as u64};}
 if mode==1{if a==0{return constant(0);}ensure(callback(a,1,0)!=0,24);return backend_call(0,a,0,0,0);}
 if mode==2{if a==0{return 0 as u64;}ensure(callback(a,1,1)!=0,25);backend_call(1,a,0,0,0);return 0 as u64;}
 if mode==3{ensure(a!=0,26);return device(6,ip(get(a,1,1,0)),0,0 as u64,0 as u64);}
 if mode==4||mode==5{io_async(a,b,c,d,e,mode==4);return 0 as u64;}
 if mode==6||mode==7{io_2d(a,b,c,d,e,f,g,h,mode==6,true);return 0 as u64;}
 if mode==8||mode==9{io_sync(b,c,d,e,mode==8,0);return 0 as u64;}
 if mode==10||mode==11{io_2d(0,b,c,d,e,f,g,h,mode==10,false);return 0 as u64;}
 if mode==12{ensure(b!=0,27);let buffer=owner(b);if e==0{return 0 as u64;}ensure(buffer!=0&&t(b,0,0)!=0&&callback(buffer,0,4)!=0,28);span(b,d,e,1 as u64,0 as u64);io_status(io_call(0,4,buffer,b,c,d,e,1 as u64,0 as u64,0 as u64),0);return 0 as u64;}
 if mode==13{sync(a);return 0 as u64;}
 if mode>=14&&mode<=18{ensure(a!=0,29);if mode==18{ensure(callback(a,1,11)!=0,30);let result=backend_call(11,a,b,0,0);sync(a);return result;}let which=if mode==14{8}else{if mode==15{9}else{if mode==16{10}else{11}}};ensure(callback(a,1,which)!=0,31);return backend_call(which,a,b,0,0);}
 if mode>=19&&mode<=21{ensure(a!=0,32);return device(if mode==19{9}else{if mode==20{10}else{11}},ip(get(a,1,1,0)),b,0 as u64,0 as u64);}
 if mode==22{ensure(a!=0,33);return get(a,1,1,0);}
 if mode==23{copy_sync(a,b);return 0 as u64;}
 if mode==24{copy_async(a,b,c,ip(d));return 0 as u64;}
 if mode==25{return device(12,a,0,0 as u64,0 as u64);}
 if mode==26{if a==0{return 0 as u64;}device(13,ip(get(a,2,0,0)),a,0 as u64,0 as u64);return 0 as u64;}
 if mode==27||mode==29{ensure(a!=0,34);let which=if mode==27{12}else{13};ensure(callback(a,1,which)!=0,35);backend_call(which,a,b,0,0);return 0 as u64;}
 if mode==28{ensure(a!=0,36);device(14,ip(get(a,2,0,0)),a,0 as u64,0 as u64);return 0 as u64;}
 if mode==30{ensure(a!=0,37);if callback(a,1,14)!=0{backend_call(14,a,b,c,0);}return 0 as u64;}
 if mode>=31&&mode<=35{return device(mode-31,a,b,c as u64,d);}
 if mode==36{ensure(a!=0,38);return get(a,3,0,0);}
 if mode>=37&&mode<=43{return device(mode-32,a,b,c as u64,d);}
 if mode>=44&&mode<=47{ensure(a!=0,39);let which=mode-44;if which==3&&callback(a,3,which)==0{return 0 as u64;}ensure(callback(a,3,which)!=0,40);return reg_call(which,a,arg(p,2));}
 fail(99);return 0 as u64;
}
fn main(){print(dispatch(0));}
