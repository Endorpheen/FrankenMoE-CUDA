// Full normalization validation,ownership-free tensor projection and launch selection.
extern "fg168_arg" fn arg(packet:i64,k:i64)->i64;
extern "fg168_plan_ptr" fn plan_ptr(packet:i64)->i64;
extern "fg168_hget" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg168_hput" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg168_hfloat" fn put_float(p:i64,i:i64,v:f32);
extern "fg168_ti" fn ti(t:i64,k:i64,i:i64)->i64;
extern "fg168_tf" fn tf(t:i64,i:i64)->f32;
extern "fg168_contiguous" fn contiguous(t:i64)->bool;
extern "fg168_f32_type" fn f32_type()->i64;
extern "fg168_launch" fn launch(packet:i64);
extern "fg168_fail" fn fail(code:i64);
fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn U()->u64{return 18446744073709551615 as u64;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,1);return a*b;}
fn add(a:u64,b:u64)->u64{ensure(a<=U()-b,26);return a+b;}
fn ceil(a:u64,b:u64)->u64{ensure(b!=0,2);return a/b+if a%b!=0{1 as u64}else{0 as u64};}
fn minimum(a:u64,b:u64)->u64{return if a<b{a}else{b};}
fn magic(p:i64,key:i64,d:u64){ensure(d>0&&d<=4294967295,3);var l:u64=0;while l<32&&((1 as u64)<<l)<d{l+=1;}let mp=((1 as u64)<<32)*(((1 as u64)<<l)-d)/d+1;ensure(mp<=4294967295,4);put(p,4,key*3,mp);put(p,4,key*3+1,l);put(p,4,key*3+2,d);}
fn copy_tensor(p:i64,t:i64,which:i64){ensure(t!=0&&ti(t,2,0)==f32_type()&&ti(t,1,0)!=0&&ti(t,4,0)==4,5);put(p,0,which,ti(t,1,0) as u64);var i:i64=0;var ext:u64=4;while i<4{let n=ti(t,3,i);let stride=ti(t,4,i);ensure(n>0&&n<=2147483647&&stride>=0&&(stride as u64)%4==0,6);put(p,1,which*4+i,n as u64);put(p,2,which*4+i,(stride as u64)/4);ext=add(ext,mul((n-1) as u64,stride as u64));i+=1;}let span=ti(t,9,0);ensure(span>0&&ext<=(span as u64),25);}
fn aligned(t:i64)->bool{if (ti(t,1,0) as u64)%16!=0{return false;}var i:i64=1;while i<4{if (ti(t,4,i) as u64)%16!=0{return false;}i+=1;}return true;}
fn other(t:i64,source:i64)->i64{ensure(t!=0,7);if ti(t,0,0)==source{return ti(t,0,1);}ensure(ti(t,0,1)==source,8);return ti(t,0,0);}
fn run(packet:i64){let mode=arg(packet,0);let p=plan_ptr(packet);let node=arg(packet,2);let second=arg(packet,3);let third=arg(packet,4);ensure(mode>=0&&mode<8&&node!=0&&arg(packet,1)!=0,9);ensure(ti(node,2,0)==f32_type(),10);var x=ti(node,0,0);var d=node;var m:i64=0;var a:i64=0;var grad:i64=0;if mode==3||mode==5{m=other(second,node);ensure(m!=0,20);d=second;}if mode==5{a=other(third,second);ensure(a!=0,21);d=third;}if mode==4{ensure(second!=0&&ti(second,0,0)==node&&tf(second,1)==0.0,11);d=second;put_float(p,1,tf(second,0));}if mode==6{grad=x;x=ti(node,0,1);}copy_tensor(p,x,0);copy_tensor(p,d,1);var i:i64=0;while i<4{ensure(get(p,1,i)==get(p,1,4+i),12);i+=1;}if mode==3||mode==4||mode==5{i=0;while i<4{ensure(ti(node,3,i) as u64==get(p,1,i),22);i+=1;}}if mode==5{ensure(ti(second,2,0)==f32_type(),23);i=0;while i<4{ensure(ti(second,3,i) as u64==get(p,1,i),24);i+=1;}}if mode==6{copy_tensor(p,grad,4);ensure(contiguous(grad),13);i=0;while i<4{ensure(get(p,1,16+i)==get(p,1,i),14);i+=1;}}
 let eps=tf(node,if mode==1{1}else{0});ensure(eps>=0.0,15);put_float(p,0,eps);let n=get(p,1,0);let rows=get(p,1,1);let channels=get(p,1,2);let samples=get(p,1,3);put(p,3,0,mode as u64);put(p,3,1,n);put(p,3,2,rows);put(p,3,3,channels);put(p,3,4,samples);put(p,3,11,rows);put(p,3,12,minimum(channels,65535 as u64));put(p,3,13,minimum(samples,65535 as u64));var bs:u64=if n<1024{32 as u64}else{1024 as u64};var shared:u64=if bs>32{if mode==0||mode==6{256 as u64}else{128 as u64}}else{0 as u64};
 if mode==1{ensure(contiguous(x)&&contiguous(d),16);let groups=ti(node,8,0);ensure(groups>0,17);let size=mul(mul(n,rows),ceil(channels,groups as u64));let total=mul(mul(mul(n,rows),channels),samples);let ng=mul(groups as u64,samples);ensure(size<=2147483647&&total<=2147483647&&ng<=2147483647,18);bs=if size<1024{32 as u64}else{1024 as u64};shared=if bs>32{256 as u64}else{0 as u64};put(p,3,15,size);put(p,3,16,total);put(p,3,17,ng);put(p,3,11,ng);put(p,3,12,1 as u64);put(p,3,13,1 as u64);}
 if mode==6{let nr=mul(mul(rows,channels),samples);ensure(nr<=2147483647,19);put(p,3,11,nr);put(p,3,12,1 as u64);put(p,3,13,1 as u64);}
 if mode>=2&&mode<=5{if m!=0{copy_tensor(p,m,2);put(p,3,8,1 as u64);i=0;while i<4{magic(p,i,get(p,1,8+i));i+=1;}}if a!=0{copy_tensor(p,a,3);put(p,3,9,1 as u64);i=0;while i<4{magic(p,4+i,get(p,1,12+i));i+=1;}}if mode==4{put(p,3,10,1 as u64);}var vec=n%4==0&&aligned(x)&&aligned(d);if m!=0{vec=vec&&get(p,1,8)==n&&aligned(m);}if a!=0{vec=vec&&get(p,1,12)==n&&aligned(a);}var vpt:u64=1;if vec{let n4=n/4;if n4<=32{bs=32;}else{if n4<=128{bs=128;}else{if n4<=256{bs=256;}else{if n4<=512{bs=512;}else{bs=1024;if n4>1024&&n4<=4096{vpt=4;}if n4>4096{vec=false;}}}}}}else{bs=if n<1024{256 as u64}else{1024 as u64};}shared=if bs>32{128 as u64}else{0 as u64};put(p,3,6,vpt);put(p,3,7,if vec{1 as u64}else{0 as u64});}
 put(p,3,5,bs);put(p,3,14,shared);launch(packet);
}
fn main(){run(0);}
