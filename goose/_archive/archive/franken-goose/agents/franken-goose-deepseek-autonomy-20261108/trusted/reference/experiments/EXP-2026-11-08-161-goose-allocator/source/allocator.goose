// Own allocator control/state transitions;imports are typed storage,heap or backend effects.
extern "fg161_arg" fn arg(p:i64,i:i64)->u64;
extern "fg161_get" fn get(p:i64,kind:i64,k:i64,i:i64)->u64;
extern "fg161_put" fn put(p:i64,kind:i64,k:i64,i:i64,v:u64);
extern "fg161_constant" fn constant(k:i64)->u64;
extern "fg161_heap" fn heap(n:u64,clear:i64)->i64;
extern "fg161_release" fn release(p:i64);
extern "fg161_zero" fn zero(p:i64,n:u64);
extern "fg161_copy" fn copy(dst:i64,src:i64,n:u64);
extern "fg161_ptr" fn ptr(p:i64,i:i64)->u64;
extern "fg161_ptr_put" fn ptr_put(p:i64,i:i64,v:u64);
extern "fg161_i32" fn i32(p:i64,i:i64)->i64;
extern "fg161_i32_put" fn i32_put(p:i64,i:i64,v:i64);
extern "fg161_record" fn record(p:i64,kind:i64,i:i64,sub:i64)->i64;
extern "fg161_buffer" fn backend(p:i64,what:i64,t:i64)->u64;
extern "fg161_buft" fn buft(p:i64,what:i64,t:i64)->u64;
extern "fg161_alloc_buffer" fn allocate_buffer(buft:i64,size:u64)->i64;
extern "fg161_free_buffer" fn free_buffer(buffer:i64);
extern "fg161_usage" fn set_usage(buffer:i64,usage:i64);
extern "fg161_reset_buffer" fn reset_buffer(buffer:i64);
extern "fg161_tensor_alloc" fn tensor_alloc(buffer:i64,t:i64,address:u64)->i64;
extern "fg161_view_init" fn view_init(t:i64);
extern "fg161_hash_size" fn hash_size(n:u64)->u64;
extern "fg161_hash_slot" fn hash_slot(g:i64,t:i64)->u64;
extern "fg161_hash_reset" fn hash_reset(g:i64);
extern "fg161_first" fn first(c:i64)->i64;
extern "fg161_next" fn next(c:i64,t:i64)->i64;
extern "fg161_no_alloc" fn no_alloc(c:i64)->i64;
extern "fg161_bulk" fn bulk(buft:i64,tensors:i64,n:i64,size_only:i64)->u64;
extern "fg161_default_buft" fn default_buft(backend:i64)->i64;
extern "fg161_fail" fn fail(code:i64);
fn can_inplace(op:i64)->bool{
if op==73{return true;}
if op==32{return true;}
if op==45{return true;}
if op==44{return true;}
if op==2{return true;}
if op==3{return true;}
if op==4{return true;}
if op==6{return true;}
if op==7{return true;}
if op==8{return true;}
if op==9{return true;}
if op==10{return true;}
if op==11{return true;}
if op==91{return true;}
if op==48{return true;}
if op==49{return true;}
if op==23{return true;}
if op==25{return true;}
if op==26{return true;}
if op==50{return true;}
if op==46{return true;}
if op==47{return true;}
return false;}
fn ip(x:u64)->i64{return x as! i64;}
fn U()->u64{return 18446744073709551615 as u64;}
fn ensure(v:bool,code:i64){if !v{fail(code);}}
fn add(a:u64,b:u64)->u64{ensure(a<=U()-b,1);return a+b;}
fn mul(a:u64,b:u64)->u64{ensure(a==0||b<=U()/a,2);return a*b;}
fn alignment(a:u64){ensure(a>0&&(a&(a-1))==0,3);}
fn aligned(base:u64,off:u64,a:u64)->u64{alignment(a);let at=add(base,off);let more=(a-at%a)%a;return add(off,more);}
fn g(p:i64,k:i64,i:i64)->u64{return get(p,4,k,i);}
fn gs(p:i64,k:i64,i:i64,v:u64){put(p,4,k,i,v);}
fn t(p:i64,k:i64,i:i64)->u64{return get(p,7,k,i);}
fn d(p:i64,k:i64,i:i64)->u64{return get(p,1,k,i);}
fn ds(p:i64,k:i64,i:i64,v:u64){put(p,1,k,i,v);}
fn ch(p:i64,k:i64,i:i64)->u64{return get(p,2,k,i);}
fn cs(p:i64,k:i64,i:i64,v:u64){put(p,2,k,i,v);}
fn state(galloc:i64,node:i64)->i64{let slot=hash_slot(galloc,node);ensure(slot<get(ip(g(galloc,5,0)),9,0,0),4);return record(galloc,0,slot as i64,0);}
fn hs(p:i64,k:i64)->u64{return get(p,5,k,0);}
fn hw(p:i64,k:i64,v:u64){put(p,5,k,0,v);}
fn addr(out:i64,chunk:i64,off:u64){put(out,10,0,0,chunk as! u64);put(out,10,1,0,off);}
fn remove_block(c:i64,i:i64){let n=ip(ch(c,0,0));ensure(i>=0&&i<n,5);var j=i;while j<n-1{cs(c,2,j,ch(c,2,j+1));cs(c,3,j,ch(c,3,j+1));j+=1;}cs(c,0,0,(n-1) as u64);}
fn insert_block(c:i64,off:u64,size:u64){let n=ip(ch(c,0,0));ensure(n>=0&&(n as u64)<constant(11),6);var at:i64=0;while at<n&&ch(c,2,at)<off{at+=1;}var j=n;while j>at{cs(c,2,j,ch(c,2,j-1));cs(c,3,j,ch(c,3,j-1));j-=1;}cs(c,2,at,off);cs(c,3,at,size);cs(c,0,0,(n+1) as u64);}
fn new_chunk(a:i64,size:u64)->i64{let n=ip(d(a,2,0));if (n as u64)>=constant(10){return -1;}let c=heap(constant(2),1);ensure(c!=0,7);cs(c,0,0,1 as u64);var available=d(a,1,0);if size>available{available=size;}if (n as u64)==constant(10)-1{available=U()/2;}cs(c,3,0,available);ds(a,3,n,c as u64);ds(a,2,0,(n+1) as u64);return n;}
fn dyn_try(a:u64,maxsize:u64)->i64{alignment(a);let p=heap(constant(1),1);if p==0{return 0;}ds(p,0,0,a);ds(p,1,0,if maxsize<U()/2{maxsize}else{U()/2});return p;}
fn dyn_new(a:u64,maxsize:u64)->i64{let p=dyn_try(a,maxsize);ensure(p!=0,8);return p;}
fn dyn_reset(a:i64){var i:i64=0;while (i as u64)<constant(10){release(ip(d(a,3,i)));ds(a,3,i,0 as u64);i+=1;}ds(a,2,0,0 as u64);}
fn dyn_free(a:i64){if a==0{return;}var i:i64=0;while i<ip(d(a,2,0)){release(ip(d(a,3,i)));i+=1;}release(a);}
fn dyn_size(a:i64,c:i64)->u64{ensure(c>=0,9);return if c<ip(d(a,2,0)){ch(ip(d(a,3,c)),1,0)}else{0 as u64};}
fn dyn_alloc(a:i64,size0:u64,out:i64){
 let size=aligned(0 as u64,size0,d(a,0,0));var best_chunk:i64=-1;var best_block:i64=-1;var c:i64=0;
 // Preserve the archived selector's last-tie rule and the native per-chunk reset.
 while c<ip(d(a,2,0)){let chunk=ip(d(a,3,c));var best=U();var i:i64=0;while i<ip(ch(chunk,0,0))-1{let bytes=ch(chunk,3,i);if bytes>=size&&bytes<=best{best_chunk=c;best_block=i;best=bytes;}i+=1;}c+=1;}
 if best_block<0{var reuse:i64=-9223372036854775807-1;c=0;while c<ip(d(a,2,0)){let chunk=ip(d(a,3,c));let n=ip(ch(chunk,0,0));if n>0{let i=n-1;let fit=(ch(chunk,1,0)-ch(chunk,2,i)-size) as! i64;let better=reuse<0&&fit>reuse;let tighter=fit>=0&&fit<reuse;if ch(chunk,3,i)>=size&&(better||tighter){best_chunk=c;best_block=i;reuse=fit;}}c+=1;}}
 if best_block<0{best_chunk=new_chunk(a,size);best_block=0;}ensure(best_chunk>=0,10);let chunk=ip(d(a,3,best_chunk));let off=ch(chunk,2,best_block);let end=add(off,size);ensure(ch(chunk,3,best_block)>=size,11);cs(chunk,2,best_block,end);cs(chunk,3,best_block,ch(chunk,3,best_block)-size);if ch(chunk,3,best_block)==0{remove_block(chunk,best_block);}if end>ch(chunk,1,0){cs(chunk,1,0,end);}addr(out,best_chunk,off);
}
fn dyn_release(a:i64,chunkid:i64,off:u64,size0:u64){
 ensure(chunkid>=0&&chunkid<ip(d(a,2,0)),12);let size=aligned(0 as u64,size0,d(a,0,0));let end=add(off,size);let c=ip(d(a,3,chunkid));ensure(end<=ch(c,1,0),13);var i:i64=0;
 // Reject overlapping/double release before mutating the free list;zero-size remains valid.
 if size!=0{while i<ip(ch(c,0,0)){let start=ch(c,2,i);let length=ch(c,3,i);if length!=0{let finish=add(start,length);ensure(end<=start||off>=finish,14);}i+=1;}}
 i=0;while i<ip(ch(c,0,0)){let start=ch(c,2,i);let length=ch(c,3,i);
  if add(start,length)==off{cs(c,3,i,add(length,size));if i<ip(ch(c,0,0))-1&&add(start,ch(c,3,i))==ch(c,2,i+1){cs(c,3,i,add(ch(c,3,i),ch(c,3,i+1)));remove_block(c,i+1);}return;}
  if end==start{cs(c,2,i,off);cs(c,3,i,add(length,size));if i>0&&add(ch(c,2,i-1),ch(c,3,i-1))==off{cs(c,3,i-1,add(ch(c,3,i-1),ch(c,3,i)));remove_block(c,i);}return;}i+=1;
 }insert_block(c,off,size);
}
fn vf(p:i64,i:i64)->i64{return ip(get(p,3,0,i));}
fn vfree(p:i64){if p==0{return;}var i:i64=0;while (i as u64)<constant(10){free_buffer(vf(p,i));i+=1;}release(p);}
fn vchunk(p:i64,i:i64)->u64{let b=vf(p,i);return if b!=0{backend(b,2,0)}else{0 as u64};}
fn vsize(p:i64)->u64{var size:u64=0;var i:i64=0;while (i as u64)<constant(10)&&vf(p,i)!=0{size=add(size,backend(vf(p,i),2,0));i+=1;}return size;}
fn grow(kind:i64,size:u64,limit:u64)->u64{if limit==0{return size;}var wanted=if size>(U()-3)/4{U()}else{size*4};if limit<wanted{wanted=limit;}let maxsize=buft(kind,1,0);if maxsize<wanted{wanted=buft(kind,1,0);}return if wanted>size{wanted}else{size};}
fn valloc(kind:i64,a:i64,usage:i64,limit:u64)->i64{let p=heap(constant(3),1);if p==0{return 0;}var i:i64=0;while i<ip(d(a,2,0)){let size=ch(ip(d(a,3,i)),1,0);var wanted=size;if (usage as u64)==constant(15)&&d(a,2,0)==1&&limit>size{wanted=grow(kind,size,limit);}var b=allocate_buffer(kind,wanted);if b==0&&wanted>size{b=allocate_buffer(kind,size);}put(p,3,0,i,b as u64);if b==0{vfree(p);return 0;}set_usage(b,usage);i+=1;}return p;}
fn vtalloc(p:i64,tensor:i64,chunk:i64,off:u64){ensure(chunk>=0&&(chunk as u64)<constant(10),15);let b=vf(p,chunk);ensure(b!=0,16);let base=backend(b,0,0);ensure(off<=backend(b,2,0),17);tensor_alloc(b,tensor,add(base,off));}
fn vreset(p:i64){var i:i64=0;while (i as u64)<constant(10)&&vf(p,i)!=0{reset_buffer(vf(p,i));i+=1;}}
fn ga_free(p:i64){
 if p==0{return;}var i:i64=0;while i<ip(g(p,0,0)){
  if g(p,2,-1)!=0{let b=g(p,2,i);var j:i64=0;var done=false;while j<i{if g(p,2,j)==b{done=true;break;}j+=1;}if !done{vfree(ip(b));}}
  if g(p,3,-1)!=0{let a=g(p,3,i);var j:i64=0;var done=false;while j<i{if g(p,3,j)==a{done=true;break;}j+=1;}if !done{dyn_free(ip(a));}}i+=1;
 }
 let h=ip(g(p,5,0));release(ip(get(h,9,1,0)));release(ip(get(h,9,2,0)));release(ip(g(p,6,0)));release(ip(g(p,1,-1)));release(ip(g(p,2,-1)));release(ip(g(p,3,-1)));release(ip(g(p,4,-1)));release(ip(g(p,8,0)));release(ip(g(p,10,0)));release(p);
}
fn ga_new(kinds:i64,n:i64)->i64{
 ensure(n>=0&&(n==0||kinds!=0),18);let bytes=mul(n as u64,8 as u64);let p=heap(constant(4),1);ensure(p!=0,19);gs(p,0,0,n as u64);var k:i64=1;
 while k<=4{let v=heap(bytes,1);if v==0{ga_free(p);fail(20);return 0;}gs(p,k,-1,v as u64);k+=1;}
 var i:i64=0;while i<n{let kind=ip(ptr(kinds,i));if kind==0{ga_free(p);fail(21);return 0;}gs(p,1,i,kind as u64);var j:i64=0;while j<i{if ptr(kinds,j)==(kind as u64){gs(p,3,i,g(p,3,j));break;}j+=1;}
  if g(p,3,i)==0{let a=buft(kind,0,0);if a==0||(a&(a-1))!=0{ga_free(p);fail(22);return 0;}let dyn=dyn_try(a,buft(kind,1,0));if dyn==0{ga_free(p);fail(38);return 0;}gs(p,3,i,dyn as u64);}i+=1;
 }return p;
}
fn limits(p:i64,values:i64){if p==0{return;}var i:i64=0;while i<ip(g(p,0,0)){var limit=if values!=0{ptr(values,i)}else{0 as u64};if values!=0{var j:i64=0;while j<ip(g(p,0,0)){if g(p,3,j)==g(p,3,i)&&ptr(values,j)<limit{limit=ptr(values,j);}j+=1;}}gs(p,4,i,limit);i+=1;}}
fn own(p:i64,node:i64)->bool{return hs(state(p,node),5)!=0;}
fn allocated(p:i64,node:i64)->bool{return t(node,6,0)!=0||t(node,5,0)!=0||own(p,node);}
fn layout(a:i64,b:i64)->bool{if t(a,2,0)!=t(b,2,0){return false;}var i:i64=0;while i<4{if t(a,0,i)!=t(b,0,i)||t(a,1,i)!=t(b,1,i){return false;}i+=1;}return true;}
fn move_addr(dst:i64,src:i64){hw(dst,2,hs(src,2));hw(dst,3,hs(src,3));hw(dst,4,hs(src,4));}
fn valid_id(p:i64,i:i64){ensure(i>=0&&i<ip(g(p,0,0)),23);}
fn extra(p:i64,node:i64,parent:i64){let hn=state(p,node);let ph=state(p,parent);let pi=ip(hs(ph,2));let ni=ip(hs(hn,2));valid_id(p,pi);valid_id(p,ni);var ps=buft(ip(g(p,1,pi)),2,parent);var ns=buft(ip(g(p,1,ni)),2,node);ensure(ps>=ns,24);let a=ip(g(p,3,pi));ps=aligned(0 as u64,ps,d(a,0,0));ns=aligned(0 as u64,ns,d(a,0,0));if ps>ns{dyn_release(a,ip(hs(ph,3)),add(hs(ph,4),ns),ps-ns);}}
fn allocate_node(p:i64,node:i64,id:i64){
 valid_id(p,id);let hn=state(p,node);if allocated(p,node)||t(node,7,0)!=0{return;}hw(hn,5,1 as u64);var i:i64=0;
 if can_inplace(ip(t(node,3,0))){while (i as u64)<constant(12){let parent=ip(t(node,9,i));i+=1;if parent==0{continue;}if !own(p,parent){continue;}let view=ip(t(parent,7,0));if (t(parent,4,0)&constant(14))!=0||(view!=0&&(t(view,4,0)&constant(14))!=0){continue;}if !layout(node,parent){continue;}let ph=state(p,parent);
  if hs(ph,0)==1&&hs(ph,1)==0{if view!=0{let vh=state(p,view);if hs(vh,1)==1&&hs(vh,0)==0&&t(view,6,0)==t(parent,6,0){move_addr(hn,ph);hw(ph,5,0 as u64);hw(vh,5,0 as u64);extra(p,node,view);return;}}else{move_addr(hn,ph);hw(ph,5,0 as u64);extra(p,node,parent);return;}}
 }}
 hw(hn,2,id as u64);let out=record(hn,5,0,0);dyn_alloc(ip(g(p,3,id)),buft(ip(g(p,1,id)),2,node),out);
}
fn free_node(p:i64,node:i64){if (t(node,4,0)&constant(14))!=0{return;}let hn=state(p,node);let id=ip(hs(hn,2));valid_id(p,id);dyn_release(ip(g(p,3,id)),ip(hs(hn,3)),hs(hn,4),buft(ip(g(p,1,id)),2,node));hw(hn,5,0 as u64);}
fn buffer_id(ids:i64,i:i64)->i64{return if ids!=0{i32(ids,i)}else{0};}
fn counter(p:i64,k:i64,delta:i64){let v=ip(hs(p,k));ensure((delta>0&&v<2147483647)||(delta<0&&v> -2147483647-1),25);hw(p,k,(v+delta) as! u64);}
fn plan_graph(p:i64,graph:i64,nodeids:i64,leafids:i64){
 if get(ip(g(p,5,0)),9,0,0)>0{hash_reset(p);zero(ip(g(p,6,0)),mul(get(ip(g(p,5,0)),9,0,0),constant(5)));}var i:i64=0;
 while i<ip(get(graph,8,1,0)){allocate_node(p,ip(get(graph,8,3,i)),buffer_id(leafids,i));i+=1;}
 i=0;while i<ip(get(graph,8,0,0)){let node=ip(get(graph,8,2,i));let view=ip(t(node,7,0));if view!=0&&t(node,3,0)!=constant(16){counter(state(p,view),1,1);}if (t(node,4,0)&constant(13))!=0{allocate_node(p,node,buffer_id(nodeids,i));}var j:i64=0;while (j as u64)<constant(12){let src=ip(t(node,9,j));if src!=0{counter(state(p,src),0,1);if (t(src,4,0)&constant(13))!=0{allocate_node(p,src,buffer_id(nodeids,i));}}j+=1;}i+=1;}
 i=0;while i<ip(get(graph,8,0,0)){let node=ip(get(graph,8,2,i));let id=buffer_id(nodeids,i);var j:i64=0;while (j as u64)<constant(12){let src=ip(t(node,9,j));if src!=0{allocate_node(p,src,id);}j+=1;}allocate_node(p,node,id);
  j=0;while (j as u64)<constant(12){let parent=ip(t(node,9,j));if parent!=0{let ph=state(p,parent);counter(ph,0,-1);if hs(ph,0)==0&&hs(ph,1)==0{let view=ip(t(parent,7,0));if view!=0{let vh=state(p,view);counter(vh,1,-1);if hs(vh,1)==0&&hs(vh,0)==0&&hs(vh,5)!=0{free_node(p,view);}}else{if hs(ph,5)!=0{free_node(p,parent);}}}}j+=1;}i+=1;
 }
}
fn save_tensor(p:i64,node:i64,allocation:i64){if node==0||t(node,7,0)!=0||t(node,6,0)!=0{put(allocation,6,0,0,(-1) as! u64);put(allocation,6,1,0,(-1) as! u64);put(allocation,6,2,0,U());put(allocation,6,3,0,0 as u64);return;}let hn=state(p,node);put(allocation,6,0,0,hs(hn,2));put(allocation,6,1,0,hs(hn,3));put(allocation,6,2,0,hs(hn,4));put(allocation,6,3,0,buft(ip(g(p,1,ip(hs(hn,2)))),2,node));}
fn reserve(p:i64,graph:i64,nodeids:i64,leafids:i64,noalloc:bool)->bool{
 let nodes=get(graph,8,0,0);let leafs=get(graph,8,1,0);let count=add(nodes,leafs);let minsize=add(count,count/4);let h=ip(g(p,5,0));
 if get(h,9,0,0)<minsize{
  let size=hash_size(minsize);let bytes=mul(size,8 as u64);let bitbytes=mul(size/32+if size%32!=0{1 as u64}else{0 as u64},4 as u64);let values=mul(size,constant(5));let keys=heap(bytes,0);ensure(keys!=0,26);let used=heap(bitbytes,1);if used==0{release(keys);fail(27);return false;}let state_values=heap(values,0);if state_values==0{release(keys);release(used);fail(28);return false;}
  release(ip(get(h,9,1,0)));release(ip(get(h,9,2,0)));release(ip(g(p,6,0)));put(h,9,0,0,size);put(h,9,1,0,used as u64);put(h,9,2,0,keys as u64);gs(p,6,0,state_values as u64);
 }
 var i:i64=0;while i<ip(g(p,0,0)){dyn_reset(ip(g(p,3,i)));i+=1;}plan_graph(p,graph,nodeids,leafids);
 if g(p,7,0)<nodes{let a=heap(mul(nodes,constant(7)),1);ensure(a!=0,29);release(ip(g(p,8,0)));gs(p,8,0,a as u64);}gs(p,7,0,nodes);
 i=0;while (i as u64)<nodes{let node=ip(get(graph,8,2,i));save_tensor(p,node,record(p,1,i,-1));var j:i64=0;while (j as u64)<constant(12){save_tensor(p,ip(t(node,9,j)),record(p,1,i,j));j+=1;}i+=1;}
 if g(p,9,0)<leafs{let a=heap(mul(leafs,constant(8)),1);ensure(a!=0,30);release(ip(g(p,10,0)));gs(p,10,0,a as u64);}gs(p,9,0,leafs);
 i=0;while (i as u64)<leafs{let leaf=ip(get(graph,8,3,i));state(p,leaf);save_tensor(p,leaf,record(p,2,i,0));i+=1;}
 i=0;while i<ip(g(p,0,0)){var j:i64=0;while j<i{if g(p,3,j)==g(p,3,i){gs(p,2,i,g(p,2,j));break;}j+=1;}let a=ip(g(p,3,i));let b=ip(g(p,2,i));var reallocate=b==0;var chunk:i64=0;var total:u64=0;
  while chunk<ip(d(a,2,0)){let cur=if b!=0{vchunk(b,chunk)}else{0 as u64};let wanted=dyn_size(a,chunk);total=add(total,wanted);if wanted>cur{reallocate=true;}chunk+=1;}
  if reallocate{vfree(b);gs(p,2,i,0 as u64);if !noalloc{let fresh=valloc(ip(g(p,1,i)),a,constant(15) as i64,g(p,4,i));gs(p,2,i,fresh as u64);if fresh==0{return false;}}}i+=1;
 }return true;
}
fn init_tensor(p:i64,node:i64,allocation:i64){
 let id=ip(get(allocation,6,0,0));let off=get(allocation,6,2,0);if t(node,7,0)!=0{if t(node,5,0)==0{let view=ip(t(node,7,0));if t(view,5,0)==0{return;}view_init(node);}return;}
 if t(node,6,0)==0{valid_id(p,id);ensure(off!=U()&&buft(ip(g(p,1,id)),2,node)<=get(allocation,6,3,0),31);let vb=ip(g(p,2,id));ensure(vb!=0,32);vtalloc(vb,node,ip(get(allocation,6,1,0)),off);}
}
fn node_valid(p:i64,node:i64,allocation:i64)->bool{var size:u64=0;if t(node,6,0)==0&&t(node,7,0)==0{let id=ip(get(allocation,6,0,0));if id<0{return false;}valid_id(p,id);size=buft(ip(g(p,1,id)),2,node);}return get(allocation,6,3,0)>=size;}
fn needs(p:i64,graph:i64)->bool{if g(p,7,0)!=get(graph,8,0,0)||g(p,9,0)!=get(graph,8,1,0){return true;}var i:i64=0;while i<ip(get(graph,8,0,0)){let node=ip(get(graph,8,2,i));if !node_valid(p,node,record(p,1,i,-1)){return true;}var j:i64=0;while (j as u64)<constant(12){let src=ip(t(node,9,j));if src!=0&&!node_valid(p,src,record(p,1,i,j)){return true;}j+=1;}i+=1;}return false;}
fn allocate_graph(p:i64,graph:i64)->bool{if needs(p,graph){if g(p,0,0)!=1{return false;}if !reserve(p,graph,0,0,false){return false;}}var i:i64=0;while i<ip(g(p,0,0)){let b=ip(g(p,2,i));if b!=0{vreset(b);}i+=1;}
 i=0;while i<ip(get(graph,8,1,0)){init_tensor(p,ip(get(graph,8,3,i)),record(p,2,i,0));i+=1;}i=0;while i<ip(get(graph,8,0,0)){let node=ip(get(graph,8,2,i));var j:i64=0;while (j as u64)<constant(12){let src=ip(t(node,9,j));if src!=0{init_tensor(p,src,record(p,1,i,j));}j+=1;}init_tensor(p,node,record(p,1,i,-1));i+=1;}return true;}
fn collect(ctx:i64,count_out:i64)->i64{var n:i64=0;var t=first(ctx);while t!=0{ensure(n<2147483647,33);n+=1;t=next(ctx,t);}i32_put(count_out,0,n);if n==0{return 0;}let a=heap(mul(n as u64,8 as u64),0);if a==0{return 0;}t=first(ctx);var i:i64=0;while t!=0{ptr_put(a,i,t as u64);i+=1;t=next(ctx,t);}return a;}
fn context_bulk(ctx:i64,kind:i64,size_only:bool,count_out:i64)->u64{ensure(no_alloc(ctx)!=0,34);let tensors=collect(ctx,count_out);if tensors==0{return 0 as u64;}let result=bulk(kind,tensors,i32(count_out,0),if size_only{1}else{0});release(tensors);return result;}
fn dispatch(p:i64)->u64{
 let mode=ip(arg(p,0));let a=ip(arg(p,1));let b=ip(arg(p,2));let c=ip(arg(p,3));let d0=arg(p,4);let e=arg(p,5);
 if mode==0{return aligned(arg(p,1),arg(p,2),arg(p,3));}
 if mode==1{return if can_inplace(a){1 as u64}else{0 as u64};}
 if mode==2{let base=backend(b,0,0);let align=backend(b,1,0);alignment(align);put(a,0,0,0,b as u64);put(a,0,1,0,base);put(a,0,2,0,align);put(a,0,3,0,aligned(base,0 as u64,align));return 0 as u64;}
 if mode==3{let buffer=ip(get(a,0,0,0));let align=get(a,0,2,0);let size=aligned(0 as u64,backend(buffer,3,b),align);let off=get(a,0,3,0);let cap=backend(buffer,2,0);ensure(off<=cap&&size<=cap-off,35);let address=add(backend(buffer,0,0),off);put(a,0,3,0,off+size);ensure(address%align==0,36);return tensor_alloc(buffer,b,address) as! u64;}
 if mode==4{insert_block(a,arg(p,2),arg(p,3));return 0 as u64;}
 if mode==5{remove_block(a,b);return 0 as u64;}
 if mode==6{return new_chunk(a,arg(p,2)) as! u64;}
 if mode==7{dyn_alloc(a,arg(p,2),c);return 0 as u64;}
 if mode==8{dyn_release(a,b,arg(p,3),d0);return 0 as u64;}
 if mode==9{dyn_reset(a);return 0 as u64;}
 if mode==10{return dyn_new(arg(p,1),arg(p,2)) as u64;}
 if mode==11{dyn_free(a);return 0 as u64;}
 if mode==12{return dyn_size(a,b);}
 if mode==13{vfree(a);return 0 as u64;}
 if mode==14{return vchunk(a,b);}
 if mode==15{return vsize(a);}
 if mode==16{return grow(a,arg(p,2),arg(p,3));}
 if mode==17{return valloc(a,b,c,d0) as u64;}
 if mode==18{vtalloc(a,b,c,d0);return 0 as u64;}
 if mode==19{vreset(a);return 0 as u64;}
 if mode==20{return ga_new(a,b) as u64;}
 if mode==21{limits(a,b);return 0 as u64;}
 if mode==22{ga_free(a);return 0 as u64;}
 if mode==23{return state(a,b) as u64;}
 if mode==24{return if own(a,b){1 as u64}else{0 as u64};}
 if mode==25{return if allocated(a,b){1 as u64}else{0 as u64};}
 if mode==26{extra(a,b,c);return 0 as u64;}
 if mode==27{allocate_node(a,b,c);return 0 as u64;}
 if mode==28{free_node(a,b);return 0 as u64;}
 if mode==29{return buffer_id(a,b) as! u64;}
 if mode==30{plan_graph(a,b,c,ip(d0));return 0 as u64;}
 if mode==31{return if reserve(a,b,c,ip(d0),e!=0){1 as u64}else{0 as u64};}
 if mode==32{ensure(reserve(a,b,c,ip(d0),true),37);var i:i64=0;while i<ip(g(a,0,0)){var sum:u64=0;let talloc=ip(g(a,3,i));var chunk:i64=0;while chunk<ip(d(talloc,2,0)){sum=add(sum,dyn_size(talloc,chunk));chunk+=1;}ptr_put(ip(e),i,sum);i+=1;}return 0 as u64;}
 if mode==33{init_tensor(a,b,c);return 0 as u64;}
 if mode==34{return if node_valid(a,b,c){1 as u64}else{0 as u64};}
 if mode==35{return if needs(a,b){1 as u64}else{0 as u64};}
 if mode==36{return if allocate_graph(a,b){1 as u64}else{0 as u64};}
 if mode==37{valid_id(a,b);let buf=ip(g(a,2,b));if buf==0{return 0 as u64;}var i:i64=0;while i<b{if g(a,2,i)==(buf as u64){return 0 as u64;}i+=1;}return vsize(buf);}
 if mode==38{return collect(a,b) as u64;}
 if mode==39||mode==40{return context_bulk(a,b,mode==40,c);}
 if mode==41{return if (if a!=c{a<c}else{arg(p,2)<d0}){1 as u64}else{0 as u64};}
 fail(99);return 0 as u64;
}
fn main(){print(dispatch(0));}
