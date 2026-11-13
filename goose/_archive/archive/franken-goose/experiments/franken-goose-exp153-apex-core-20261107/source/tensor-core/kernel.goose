extern "fg153_get" fn get(p:i64,k:i64,i:i64)->i64;
extern "fg153_put" fn put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_service" fn service(op:i64,a:i64,b:i64)->i64;
extern "fg153_witness" fn witness(mode:i64);
fn ensure(ok:bool) {if !ok {let ignored=service(10,0,0);assert(false);}}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0);ensure(a==0||b<=9223372036854775807/a);return a*b;}
fn align_bytes(n:i64)->i64 {let alignment=get(0,42,0);return mul(add(n,alignment-1)/alignment,alignment);}
fn zero(p:i64,n:i64) {let ignored=service(7,p,n);}
fn initialize(size:i64,buffer:i64,no_alloc:i64)->i64 {
 let locked=service(0,0,0);
 if get(0,47,0)!=0 {let timed=service(2,0,0);put(0,47,0,0);}
 let unlocked=service(1,0,0);
 let ctx=service(3,get(0,48,0),0);ensure(ctx!=0);zero(ctx,get(0,48,0));
 var bytes=size;if bytes==0 {bytes=get(0,42,0);}if buffer==0 {bytes=align_bytes(bytes);}
 var storage=buffer;if buffer==0 {storage=service(4,bytes,0);}
 ensure(storage!=0&&storage%get(0,42,0)==0);
 put(ctx,1,0,bytes);put(ctx,2,0,storage);var owned:i64=0;if buffer==0 {owned=1;}put(ctx,3,0,owned);put(ctx,4,0,no_alloc);return ctx;
}
fn reset(ctx:i64) {if ctx!=0 {put(ctx,5,0,0);put(ctx,6,0,0);put(ctx,7,0,0);}}
fn release(ctx:i64) {if ctx!=0 {if get(ctx,3,0)!=0 {let ignored=service(6,get(ctx,2,0),get(ctx,1,0));}let ignored=service(5,ctx,0);}}
fn used(ctx:i64)->i64 {let last=get(ctx,7,0);if last==0 {return 0;}return add(get(last,10,0),get(last,11,0));}
fn object(ctx:i64,kind:i64,size:i64)->i64 {
 let start=used(ctx);let needed=align_bytes(size);let overhead=get(0,40,0);
 if start>9223372036854775807-needed {return 0;}
 let end=start+needed;if end>9223372036854775807-overhead||end+overhead>get(ctx,1,0) {let warned=service(8,ctx,size);return 0;}
 let obj=add(get(ctx,2,0),start);zero(obj,overhead);
 put(obj,10,0,add(start,overhead));put(obj,11,0,needed);put(obj,13,0,kind);
 let last=get(ctx,7,0);if last!=0 {put(last,12,0,obj);}else {put(ctx,6,0,obj);}put(ctx,7,0,obj);return obj;
}
fn row_size(kind:i64,ne:i64)->i64 {ensure(kind>=0&&kind<get(0,43,0));let block_size=get(0,31,kind);ensure(block_size>0&&ne>=0&&ne%block_size==0);return mul(get(0,30,kind),ne)/block_size;}
fn bytes(t:i64)->i64 {
 for axis in 4 {if get(t,26,axis)<=0 {return 0;}}
 let kind=get(t,20,0);let block_size=get(0,31,kind);var value:i64=0;
 if block_size==1 {value=get(0,30,kind);for axis in 4 {value=add(value,mul(get(t,26,axis)-1,get(t,27,axis)));}}
 else {value=mul(get(t,26,0),get(t,27,0))/block_size;for axis in 3 {value=add(value,mul(get(t,26,axis+1)-1,get(t,27,axis+1)));}}
 return value;
}
fn tensor(ctx:i64,kind:i64,dims:i64,shape:i64,source:i64,offset:i64)->i64 {
 ensure(kind>=0&&kind<get(0,43,0)&&dims>=1&&dims<=4);
 var view=source;var offs=offset;
 if view!=0&&get(view,23,0)!=0 {offs=add(offs,get(view,24,0));view=get(view,23,0);}
 var elements=get(shape,32,0);ensure(elements>=0);
 for axis in dims-1 {let n=get(shape,32,axis+1);ensure(n>=0);if n>1 {ensure(9223372036854775807/n>elements);elements=mul(elements,n);}}
 var size=row_size(kind,get(shape,32,0));for axis in dims-1 {size=mul(size,get(shape,32,axis+1));}
 if view!=0&&size!=0 {ensure(offs>=0&&size<=bytes(view)&&offs<=bytes(view)-size);}
 var data:i64=0;if view!=0 {data=get(view,25,0);if data!=0 {data=add(data,offs);}}
 var allocated:i64=0;if view==0&&get(ctx,4,0)==0 {allocated=size;}
 let obj=object(ctx,get(0,44,0),add(get(0,41,0),allocated));ensure(obj!=0);
 let result=add(get(ctx,2,0),get(obj,10,0));zero(result,get(0,41,0));
 put(result,20,0,kind);put(result,23,0,view);put(result,24,0,offs);
 if allocated>0 {data=add(result,get(0,41,0));}put(result,25,0,data);
 for axis in 4 {put(result,26,axis,1);}
 for axis in dims {put(result,26,axis,get(shape,32,axis));}
 put(result,27,0,get(0,30,kind));put(result,27,1,mul(get(result,27,0),get(result,26,0)/get(0,31,kind)));
 for axis in 2 {put(result,27,axis+2,mul(get(result,27,axis+1),get(result,26,axis+1)));}
 put(ctx,5,0,add(get(ctx,5,0),1));return result;
}
fn find(ctx:i64,obj:i64,name:i64)->i64 {
 var current=obj;while current!=0 {if get(current,13,0)==get(0,44,0) {let t=add(get(ctx,2,0),get(current,10,0));if name==0||service(9,t,name)!=0 {return t;}}current=get(current,12,0);}return 0;
}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
 witness(mode);
 if mode==0 {return initialize(a,b,c);}if mode==1 {reset(a);return 0;}if mode==2 {release(a);return 0;}
 if mode==3 {return used(a);}if mode==4 {return object(a,b,c);}if mode==5 {return tensor(a,b,c,d,e,f);}
 if mode==6 {return find(a,get(a,6,0),0);}if mode==7 {ensure(b>=get(0,40,0));return find(a,get(b-get(0,40,0),12,0),0);}
 if mode==8 {return bytes(a);}if mode==9||mode==10 {var count:i64=1;var axis:i64=0;if mode==10 {axis=1;}while axis<4 {count=mul(count,get(a,26,axis));axis+=1;}return count;}
 if mode==11 {var largest:i64=0;var t=find(a,get(a,6,0),0);while t!=0 {let size=bytes(t);if size>largest {largest=size;}t=find(a,get(t-get(0,40,0),12,0),0);}return largest;}
 if mode==12 {return find(a,get(a,6,0),b);}if mode==13 {return row_size(a,b);}ensure(false);return 0;
}
fn main(){print(entry(3,0,0,0,0,0,0));}
