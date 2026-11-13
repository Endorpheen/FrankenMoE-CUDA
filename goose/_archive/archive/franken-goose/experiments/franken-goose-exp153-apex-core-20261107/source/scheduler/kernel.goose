extern "fg153_sg" fn read(p:i64,k:i64,i:i64)->i64;
extern "fg153_sp" fn write(p:i64,k:i64,i:i64,v:i64);
extern "fg153_ss" fn call(op:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64;
extern "fg153_sw" fn witness(mode:i64);
fn ensure(ok:bool){if !ok {let unused0=call(0,0,0,0,0,0);assert(false);}}
fn heap(n:i64)->i64 {ensure(n>=0);let p=call(1,n,0,0,0,0);ensure(p!=0);return p;}
fn drop(p:i64){let unused1=call(2,p,0,0,0,0);}
fn hash(s:i64,t:i64)->i64 {return call(3,s,t,0,0,0);}
fn id(s:i64,t:i64)->i64 {return read(s,7,hash(s,t));}
fn setid(s:i64,t:i64,b:i64){write(s,7,hash(s,t),b);}
fn backend(s:i64,b:i64)->i64 {ensure(b>=0&&b<read(s,2,0));return read(s,3,b);}
fn copies(s:i64,t:i64,b:i64,c:i64)->i64 {return read(s,8,(hash(s,t)*read(s,2,0)+b)*read(s,18,0)+c);}
fn setcopy(s:i64,t:i64,b:i64,c:i64,v:i64){write(s,8,(hash(s,t)*read(s,2,0)+b)*read(s,18,0)+c,v);}
fn split(s:i64,i:i64)->i64{return read(s,15,i);}
fn event(s:i64,b:i64)->i64{return read(s,21,b*read(0,403,0)+read(s,19,0));}
fn view(t:i64)->bool{return call(4,read(t,103,0),0,0,0,0)!=0;}
fn hostweight(t:i64)->bool {let b=read(t,102,0);return b!=0&&read(b,110,0)==read(0,404,0)&&call(5,b,0,0,0,0)!=0;}
fn bufferfrom(s:i64,t:i64,op:i64)->i64 {
 var t0=t;if read(t,101,0)!=0 {t0=read(t,101,0);}let buf=read(t0,102,0);if buf==0{return -1;}
 for b in read(s,2,0) {if call(6,backend(s,b),read(buf,111,0),0,0,0)!=0&&call(7,backend(s,b),op,0,0,0)!=0 {return b;}}return -1;
}
fn current(s:i64,t:i64)->i64 {
 var b=bufferfrom(s,t,t);if b>=0{return b;}let vs=read(t,101,0);if vs!=0{b=bufferfrom(s,vs,t);if b>=0{return b;}}
 ensure(read(t,102,0)==0&&(vs==0||read(vs,102,0)==0));
 if (read(t,104,0)&read(0,405,0))!=0{return read(s,2,0)-1;}
 if read(t,103,0)!=read(0,406,0)&&read(t,103,0)!=read(0,407,0){
  for j in read(0,402,0){let src=read(t,105,j);if src!=0{let buf=read(src,102,0);if buf!=0&&read(buf,110,0)==read(0,404,0){
   let sb=bufferfrom(s,src,t);if read(s,31,0)!=0&&sb==read(s,2,0)-1&&call(5,buf,0,0,0,0)!=0{
    for k in sb{if call(7,backend(s,k),t,0,0,0)!=0&&call(8,backend(s,k),t,0,0,0)!=0{return k;}}
   }return sb;
  }}}
 }return -1;
}
fn supported(s:i64,t:i64,b:i64)->bool {
 var t0=t;if read(t,101,0)!=0{t0=read(t,101,0);}let buf=read(t0,102,0);var bt:i64=0;
 if buf!=0{bt=read(buf,111,0);}else{var tb=id(s,t);if tb<0&&read(t,101,0)!=0{tb=id(s,read(t,101,0));}if tb>=0{bt=read(s,4,tb);}}
 return bt!=0&&call(6,backend(s,b),bt,0,0,0)!=0;
}
fn expansion(s:i64,g:i64,reverse:bool,gpu:bool){
 var b:i64=-1;let n=read(g,200,0);for k in n{var i=k;if reverse{i=n-1-k;}let t=read(g,202,i);if !view(t){let tb=id(s,t);if tb>=0{b=tb;if gpu&&tb==read(s,2,0)-1{b=-1;}}else{if b>=0&&call(7,backend(s,b),t,0,0,0)!=0{setid(s,t,b);}}}}
}
fn append(s:i64,t:i64,b:i64,leaf:bool){let g=read(s,14,0);var count=read(g,200,0);var field:i64=202;var ids:i64=9;if leaf{count=read(g,201,0);field=203;ids=10;}ensure(count<read(g,204,0));write(s,ids,count,b);write(g,field,count,t);if leaf{write(g,201,0,count+1);}else{write(g,200,0,count+1);}}
fn duplicate(s:i64,t:i64,b:i64,c:i64)->i64 {let result=call(9,read(s,25,0),t,backend(s,b),c,0);ensure(result!=0);return result;}
fn flags(t:i64){write(t,104,0,read(t,104,0)|read(0,405,0)|read(0,408,0));}
fn resize(p:i64,bytes:i64)->i64{let result=call(53,p,bytes,0,0,0);ensure(result!=0);return result;}
fn growsplits(s:i64){let old=read(s,17,0);ensure(old>0&&old<=1073741823);let cap=old*2;let bytes=read(0,423,0);let ptr=resize(read(s,15,-1),cap*bytes);let unused200=call(54,ptr+old*bytes,(cap-old)*bytes,0,0,0);write(s,15,-1,ptr);write(s,17,0,cap);}
fn growinputs(p:i64,splitinput:bool){var capfield:i64=24;var ptrfield:i64=22;if splitinput{capfield=305;ptrfield=303;}let old=read(p,capfield,0);ensure(old>=0&&old<=1073741823);var cap:i64=30;if old>0{cap=old*2;}let ptr=resize(read(p,ptrfield,-1),cap*8);write(p,ptrfield,-1,ptr);write(p,capfield,0,cap);}
fn growgraph(s:i64,size:i64){let g=read(s,14,0);if read(g,204,0)<size{write(g,202,-1,resize(read(g,202,-1),size*8));write(g,203,-1,resize(read(g,203,-1),size*8));write(g,204,0,size);}}
fn schedule(s:i64,g:i64){
 write(s,16,0,0);write(s,23,0,0);write(s,0,0,0);let unused2=call(10,s,0,0,0,0);write(g,205,0,call(11,0,0,0,0,0));
 for i in read(g,201,0){let t=read(g,203,i);if id(s,t)<0{setid(s,t,current(s,t));}}
 for i in read(g,200,0){let t=read(g,202,i);if id(s,t)<0{setid(s,t,current(s,t));}}
 expansion(s,g,false,true);expansion(s,g,true,true);expansion(s,g,false,false);expansion(s,g,true,false);
 for i in read(g,200,0){let t=read(g,202,i);if !view(t){var tb=id(s,t);
  if tb<0{var best:i64=-1;for b in read(s,2,0){if call(7,backend(s,b),t,0,0,0)!=0{var count:i64=0;for j in read(0,402,0){let src=read(t,105,j);if src!=0{var assigned=id(s,src)>=0;let vs=read(src,101,0);if vs!=0&&id(s,vs)>=0{assigned=true;}if assigned&&supported(s,src,b){count+=1;}}}if count>best{best=count;tb=b;setid(s,t,b);}}}}
  else{for b in tb{if read(s,4,b)==read(s,4,tb)&&call(7,backend(s,b),t,0,0,0)!=0{var ok=true;for j in read(0,402,0){let src=read(t,105,j);if src!=0&&!supported(s,src,b){ok=false;break;}}if ok{setid(s,t,b);break;}}}}
 }}
 for i in read(g,200,0){let t=read(g,202,i);let vs=read(t,101,0);if vs!=0&&id(s,t)<0{setid(s,t,id(s,vs));}for j in read(0,402,0){let src=read(t,105,j);if src!=0&&id(s,src)<0{let v=read(src,101,0);if v!=0{setid(s,src,id(s,v));}else{setid(s,src,id(s,t));}}}for b in read(s,2,0){if id(s,t)<0&&call(7,backend(s,b),t,0,0,0)!=0{setid(s,t,b);}}ensure(id(s,t)>=0);}
 var si:i64=0;var sp=split(s,0);var first:i64=0;let nodes=read(g,200,0);
 while first<nodes&&view(read(g,202,first)){first+=1;}if first<nodes{write(sp,300,0,id(s,read(g,202,first)));}write(sp,301,0,0);write(sp,304,0,0);var b=read(sp,300,0);var ni=first;
 while ni<nodes{let t=read(g,202,ni);if !view(t){let tb=id(s,t);ensure(tb>=0);var fresh=false;
  if tb==b&&read(sp,304,0)>0{for j in read(0,402,0){let src=read(t,105,j);if src!=0{let buf=read(src,102,0);if buf!=0&&read(buf,110,0)==read(0,404,0)&&id(s,src)!=b&&!supported(s,src,b){fresh=true;break;}}}}
  if tb!=b||fresh{write(sp,302,0,ni);si+=1;if si>=read(s,17,0){growsplits(s);}sp=split(s,si);write(sp,300,0,tb);write(sp,301,0,ni);write(sp,304,0,0);b=tb;}
  for j in read(0,402,0){let src=read(t,105,j);if src!=0{let sb=id(s,src);ensure(sb>=0);if sb!=b&&!supported(s,src,b){if copies(s,src,b,0)==0{for c in read(s,18,0){let cp=duplicate(s,src,b,c);if read(s,18,0)>1{flags(cp);}setcopy(s,src,b,c,cp);}let n=read(sp,304,0);if n>=read(sp,305,0){growinputs(sp,true);}write(sp,303,n,src);write(sp,304,0,n+1);}write(t,105,j,copies(s,src,b,read(s,19,0)));}}}
 }ni+=1;}
 write(sp,302,0,nodes);write(s,16,0,si+1);if read(s,32,0)!=0{let unused5=call(14,s,g,0,0,0);}
 if read(s,18,0)>1{for i in read(g,201,0){let t=read(g,203,i);if (read(t,104,0)&read(0,405,0))!=0{let tb=id(s,t);ensure(tb>=0);if copies(s,t,tb,0)==0{for c in read(s,18,0){var cp=t;if c!=read(s,19,0){cp=duplicate(s,t,tb,c);}flags(cp);setcopy(s,t,tb,c,cp);}}let n=read(s,23,0);if n>=read(s,24,0){growinputs(s,false);}write(s,22,n,t);write(s,23,0,n+1);}}}
 let temp=read(s,9,-1);write(s,9,-1,read(s,11,-1));write(s,11,-1,temp);let temp2=read(s,10,-1);write(s,10,-1,read(s,12,-1));write(s,12,-1,temp2);
 let deps=call(16,0,0,0,0,0);ensure(deps!=0);
 for i in read(s,16,0){let sp0=split(s,i);let unused7=call(17,sp0,g,read(sp0,301,0),read(sp0,302,0),0);let unused8=call(18,backend(s,read(sp0,300,0)),read(sp0,306,0),deps,0,0);}
 var depcount:i64=0;for i in call(19,deps,0,0,0,0){let count=call(20,deps,i,0,0,0);depcount+=(count+read(0,402,0)-1)/read(0,402,0);}
 var inputs=read(s,23,0);for i in read(s,16,0){inputs+=read(split(s,i),304,0);}var size=read(g,200,0);if read(g,201,0)>size{size=read(g,201,0);}size+=inputs*2*read(s,18,0)+depcount;
 write(s,34,0,read(s,33,0));write(s,33,0,size);growgraph(s,size);let gc=read(s,14,0);write(gc,200,0,0);write(gc,201,0,0);var added:i64=0;
 for i in read(s,16,0){let sp0=split(s,i);let b0=read(sp0,300,0);for j in read(sp0,304,0){let src=read(sp0,303,j);let cp=copies(s,src,b0,read(s,19,0));let dep=call(22,read(s,25,0),src,0,0,0);write(dep,105,0,src);append(s,dep,id(s,src),false);append(s,cp,b0,false);}
  var j=read(sp0,301,0);while j<read(sp0,302,0){let t=read(g,202,j);append(s,t,id(s,t),false);let di=call(23,deps,t,0,0,0);if di>=0{let count=call(20,deps,di,0,0,0);var k:i64=0;while k<count{let kept=call(24,deps,di,k,0,0);let dep=call(22,read(s,25,0),kept,0,0,0);var src:i64=0;while src<read(0,402,0)&&k+src<count{write(dep,105,src,call(24,deps,di,k+src,0,0));src+=1;}append(s,dep,b0,false);added+=1;k+=read(0,402,0);}}j+=1;}
 }ensure(added==depcount);let unused10=call(25,deps,0,0,0,0);
 if read(s,18,0)>1{for i in read(s,23,0){let t=read(s,22,i);let b0=id(s,t);for c in read(s,18,0){append(s,copies(s,t,b0,c),b0,true);}}for i in read(s,16,0){let sp0=split(s,i);let b0=read(sp0,300,0);for j in read(sp0,304,0){let t=read(sp0,303,j);for c in read(s,18,0){append(s,copies(s,t,b0,c),b0,true);}}}}
 for i in read(g,201,0){let t=read(g,203,i);append(s,t,id(s,t),true);}for i in read(s,16,0){write(read(split(s,i),306,0),205,0,call(11,0,0,0,0,0));}
}
fn pipeline(s:i64)->i64 {
 if read(s,28,0)==0||read(s,26,0)!=0||read(s,29,0)==0||read(s,18,0)!=1{return 0;}
 let nb=read(s,2,0);let sizes=heap(nb*8);let ready=heap(nb*8);let slots=heap(nb*8);var nc:i64=0;for i in read(s,16,0){nc+=read(split(s,i),304,0);}let candidates=heap((nc+1)*24);var count:i64=0;
 for b in nb{write(s,36,b,call(26,backend(s,b),0,0,0,0));}
 for i in read(s,16,0){let sp=split(s,i);let b=read(sp,300,0);let g=read(sp,306,0);if read(s,36,b)!=0&&read(g,200,0)>0{let t=read(g,202,0);let ids=read(t,105,2);if read(t,103,0)==read(0,409,0)&&ids!=0&&read(ids,106,1)>=32{for j in read(sp,304,0){let src=read(sp,303,j);let dst=copies(s,src,b,read(s,19,0));if dst==read(t,105,0)&&hostweight(src){write(s,37,0,0);if call(27,s,b,read(0,410,0),src,dst)!=0{let n=read(s,37,0);if n>read(sizes,400,b){write(sizes,400,b,n);}write(candidates,400,count*3,b);write(candidates,400,count*3+1,src);write(candidates,400,count*3+2,dst);count+=1;}}}}}}
 for b in nb{if read(s,36,b)!=0{write(s,37,0,read(sizes,400,b));write(ready,400,b,call(27,s,b,read(0,411,0),0,0));}}
 var changed:i64=0;for i in count{let b=read(candidates,400,i*3);if read(ready,400,b)!=0{write(s,37,0,read(slots,400,b));ensure(call(27,s,b,read(0,412,0),read(candidates,400,i*3+1),read(candidates,400,i*3+2))!=0);write(slots,400,b,read(s,37,0));changed=1;}}
 drop(candidates);drop(slots);drop(ready);drop(sizes);return changed;
}
fn allocate(s:i64,expert:i64)->i64 {
 let g=read(s,14,0);var changed=false;
 for group in 2{var n=read(g,200,0);var field:i64=9;var prev:i64=11;if group==1{n=read(g,201,0);field=10;prev=12;}if !changed{for i in n{let b=read(s,field,i);let old=read(s,prev,i);ensure(b>=0&&b<read(s,2,0));if b!=old&&(old<0||old>=read(s,2,0)||read(s,4,b)!=read(s,4,old)){changed=true;break;}}}}
 var ok=false;if expert==0&&!changed{ok=call(28,s,0,0,0,0)!=0;}
 if !ok{let different=read(s,34,0)!=read(s,33,0);let unused11=call(29,s,if changed {1}else{0},if different{1}else{0},0,0);let debug=read(s,35,0);ensure(debug==0||(debug==1&&(changed||different)));for b in read(s,2,0){let unused12=call(30,backend(s,b),0,0,0,0);}if call(31,s,0,0,0,0)==0{return 0;}if call(28,s,0,0,0,0)==0{return 0;}}
 return 1;
}
fn copyinput(s:i64,sp:i64,t:i64){
 let b=read(sp,300,0);let dst=backend(s,b);let sb=backend(s,id(s,t));let cp=copies(s,t,b,read(s,19,0));let ev=event(s,b);
 if (read(t,104,0)&read(0,405,0))!=0{if ev!=0{let unused13=call(32,ev,0,0,0,0);}else{let unused14=call(30,dst,0,0,0,0);}let unused15=call(33,t,cp,0,0,0);return;}
 if read(s,36,b)!=0&&read(s,29,0)!=0&&hostweight(t)&&call(27,s,b,read(0,413,0),t,cp)!=0{let copied=call(34,s,sp,t,cp,0);var op=read(0,415,0);if copied!=0{op=read(0,414,0);}let unused16=call(27,s,b,op,t,cp);if copied!=0{return;}}
 if ev!=0{let unused17=call(35,dst,ev,0,0,0);}else{let unused18=call(30,dst,0,0,0,0);}
 if read(s,29,0)!=0&&hostweight(t)&&call(34,s,sp,t,cp,0)!=0{return;}
 if call(36,sb,dst,t,cp,0)==0{let unused19=call(30,sb,0,0,0,0);if ev!=0{let unused20=call(32,ev,0,0,0,0);}else{let unused21=call(30,dst,0,0,0,0);}let unused22=call(33,t,cp,0,0,0);}
}
fn compute(s:i64)->i64{
 var prev:i64=-1;for i in read(s,16,0){let sp=split(s,i);let b=read(sp,300,0);let be=backend(s,b);let g=read(sp,306,0);
  if read(sp,304,0)==0&&prev>=0&&prev!=b{let ev=event(s,prev);if ev!=0{let unused23=call(32,ev,0,0,0,0);}else{let unused24=call(30,backend(s,prev),0,0,0,0);}}
  for phase in 2{for j in read(sp,304,0){let t=read(sp,303,j);if (phase==1&&hostweight(t))||(phase==0&&!hostweight(t)){copyinput(s,sp,t);}}}
  if read(s,26,0)==0{let ec=call(37,be,g,0,read(g,200,0),0);if ec!=read(0,416,0){return ec;}}
  else{var j:i64=0;while j<read(g,200,0){var t=read(g,202,j);var need=call(38,s,t,1,0,0)!=0;var end=j;while !need&&end<read(g,200,0)-1{end+=1;t=read(g,202,end);need=call(38,s,t,1,0,0)!=0;}let ec=call(37,be,g,j,end+1,0);if ec!=read(0,416,0){return ec;}let unused25=call(30,be,0,0,0,0);if need&&call(38,s,t,0,0,0)==0{break;}j=end+1;}}
  let ev=event(s,b);if ev!=0{let unused26=call(39,ev,be,0,0,0);}prev=b;
 }return read(0,416,0);
}
fn reset(s:i64){ensure(s!=0);if read(s,0,0)==0{let unused27=call(40,s,0,0,0,0);for i in read(s,6,0){write(s,7,i,-1);}for i in read(s,6,0)*read(s,2,0)*read(s,18,0){write(s,8,i,0);}write(s,0,0,1);}write(s,1,0,0);}
fn synchronize(s:i64){ensure(s!=0);for b in read(s,2,0){let unused28=call(30,backend(s,b),0,0,0,0);}if read(s,1,0)==0{write(s,20,0,0);}}
fn allocgraph(s:i64,g:i64)->i64{ensure(s!=0&&read(s,6,0)>=read(g,200,0)+read(g,201,0)&&read(s,1,0)==0);write(s,19,0,read(s,20,0));write(s,20,0,(read(s,20,0)+1)%read(s,18,0));schedule(s,g);let expert=pipeline(s);if allocate(s,expert)==0{return 0;}write(s,1,0,1);return 1;}
fn asynchronous(s:i64,g:i64)->i64{ensure(s!=0);if read(s,0,0)==0&&read(s,1,0)==0{reset(s);}if read(s,1,0)==0&&allocgraph(s,g)==0{return read(0,417,0);}return compute(s);}
fn destroy(s:i64){if s==0{return;}for b in read(s,2,0){for c in read(s,18,0){let unused29=call(41,read(s,21,b*read(0,403,0)+c),0,0,0,0);}}let unused30=call(42,read(s,5,0),0,0,0,0);let unused31=call(43,read(s,25,0),0,0,0,0);let unused32=call(44,s,0,0,0,0);for i in read(s,17,0){drop(read(split(s,i),303,-1));}for k in 11{drop(read(s,420,k));}drop(s);}
fn initialize(s:i64,bes:i64,bts:i64){for b in read(s,2,0){let be=read(bes,400,b);write(s,3,b,be);var bt:i64=0;if bts!=0{bt=read(bts,400,b);}else{bt=call(45,be,0,0,0,0);}write(s,4,b,bt);ensure(call(6,be,bt,0,0,0)!=0);if read(s,18,0)>1{for c in read(s,18,0){write(s,21,b*read(0,403,0)+c,call(46,be,0,0,0,0));}}}write(s,5,0,call(47,s,0,0,0,0));reset(s);}
fn product(a:i64,b:i64)->i64{ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
fn create(args:i64)->i64{
 let bes=read(args,400,0);let bts=read(args,400,1);let nb=read(args,400,2);let size=read(args,400,3);ensure(nb>0&&nb<=read(0,424,0)&&size>=0);ensure(call(52,read(bes,400,nb-1),0,0,0,0)!=0);
 let s=heap(read(0,421,0));write(s,2,0,nb);var nc:i64=1;if read(args,400,4)!=0{nc=read(0,403,0);}write(s,18,0,nc);write(s,32,0,call(51,0,0,0,0,0));write(s,35,0,call(51,1,0,0,0,0));
 let unused100=call(49,s,size,0,0,0);let hs=read(s,6,0);write(s,7,-1,heap(product(hs,4)));write(s,8,-1,heap(product(product(product(hs,nb),nc),8)));
 let maxinputs=read(0,425,0);let nodes=product(size,1+maxinputs*2);for k in 4{write(s,9+k,-1,heap(product(nodes,4)));}let context=product(product(size,maxinputs*2),read(0,422,0));let overhead=call(50,size,0,0,0,0);ensure(context<=9223372036854775807-overhead);write(s,39,0,context+overhead);write(s,38,0,heap(context+overhead));
 write(s,17,0,16);write(s,15,-1,heap(product(16,read(0,423,0))));write(s,24,0,maxinputs);write(s,22,-1,heap(product(maxinputs,8)));write(s,31,0,read(args,400,5));initialize(s,bes,bts);return s;
}
fn entry(mode:i64,s:i64,a:i64,b:i64)->i64{
 witness(mode);if mode==13{return create(s);}if mode==0{schedule(s,a);return 0;}if mode==1{return pipeline(s);}if mode==2{return allocate(s,a);}if mode==3{copyinput(s,a,b);return 0;}if mode==4{return compute(s);}if mode==5{reset(s);return 0;}if mode==6{synchronize(s);return 0;}if mode==7{destroy(s);return 0;}if mode==8{return allocgraph(s,a);}if mode==9{return asynchronous(s,a);}if mode==10||mode==11{ensure(s!=0&&read(s,6,0)>=read(a,200,0)+read(a,201,0));if mode==11{reset(s);}synchronize(s);schedule(s,a);if mode==11{return call(48,s,b,0,0,0);}if call(31,s,0,0,0,0)==0{return 0;}reset(s);return 1;}if mode==12{initialize(s,a,b);return s;}ensure(false);return 0;
}
fn main(){print(entry(-1,0,0,0));}
