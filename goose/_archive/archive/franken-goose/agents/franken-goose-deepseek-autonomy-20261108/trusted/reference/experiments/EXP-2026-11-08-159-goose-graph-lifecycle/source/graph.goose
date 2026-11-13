// Own graph/hash policy;imports project fields or perform raw storage/OS effects.
extern "fg159_arg" fn arg(p:i64,i:i64)->u64;
extern "fg159_constant" fn constant(k:i64)->u64;
extern "fg159_graph" fn gget(g:i64,k:i64)->u64;
extern "fg159_graph_put" fn gput(g:i64,k:i64,v:u64);
extern "fg159_hash" fn hget(h:i64,k:i64)->u64;
extern "fg159_hash_put" fn hput(h:i64,k:i64,v:u64);
extern "fg159_ptr" fn ptr(p:i64,i:u64)->u64;
extern "fg159_ptr_put" fn ptr_put(p:i64,i:u64,v:u64);
extern "fg159_u32" fn word(p:i64,i:u64)->u64;
extern "fg159_u32_put" fn word_put(p:i64,i:u64,v:u64);
extern "fg159_zero" fn zero(p:i64,n:u64);
extern "fg159_alloc" fn allocate(n:u64,clear:i64)->i64;
extern "fg159_free" fn free(p:i64);
extern "fg159_arena" fn arena(c:i64,n:u64)->i64;
extern "fg159_tensor" fn tget(t:i64,k:i64,i:i64)->u64;
extern "fg159_flags" fn flags(t:i64,v:u64);
extern "fg159_name" fn name(t:i64,kind:i64,index:u64);
extern "fg159_equal_name" fn equal_name(t:i64,s:i64)->i64;
extern "fg159_fail" fn fail(code:i64);
fn ip(x:u64)->i64{return x as! i64;}
fn umax()->u64{return 18446744073709551615 as u64;}
fn ensure(v:bool,code:i64){if !v{fail(code);}}
fn add(a:u64,b:u64)->u64{ensure(a<=umax()-b,1);return a+b;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=umax()/a,2);return a*b;}
fn align_up(n:u64,a:u64)->u64{ensure(a!=0&&(a&(a-1))==0,3);return add(n,a-1)/a*a;}
fn prime(i:u64)->u64{
if i==0{return 2 as u64;}
if i==1{return 3 as u64;}
if i==2{return 5 as u64;}
if i==3{return 11 as u64;}
if i==4{return 17 as u64;}
if i==5{return 37 as u64;}
if i==6{return 67 as u64;}
if i==7{return 131 as u64;}
if i==8{return 257 as u64;}
if i==9{return 521 as u64;}
if i==10{return 1031 as u64;}
if i==11{return 2053 as u64;}
if i==12{return 4099 as u64;}
if i==13{return 8209 as u64;}
if i==14{return 16411 as u64;}
if i==15{return 32771 as u64;}
if i==16{return 65537 as u64;}
if i==17{return 131101 as u64;}
if i==18{return 262147 as u64;}
if i==19{return 524309 as u64;}
if i==20{return 1048583 as u64;}
if i==21{return 2097169 as u64;}
if i==22{return 4194319 as u64;}
if i==23{return 8388617 as u64;}
if i==24{return 16777259 as u64;}
if i==25{return 33554467 as u64;}
if i==26{return 67108879 as u64;}
if i==27{return 134217757 as u64;}
if i==28{return 268435459 as u64;}
if i==29{return 536870923 as u64;}
if i==30{return 1073741827 as u64;}
if i==31{return 2147483659 as u64;}
fail(30);return 0 as u64;}
fn hash_size(n:u64)->u64{var lo:u64=0;var hi:u64=32;while lo<hi{let mid=(lo+hi)/2;if prime(mid)<n{lo=mid+1;}else{hi=mid;}}return if lo<32{prime(lo)}else{n|1};}
fn bit_words(n:u64)->u64{return n/32+if n%32!=0{1 as u64}else{0 as u64};}
fn bit(p:i64,i:u64)->bool{return (word(p,i/32)&((1 as u64)<<(i%32)))!=0;}
fn bit_set(p:i64,i:u64,value:bool){let mask=(1 as u64)<<(i%32);let old=word(p,i/32);word_put(p,i/32,if value{old|mask}else{old&~mask});}
fn find(h:i64,key:u64)->u64{
 let size=hget(h,0);ensure(size>0,4);let used=ip(hget(h,1));let keys=ip(hget(h,2));let first=(key>>4)%size;var i=first;
 while bit(used,i)&&ptr(keys,i)!=key{i=(i+1)%size;if i==first{return umax();}}
 return i;
}
fn insert(h:i64,key:u64,existing:bool)->u64{let i=find(h,key);ensure(i!=umax(),5);let used=ip(hget(h,1));if bit(used,i){return if existing{i}else{umax()-1};}bit_set(used,i,true);ptr_put(ip(hget(h,2)),i,key);return i;}
fn reset_hash(h:i64){zero(ip(hget(h,1)),mul(bit_words(hget(h,0)),4 as u64));}
fn new_hash(out:i64,n:u64){
 let size=hash_size(n);let keybytes=mul(size,8 as u64);let bitbytes=mul(bit_words(size),4 as u64);let keys=allocate(keybytes,0);ensure(keys!=0,6);let used=allocate(bitbytes,1);if used==0{free(keys);fail(7);return;}hput(out,0,size);hput(out,1,used as u64);hput(out,2,keys as u64);
}
fn graph_bytes(n:u64,grads:bool)->u64{
 ensure(n<=2147483647,8);let hs=hash_size(mul(n,2 as u64));let tensors=mul(n,8 as u64);let hashptrs=mul(hs,8 as u64);var pos=constant(0);
 pos=add(align_up(pos,8 as u64),tensors);pos=add(align_up(pos,8 as u64),tensors);pos=add(align_up(pos,4 as u64),mul(hs,4 as u64));pos=add(align_up(pos,8 as u64),hashptrs);
 if grads{pos=add(align_up(pos,8 as u64),hashptrs);pos=add(align_up(pos,8 as u64),hashptrs);}return add(align_up(pos,4 as u64),mul(bit_words(hs),4 as u64));
}
fn create(c:i64,n:u64,grads:bool)->i64{
 let bytes=graph_bytes(n,grads);let g=arena(c,bytes);zero(g,constant(0));let base=g as u64;let hs=hash_size(n*2);var pos=constant(0);let tensors=n*8;let hashptrs=hs*8;
 gput(g,0,n);gput(g,3,add(base,align_up(pos,8 as u64)));pos=add(align_up(pos,8 as u64),tensors);gput(g,4,add(base,align_up(pos,8 as u64)));pos=add(align_up(pos,8 as u64),tensors);gput(g,5,add(base,align_up(pos,4 as u64)));pos=add(align_up(pos,4 as u64),hs*4);let h=ip(gget(g,10));hput(h,0,hs);hput(h,2,add(base,align_up(pos,8 as u64)));pos=add(align_up(pos,8 as u64),hashptrs);
 if grads{gput(g,6,add(base,align_up(pos,8 as u64)));pos=add(align_up(pos,8 as u64),hashptrs);gput(g,7,add(base,align_up(pos,8 as u64)));pos=add(align_up(pos,8 as u64),hashptrs);zero(ip(gget(g,6)),hashptrs);zero(ip(gget(g,7)),hashptrs);}
 hput(h,1,add(base,align_up(pos,4 as u64)));pos=add(align_up(pos,4 as u64),bit_words(hs)*4);ensure(pos==bytes,9);reset_hash(h);return g;
}
fn clear(g:i64){gput(g,1,0 as u64);gput(g,2,0 as u64);reset_hash(ip(gget(g,10)));}
fn refused(ok:bool,scratch:i64,code:i64){if !ok{free(scratch);fail(code);}}
fn frame_enter(g:i64,t:i64,compute:bool,stack:i64,depth:u64,capacity:u64){
 refused(depth<capacity,stack,10);let nodeop=tget(t,0,0);if nodeop!=constant(8)&&compute{flags(t,tget(t,1,0)|constant(7));}
 let h=ip(gget(g,10));let slot=find(h,t as u64);refused(slot!=umax(),stack,11);let used=ip(hget(h,1));let first= !bit(used,slot);
 if first{ptr_put(ip(hget(h,2)),slot,t as u64);bit_set(used,slot,true);word_put(ip(gget(g,5)),slot,0 as u64);}
 ptr_put(stack,depth*4,t as u64);ptr_put(stack,depth*4+1,slot);ptr_put(stack,depth*4+2,if !first&&!compute{constant(5)}else{0 as u64});ptr_put(stack,depth*4+3,if first{1 as u64}else{0 as u64});
}
fn visit(g:i64,root:i64,compute:bool)->u64{
 ensure(root!=0,12);let h=ip(gget(g,10));let capacity=add(hget(h,0),1 as u64);let bytes=mul(capacity,32 as u64);let stack=allocate(bytes,0);ensure(stack!=0,13);var depth:u64=0;frame_enter(g,root,compute,stack,depth,capacity);var returned:u64=0;
 while true{
  let off=depth*4;let t=ip(ptr(stack,off));let slot=ptr(stack,off+1);let next=ptr(stack,off+2);let first=ptr(stack,off+3)!=0;
  if next<constant(5){
   ptr_put(stack,off+2,next+1);let index=if first&&gget(g,8)==1{constant(5)-1-next}else{next};let child=ip(tget(t,2,index as i64));
   if child!=0&&(first||(tget(child,1,0)&constant(7))==0){depth+=1;frame_enter(g,child,compute,stack,depth,capacity);}continue;
  }
  if first{
   let leaf=tget(t,0,0)==constant(8)&&(tget(t,1,0)&constant(6))==0;let count=gget(g,if leaf{2}else{1});refused(count<gget(g,0),stack,14);
   if tget(t,3,0)==0{name(t,if leaf{0}else{1},count);}ptr_put(ip(gget(g,if leaf{4}else{3})),count,t as u64);gput(g,if leaf{2}else{1},count+1);
  }
  returned=slot;if depth==0{break;}depth-=1;
  if ptr(stack,depth*4+3)!=0{let counts=ip(gget(g,5));let n=word(counts,slot);refused(n<2147483647,stack,15);word_put(counts,slot,n+1);}
 }
 free(stack);return returned;
}
fn forward(g:i64,t:i64,expand:bool,compute:bool){if !expand{clear(g);}let old=gget(g,1);visit(g,t,compute);let now=gget(g,1);if now>old{ensure(ptr(ip(gget(g,3)),now-1)==t as u64,16);}}
fn copy_graph(a:i64,b:i64){
 ensure(gget(b,0)>=gget(a,1)&&gget(b,0)>=gget(a,2),17);let ah=ip(gget(a,10));let bh=ip(gget(b,10));ensure(hget(bh,0)>=hget(ah,0),18);if gget(a,6)!=0{ensure(gget(b,6)!=0&&gget(b,7)!=0,19);}
 gput(b,1,gget(a,1));gput(b,2,gget(a,2));gput(b,8,gget(a,8));var i:u64=0;while i<gget(a,2){ptr_put(ip(gget(b,4)),i,ptr(ip(gget(a,4)),i));i+=1;}i=0;while i<gget(a,1){ptr_put(ip(gget(b,3)),i,ptr(ip(gget(a,3)),i));i+=1;}
 i=0;while i<hget(ah,0){if bit(ip(hget(ah,1)),i){let key=ptr(ip(hget(ah,2)),i);let target=insert(bh,key,true);word_put(ip(gget(b,5)),target,word(ip(gget(a,5)),i));}i+=1;}
 if gget(b,6)!=0{zero(ip(gget(b,6)),hget(bh,0)*8);zero(ip(gget(b,7)),hget(bh,0)*8);}
 if gget(a,6)!=0{i=0;while i<gget(a,1){let key=ptr(ip(gget(a,3)),i);let ai=find(ah,key);let bi=find(bh,key);ensure(ai!=umax()&&bi!=umax()&&bit(ip(hget(ah,1)),ai)&&bit(ip(hget(bh,1)),bi),20);ptr_put(ip(gget(b,6)),bi,ptr(ip(gget(a,6)),ai));ptr_put(ip(gget(b,7)),bi,ptr(ip(gget(a,7)),ai));i+=1;}}
}
fn slice(out:i64,g:i64,lo:i64,hi:i64){ensure(lo>=0&&hi>=lo&&(hi as u64)<=gget(g,1),21);zero(out,constant(0));gput(out,1,(hi-lo) as u64);gput(out,3,add(gget(g,3),mul(lo as u64,8 as u64)));gput(out,5,gget(g,5));gput(out,8,gget(g,8));let a=ip(gget(out,10));let b=ip(gget(g,10));var i:i64=0;while i<3{hput(a,i,hget(b,i));i+=1;}}
fn gradient(g:i64,node:i64,acc:bool)->u64{let h=ip(gget(g,10));let i=find(h,node as u64);if i==umax()||!bit(ip(hget(h,1)),i){return 0 as u64;}let values=ip(gget(g,if acc{7}else{6}));return if values!=0{ptr(values,i)}else{0 as u64};}
fn dispatch(p:i64)->u64{
 let mode=ip(arg(p,0));let a=ip(arg(p,1));let b=ip(arg(p,2));let c=ip(arg(p,3));
 if mode==0{return hash_size(arg(p,1));}
 if mode==1{new_hash(a,arg(p,2));return 0 as u64;}
 if mode==2{reset_hash(a);return 0 as u64;}
 if mode==3{free(ip(hget(a,1)));free(ip(hget(a,2)));return 0 as u64;}
 if mode==4{return bit_words(arg(p,1));}
 if mode==5{return if bit(a,arg(p,2)){1 as u64}else{0 as u64};}
 if mode==6||mode==7{bit_set(a,arg(p,2),mode==6);return 0 as u64;}
 if mode==8{return arg(p,1)>>4;}
 if mode==9{return find(a,arg(p,2));}
 if mode==10{let i=find(a,arg(p,2));return if i!=umax()&&bit(ip(hget(a,1)),i){1 as u64}else{0 as u64};}
 if mode==11||mode==12{return insert(a,arg(p,2),mode==12);}
 if mode==13{return graph_bytes(arg(p,1),b!=0);}
 if mode==14{return add(constant(2),align_up(graph_bytes(arg(p,1),b!=0),constant(1)));}
 if mode==15{return create(a,arg(p,2),c!=0) as u64;}
 if mode==16{forward(a,b,c!=0,arg(p,4)!=0);return 0 as u64;}
 if mode==17{return visit(a,b,c!=0);}
 if mode==18{let n=ip(arg(p,3));let idx=ip(arg(p,4));ensure(idx>=0&&idx<n,22);var i:i64=0;while i<n{forward(a,ip(ptr(b,i as u64)),true,i==idx);i+=1;}return ptr(b,idx as u64);}
 if mode==19{slice(a,b,c,ip(arg(p,4)));return 0 as u64;}
 if mode==20{copy_graph(a,b);return 0 as u64;}
 if mode==21{let out=create(a,gget(b,0),gget(b,6)!=0||c!=0);copy_graph(b,out);return out as u64;}
 if mode==22{clear(a);return 0 as u64;}
 if mode==23{return gget(a,0);}
 if mode==24{let n=gget(a,1);var i=b;if i<0{i+=(n as i64);}ensure(i>=0&&(i as u64)<n,23);return ptr(ip(gget(a,3)),i as u64);}
 if mode==25{return gget(a,3);}
 if mode==26{return gget(a,1);}
 if mode==27{let n=gget(a,1);ensure(n<gget(a,0),24);ptr_put(ip(gget(a,3)),n,b as u64);gput(a,1,n+1);return 0 as u64;}
 if mode==28{var group:i64=0;while group<2{let count=gget(a,if group==0{2}else{1});let values=ip(gget(a,if group==0{4}else{3}));var i:u64=0;while i<count{let t=ip(ptr(values,i));if equal_name(t,b)!=0{return t as u64;}i+=1;}group+=1;}return 0 as u64;}
 if mode==29||mode==30{return gradient(a,b,mode==30);}
 fail(99);return 0 as u64;
}
fn main(){print(dispatch(0));}
