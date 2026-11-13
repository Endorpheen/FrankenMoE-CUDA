extern "fg153_bg" fn read(p:i64,k:i64,i:i64)->i64;
extern "fg153_bp" fn write(p:i64,k:i64,i:i64,v:i64);
extern "fg153_bs" fn call(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_bw" fn witness(mode:i64);
fn ensure(ok:bool) {if !ok {let failed=call(0,0,0,0);assert(false);}}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
fn align_size(value:i64,alignment:i64)->i64 {ensure(alignment>0&&(alignment&(alignment-1))==0);return mul(add(value,alignment-1)/alignment,alignment);}
fn initialize(talloc:i64,buffer:i64) {
 let base=call(1,buffer,0,0);let alignment=call(2,buffer,0,0);ensure(alignment>0&&(alignment&(alignment-1))==0);
 write(talloc,10,0,buffer);write(talloc,11,0,base);write(talloc,12,0,alignment);write(talloc,13,0,(alignment-base%alignment)%alignment);
}
fn attach(buffer:i64,tensor:i64,addr:i64)->i64 {
 ensure(tensor!=0&&read(tensor,2,0)==0&&read(tensor,0,0)==0&&read(tensor,1,0)==0);
 let base=call(1,buffer,0,0);ensure(addr>=base);
 if call(3,buffer,0,0)==0 {let required=call(4,buffer,tensor,0);let cap=call(5,buffer,0,0);ensure(required<=cap&&addr-base<=cap-required);}
 write(tensor,2,0,buffer);write(tensor,0,0,addr);return call(6,buffer,tensor,0);
}
fn view(tensor:i64)->i64 {
 ensure(tensor!=0&&read(tensor,2,0)==0);let source=read(tensor,1,0);ensure(source!=0&&read(source,2,0)!=0&&read(source,0,0)!=0);
 write(tensor,2,0,read(source,2,0));write(tensor,0,0,add(read(source,0,0),read(tensor,3,0)));return call(6,read(tensor,2,0),tensor,0);
}
fn allocate_tensor(talloc:i64,tensor:i64)->i64 {
 let buffer=read(talloc,10,0);let alignment=read(talloc,12,0);let size=align_size(call(4,buffer,tensor,0),alignment);let offset=read(talloc,13,0);let cap=call(5,buffer,0,0);
 ensure(size<=cap&&offset<=cap-size);let addr=add(call(1,buffer,0,0),offset);write(talloc,13,0,add(offset,size));ensure(addr%alignment==0);return attach(buffer,tensor,addr);
}
fn rollback(tensors:i64,count:i64,slab:i64,created:i64,current:i64) {
 for i in count {let tensor=read(tensors,4,i);write(tensor,0,0,read(slab,5,4*count+3*i));write(tensor,2,0,read(slab,5,4*count+3*i+1));write(tensor,7,0,read(slab,5,4*count+3*i+2));}
 for i in created {let ignored=call(7,read(slab,5,3*count+i),0,0);}if current!=0 {let ignored=call(7,current,0,0);}
}
fn list(mode:i64,buft:i64,tensors:i64,count:i64)->i64 {
 ensure(buft!=0&&count>=0&&count<=2147483647);if count==0 {return 0;}
 let slab=call(8,add(mul(count,64),read(0,20,0)),0,0);if slab==0 {return 0;}
 let talloc=add(slab,mul(count,64));let alignment=call(9,buft,0,0);var max_size=call(10,buft,0,0);if max_size<0 {max_size=9223372036854775807;}
 var current:i64=0;var first:i64=0;var items:i64=0;
 for i in count {
  let tensor=read(tensors,4,i);ensure(tensor!=0);write(slab,5,4*count+3*i,read(tensor,0,0));write(slab,5,4*count+3*i+1,read(tensor,2,0));write(slab,5,4*count+3*i+2,read(tensor,7,0));
  var size:i64=0;if read(tensor,0,0)==0&&read(tensor,1,0)==0 {size=align_size(call(11,buft,tensor,0),alignment);}
  if current>0&&(size>max_size||current>max_size-size) {write(slab,5,3*items,current);write(slab,5,3*items+1,first);write(slab,5,3*items+2,i);items+=1;current=size;first=i;}else {current=add(current,size);}
 }
 if current>0 {write(slab,5,3*items,current);write(slab,5,3*items+1,first);write(slab,5,3*items+2,count);items+=1;}
 if mode==1 {var size:i64=0;for i in items {size=add(size,read(slab,5,3*i));}let ignored=call(12,slab,0,0);return size;}
 var made:i64=0;
 for i in items {
  let buffer=call(13,buft,read(slab,5,3*i),0);if buffer==0 {rollback(tensors,count,slab,made,0);let ignored=call(12,slab,0,0);return 0;}
  initialize(talloc,buffer);var failed=false;var index=read(slab,5,3*i+1);let end=read(slab,5,3*i+2);
  while index<end {let tensor=read(tensors,4,index);var status=read(0,21,0);
   if read(tensor,0,0)==0 {if read(tensor,1,0)==0 {status=allocate_tensor(talloc,tensor);}else {if read(tensor,2,0)==0 {status=view(tensor);}}}
   else {if read(tensor,1,0)!=0&&read(tensor,2,0)==0 {status=view(tensor);}}
   if status!=read(0,21,0) {failed=true;break;}index+=1;
  }
  if failed {rollback(tensors,count,slab,made,buffer);let ignored=call(12,slab,0,0);return 0;}
  write(slab,5,3*count+made,buffer);made+=1;
 }
 var result:i64=0;if made==1 {result=read(slab,5,3*count);}if made>1 {result=call(14,add(slab,mul(3*count,8)),made,0);}
 let ignored=call(12,slab,0,0);return result;
}
fn context(mode:i64,ctx:i64,buft:i64)->i64 {
 ensure(call(15,ctx,0,0)!=0);var count:i64=0;var tensor=call(16,ctx,0,0);
 while tensor!=0 {count=add(count,1);ensure(count<=2147483647);tensor=call(17,ctx,tensor,0);}if count==0 {return 0;}
 let tensors=call(8,mul(count,8),0,0);if tensors==0 {return 0;}tensor=call(16,ctx,0,0);var i:i64=0;
 while tensor!=0 {ensure(i<count);write(tensors,6,i,tensor);i+=1;tensor=call(17,ctx,tensor,0);}ensure(i==count);
 let result=call(18+mode,buft,tensors,count);let ignored=call(12,tensors,0,0);return result;
}
fn entry(mode:i64,a:i64,b:i64,c:i64)->i64 {
 witness(mode);if mode==0||mode==1 {return list(mode,a,b,c);}if mode==2 {initialize(a,b);return 0;}if mode==3 {return allocate_tensor(a,b);}if mode==4 {return view(a);}if mode==5 {return attach(a,b,c);}if mode==6||mode==7 {return context(mode-6,a,b);}ensure(false);return 0;
}
fn main(){print(entry(0,0,0,0));}
