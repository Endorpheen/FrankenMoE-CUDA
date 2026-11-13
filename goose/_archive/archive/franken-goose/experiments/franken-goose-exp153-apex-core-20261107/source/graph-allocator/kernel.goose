extern "fg153_gg" fn read(p:i64,k:i64,i:i64)->i64;
extern "fg153_gp" fn write(p:i64,k:i64,i:i64,v:i64);
extern "fg153_gs" fn call(op:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg153_memory" fn memory(mode:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg153_mg" fn mread(p:i64,k:i64,i:i64)->i64;
extern "fg153_mp" fn mwrite(p:i64,k:i64,i:i64,v:i64);
extern "fg153_gw" fn witness(mode:i64);
fn ensure(ok:bool) {if !ok {let ignored=call(0,0,0,0,0);assert(false);}}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
fn aligned_bytes(n:i64,a:i64)->i64 {ensure(a>0&&(a&(a-1))==0);return mul(add(n,a-1)/a,a);}
fn heap(n:i64)->i64 {let result=call(1,n,0,0,0);ensure(result!=0);return result;}
fn clear(p:i64) {let ignored=call(2,p,0,0,0);}
fn hash(g:i64,t:i64)->i64 {return call(3,g,t,0,0);}
fn destroy(g:i64) {
 if g==0 {return;}let count=read(g,0,0);
 for i in count {
  if read(g,12,0)!=0 {let p=read(g,2,i);var seen=false;for j in i {if read(g,2,j)==p {seen=true;}}if !seen {let ignored=memory(6,p,0,0,0);}}
  if read(g,13,0)!=0 {let p=read(g,3,i);var seen=false;for j in i {if read(g,3,j)==p {seen=true;}}if !seen {let ignored=memory(1,p,0,0,0);}}
 }
 let ignored=call(4,g,0,0,0);clear(read(g,6,0));for k in 4 {clear(read(g,11+k,0));}clear(read(g,7,0));clear(read(g,9,0));clear(g);
}
fn create(types:i64,count:i64)->i64 {
 ensure(count>0&&count<=2147483647);let g=heap(read(0,70,0));write(g,0,0,count);
 for k in 4 {let storage=call(1,mul(count,8),0,0,0);if storage==0 {destroy(g);ensure(false);}write(g,11+k,0,storage);}
 for i in count {let kind=read(types,82,i);write(g,1,i,kind);var alloc:i64=0;
  for j in i {if read(types,82,j)==kind {alloc=read(g,3,j);break;}}
  if alloc==0 {alloc=memory(0,call(5,kind,0,0,0),call(6,kind,0,0,0),0,0);if alloc==0 {destroy(g);ensure(false);}}
  write(g,3,i,alloc);
 }return g;
}
fn limits(g:i64,values:i64) {
 if g==0 {return;}for i in read(g,0,0) {var limit:i64=0;if values!=0 {limit=read(values,81,i);for j in read(g,0,0) {if read(g,3,i)==read(g,3,j) {let other=read(values,81,j);if (limit<0&&other>=0)||(limit>=0&&other>=0&&other<limit)||(limit<0&&other<limit) {limit=other;}}}}write(g,4,i,limit);}
}
fn record(g:i64,slot:i64,t:i64) {
 if t==0||read(t,31,0)!=0||read(t,30,0)!=0 {write(slot,50,0,-1);write(slot,51,0,-1);write(slot,52,0,-1);write(slot,53,0,0);return;}
 let h=hash(g,t);let id=read(h,42,0);ensure(id>=0&&id<read(g,0,0));write(slot,50,0,id);write(slot,51,0,read(h,43,0));write(slot,52,0,read(h,44,0));write(slot,53,0,call(7,read(g,1,id),t,0,0));
}
fn reserve(g:i64,graph:i64,nids:i64,lids:i64,no_alloc:i64)->i64 {
 let nodes=read(graph,20,0);let leaves=read(graph,21,0);let count=add(nodes,leaves);let minimum=add(count,count/4);
 if read(g,5,0)<minimum {let ignored=call(8,g,minimum,0,0);}
 for i in read(g,0,0) {let ignored=memory(2,read(g,3,i),0,0,0);}let walked=call(9,g,graph,nids,lids);
 if read(g,8,0)<nodes {clear(read(g,7,0));write(g,7,0,heap(mul(nodes,read(0,71,0))));}write(g,8,0,nodes);
 for i in nodes {let t=read(graph,22,i);record(g,read(g,15,i),t);for j in read(0,74,0) {record(g,read(g,16,i*read(0,74,0)+j),read(t,35,j));}}
 if read(g,10,0)<leaves {clear(read(g,9,0));write(g,9,0,heap(mul(leaves,read(0,72,0))));}write(g,10,0,leaves);
 for i in leaves {record(g,read(g,17,i),read(graph,23,i));}
 for i in read(g,0,0) {
  let alloc=read(g,3,i);for j in i {if read(g,3,j)==alloc {write(g,2,i,read(g,2,j));break;}}
  let previous=read(g,2,i);var needed=previous==0;
  for chunk in mread(alloc,3,0) {var old:i64=0;if previous!=0 {old=memory(8,previous,chunk,0,0);}if memory(4,alloc,chunk,0,0)>old {needed=true;}}
  if needed {
   // Clear every alias before freeing so an OOM cannot leave a shared dangling buffer.
   for j in read(g,0,0) {if read(g,2,j)==previous {write(g,2,j,0);}}
   let freed=memory(6,previous,0,0,0);
   if no_alloc==0 {let next=memory(5,read(g,1,i),alloc,read(0,78,0),read(g,4,i));if next==0 {return 0;}
    for j in read(g,0,0) {if read(g,3,j)==alloc {write(g,2,j,next);}}
   }
  }
 }return 1;
}
fn init_tensor(g:i64,t:i64,slot:i64)->i64 {
 let id=read(slot,50,0);let view=read(t,31,0);
 if view!=0 {if read(t,32,0)==0&&read(view,32,0)!=0 {if call(10,t,0,0,0)!=read(0,79,0) {return 0;}}return 1;}
 if read(t,30,0)==0 {ensure(id>=0&&id<read(g,0,0));ensure(call(7,read(g,1,id),t,0,0)<=read(slot,53,0));if memory(10,read(g,2,id),t,read(slot,51,0),read(slot,52,0))!=read(0,79,0) {return 0;}}
 return 1;
}
fn fits(g:i64,t:i64,slot:i64)->bool {
 var size:i64=0;if read(t,30,0)==0&&read(t,31,0)==0 {let id=read(slot,50,0);if id<0 {return false;}ensure(id<read(g,0,0));size=call(7,read(g,1,id),t,0,0);}return read(slot,53,0)>=size;
}
fn needs(g:i64,graph:i64)->bool {
 if read(g,8,0)!=read(graph,20,0)||read(g,10,0)!=read(graph,21,0) {return true;}
 for i in read(graph,20,0) {let t=read(graph,22,i);if !fits(g,t,read(g,15,i)) {return true;}for j in read(0,74,0) {let src=read(t,35,j);if src!=0&&!fits(g,src,read(g,16,i*read(0,74,0)+j)) {return true;}}}return false;
}
fn capture(journal:i64,index:i64,t:i64) {write(journal,84,4*index,t);write(journal,84,4*index+1,read(t,30,0));write(journal,84,4*index+2,read(t,32,0));write(journal,84,4*index+3,read(t,39,0));}
fn restore(journal:i64,count:i64) {for i in count {let t=read(journal,84,4*i);write(t,30,0,read(journal,84,4*i+1));write(t,32,0,read(journal,84,4*i+2));write(t,39,0,read(journal,84,4*i+3));}}
fn allocate_graph(g:i64,graph:i64)->i64 {
 if needs(g,graph) {if read(g,0,0)!=1||reserve(g,graph,0,0,0)==0 {return 0;}}
 for i in read(g,0,0) {let buffer=read(g,2,i);if buffer!=0 {let ignored=memory(9,buffer,0,0,0);}}
 let capacity=add(read(graph,21,0),mul(read(graph,20,0),add(read(0,74,0),1)));if capacity==0 {return 1;}
 let journal=call(1,mul(capacity,32),0,0,0);if journal==0 {return 0;}var count:i64=0;
 for i in read(graph,21,0) {capture(journal,count,read(graph,23,i));count+=1;}
 for i in read(graph,20,0) {let t=read(graph,22,i);for j in read(0,74,0) {let src=read(t,35,j);if src!=0 {ensure(count<capacity);capture(journal,count,src);count+=1;}}ensure(count<capacity);capture(journal,count,t);count+=1;}
 for i in read(graph,21,0) {if init_tensor(g,read(graph,23,i),read(g,17,i))==0 {restore(journal,count);clear(journal);return 0;}}
 for i in read(graph,20,0) {let t=read(graph,22,i);for j in read(0,74,0) {let src=read(t,35,j);if src!=0 {if init_tensor(g,src,read(g,16,i*read(0,74,0)+j))==0 {restore(journal,count);clear(journal);return 0;}}}
  if init_tensor(g,t,read(g,15,i))==0 {restore(journal,count);clear(journal);return 0;}}
 clear(journal);return 1;
}
fn free_bytes(alloc:i64,chunk:i64,offset:i64,size:i64) {ensure(chunk>=0&&chunk<mread(alloc,3,0));let c=mread(alloc,2,chunk);let count=call(11,c,offset,aligned_bytes(size,mread(alloc,0,0)),0);mwrite(c,10,0,count);}
fn extra(g:i64,node:i64,parent:i64) {
 let h=hash(g,node);let ph=hash(g,parent);let alloc=read(g,3,read(ph,42,0));let alignment=mread(alloc,0,0);
 let parent_size=aligned_bytes(call(7,read(g,1,read(ph,42,0)),parent,0,0),alignment);let node_size=aligned_bytes(call(7,read(g,1,read(h,42,0)),node,0,0),alignment);ensure(parent_size>=node_size);
 if parent_size>node_size {free_bytes(alloc,read(ph,43,0),add(read(ph,44,0),node_size),parent_size-node_size);}
}
fn copy_address(dst:i64,src:i64) {write(dst,42,0,read(src,42,0));write(dst,43,0,read(src,43,0));write(dst,44,0,read(src,44,0));}
fn own(g:i64,t:i64)->bool {return read(hash(g,t),45,0)!=0;}
fn same_layout(a:i64,b:i64)->bool {if read(a,36,0)!=read(b,36,0) {return false;}for i in 4 {if read(a,37,i)!=read(b,37,i)||read(a,38,i)!=read(b,38,i) {return false;}}return true;}
fn is_view(t:i64)->bool {return read(t,31,0)!=0;}
fn inplace(op:i64)->bool {return op==73||op==32||op==45||op==44||op==2||op==3||op==4||op==6||op==7||op==8||op==9||op==10||op==11||op==91||op==48||op==49||op==23||op==25||op==26||op==50||op==46||op==47;}
fn allocate_node(g:i64,t:i64,id:i64) {
 ensure(id>=0&&id<read(g,0,0));let h=hash(g,t);if read(t,30,0)!=0||read(t,32,0)!=0||own(g,t)||is_view(t) {return;}
 write(h,45,0,1);
 if inplace(read(t,34,0)) {for i in read(0,74,0) {let parent=read(t,35,i);if parent==0 {continue;}if !own(g,parent) {continue;}
  let owner=read(parent,31,0);if (read(parent,33,0)&read(0,75,0))!=0 {continue;}if owner!=0&&(read(owner,33,0)&read(0,75,0))!=0 {continue;}
  if !same_layout(t,parent) {continue;}let ph=hash(g,parent);if read(ph,40,0)!=1||read(ph,41,0)!=0 {continue;}
  if is_view(parent) {ensure(owner!=0);let vh=hash(g,owner);if read(vh,41,0)==1&&read(vh,40,0)==0&&read(owner,30,0)==read(parent,30,0) {copy_address(h,ph);write(ph,45,0,0);write(vh,45,0,0);extra(g,t,owner);return;}}
  else {copy_address(h,ph);write(ph,45,0,0);extra(g,t,parent);return;}
 }}
 let alloc=read(g,3,id);let size=call(7,read(g,1,id),t,0,0);write(h,42,0,id);let ignored=memory(12,alloc,size,read(h,49,0),0);
}
fn free_node(g:i64,t:i64) {if (read(t,33,0)&read(0,75,0))!=0 {return;}let h=hash(g,t);let id=read(h,42,0);ensure(id>=0&&id<read(g,0,0));free_bytes(read(g,3,id),read(h,43,0),read(h,44,0),call(7,read(g,1,id),t,0,0));write(h,45,0,0);}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 witness(mode);if mode==0 {return create(a,b);}if mode==1 {destroy(a);return 0;}if mode==2 {limits(a,b);return 0;}if mode==3 {return reserve(a,b,c,d,e);}if mode==4 {ensure(init_tensor(a,b,c)!=0);return 0;}if mode==5 {var answer:i64=0;if needs(a,b) {answer=1;}return answer;}if mode==6 {return allocate_graph(a,b);}
 if mode==7 {ensure(b>=0&&b<read(a,0,0));let buffer=read(a,2,b);if buffer==0 {return 0;}for i in b {if read(a,2,i)==buffer {return 0;}}return memory(7,buffer,0,0,0);}
 if mode==8 {allocate_node(a,b,c);return 0;}if mode==9 {free_node(a,b);return 0;}if mode==10 {extra(a,b,c);return 0;}ensure(false);return 0;
}
fn main(){print(entry(1,0,0,0,0,0));}
