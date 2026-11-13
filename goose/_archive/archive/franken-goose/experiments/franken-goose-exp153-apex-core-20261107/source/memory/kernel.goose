extern "fg153_mg" fn read(p:i64,k:i64,i:i64)->i64;
extern "fg153_mp" fn write(p:i64,k:i64,i:i64,v:i64);
extern "fg153_ms" fn call(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_mw" fn witness(mode:i64);
fn ensure(ok:bool) {if !ok {let ignored=call(0,0,0,0);assert(false);}}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
fn align_size(n:i64,a:i64)->i64 {ensure(a>0&&(a&(a-1))==0);return mul(add(n,a-1)/a,a);}
fn minimum(a:i64,b:i64)->i64 {return if a<b {a}else{b};}
fn maximum(a:i64,b:i64)->i64 {return if a>b {a}else{b};}
fn reset(alloc:i64) {for i in read(0,33,0) {let ignored=call(2,read(alloc,2,i),0,0);write(alloc,2,i,0);}write(alloc,3,0,0);}
fn create(alignment:i64,limit:i64)->i64 {
 ensure(alignment>0&&(alignment&(alignment-1))==0);let p=call(1,read(0,30,0),0,0);if p==0 {return 0;}
 var cap=limit;if cap<0 {cap=9223372036854775807;}write(p,0,0,alignment);write(p,1,0,minimum(cap,9223372036854775807));return p;
}
fn destroy(alloc:i64) {if alloc!=0 {reset(alloc);let ignored=call(2,alloc,0,0);}}
fn new_chunk(alloc:i64,requested:i64)->i64 {
 let count=read(alloc,3,0);let max_chunks=read(0,33,0);ensure(count>=0&&count<=max_chunks);if count==max_chunks {return -1;}
 let chunk=call(1,read(0,31,0),0,0);if chunk==0 {return -1;}
 var cap=maximum(requested,read(alloc,1,0));if count==max_chunks-1 {cap=9223372036854775807;}
 write(chunk,10,0,1);write(chunk,12,0,cap);write(alloc,2,count,chunk);write(alloc,3,0,count+1);return count;
}
fn remove_slot(chunk:i64,index:i64) {
 let count=read(chunk,10,0);ensure(index>=0&&index<count);var i=index;
 while i<count-1 {write(chunk,11,i,read(chunk,11,i+1));write(chunk,12,i,read(chunk,12,i+1));i+=1;}write(chunk,10,0,count-1);
}
fn allocate(alloc:i64,requested:i64,address:i64)->i64 {
 let size=align_size(requested,read(alloc,0,0));var chosen_chunk:i64= -1;var chosen_slot:i64= -1;
 for i in read(alloc,3,0) {let chunk=read(alloc,2,i);let selected=call(15,chunk,size,0);if selected>=0 {chosen_chunk=i;chosen_slot=selected;}}
 if chosen_slot<0 {var best:i64= -9223372036854775807-1;
  for i in read(alloc,3,0) {let chunk=read(alloc,2,i);let count=read(chunk,10,0);if count>0 {let index=count-1;let reuse=read(chunk,13,0)-add(read(chunk,11,index),size);let better=(best<0&&reuse>best)||(reuse>=0&&reuse<best);
   if read(chunk,12,index)>=size&&better {chosen_chunk=i;chosen_slot=index;best=reuse;}
  }}
 }
 if chosen_slot<0 {chosen_chunk=new_chunk(alloc,size);chosen_slot=0;}ensure(chosen_chunk>=0);
 let chunk=read(alloc,2,chosen_chunk);let offset=read(chunk,11,chosen_slot);let available=read(chunk,12,chosen_slot);ensure(available>=size);
 write(address,20,0,chosen_chunk);write(address,21,0,offset);write(chunk,11,chosen_slot,add(offset,size));write(chunk,12,chosen_slot,available-size);
 if available==size {remove_slot(chunk,chosen_slot);}write(chunk,13,0,maximum(read(chunk,13,0),add(offset,size)));return 0;
}
fn free_vbuffer(buf:i64) {if buf!=0 {for i in read(0,33,0) {let ignored=call(3,read(buf,4,i),0,0);}let ignored=call(2,buf,0,0);}}
fn grow(buft:i64,size:i64,limit:i64)->i64 {
 if limit==0 {return size;}var maximum_size=call(4,buft,0,0);if maximum_size<0 {maximum_size=9223372036854775807;}
 var target:i64=9223372036854775807;if size<=9223372036854775807/4 {target=4*size;}if limit>=0 {target=minimum(target,limit);}target=minimum(target,maximum_size);return maximum(size,target);
}
fn vbuffer(buft:i64,alloc:i64,usage:i64,limit:i64)->i64 {
 let buf=call(1,read(0,32,0),0,0);if buf==0 {return 0;}let chunks=read(alloc,3,0);
 for i in chunks {let chunk=read(alloc,2,i);let required=read(chunk,13,0);var size=required;
  if usage==read(0,34,0)&&chunks==1&&(limit<0||limit>required) {size=grow(buft,required,limit);}
  var buffer=call(5,buft,size,0);if buffer==0&&size>required {buffer=call(5,buft,required,0);}write(buf,4,i,buffer);
  if buffer==0 {free_vbuffer(buf);return 0;}let ignored=call(6,buffer,usage,0);
 }return buf;
}
fn size(buf:i64)->i64 {var bytes:i64=0;for i in read(0,33,0) {let buffer=read(buf,4,i);if buffer==0 {break;}bytes=add(bytes,call(7,buffer,0,0));}return bytes;}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64)->i64 {
 witness(mode);if mode==0 {return create(a,b);}if mode==1 {destroy(a);return 0;}if mode==2 {reset(a);return 0;}if mode==3 {return new_chunk(a,b);}
 if mode==4 {ensure(b>=0&&b<read(0,33,0));if b>=read(a,3,0) {return 0;}return read(read(a,2,b),13,0);}
 if mode==5 {return vbuffer(a,b,c,d);}if mode==6 {free_vbuffer(a);return 0;}if mode==7 {return size(a);}
 if mode==8 {ensure(b>=0&&b<read(0,33,0));let buffer=read(a,4,b);if buffer==0 {return 0;}return call(7,buffer,0,0);}
 if mode==9 {for i in read(0,33,0) {let buffer=read(a,4,i);if buffer==0 {break;}let ignored=call(8,buffer,0,0);}return 0;}
 if mode==10 {ensure(c>=0&&c<read(0,33,0));let buffer=read(a,4,c);let addr=add(call(9,buffer,0,0),d);return call(10,buffer,b,addr);}
 if mode==11 {return grow(a,b,c);}if mode==12 {return allocate(a,b,c);}ensure(false);return 0;
}
fn main(){print(entry(7,0,0,0,0));}
