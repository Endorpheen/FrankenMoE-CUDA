// common-memory.goose - status policy, state-IO string format and memory buffer
// collection for the common Apex memory layer.
// Source anchors: inputs/llama-memory.cpp:9-92, inputs/llama-io.cpp:5-20.
// Owned decisions (Goose): status_combine/is_fail, u32 string length format and
// truncation, sort/unique buffer collection. Byte/tensor transfers, storage and
// allocation are typed primitives (services.cpp).
extern "fg153_cg" fn cg(p:i64,k:i64,i:i64)->i64;
extern "fg153_cp" fn cp(p:i64,k:i64,i:i64,v:i64);
extern "fg153_cs" fn cs(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_cw" fn cw(mode:i64);
fn status_combine(s0:i64,s1:i64)->i64 {
 var has_update=false;
 if s0==2||s0==3 {return s0;}
 if s0==0 {has_update=true;}
 if s1==2||s1==3 {return s1;}
 if s1==0 {has_update=true;}
 return if has_update {0}else{1};
}
fn status_is_fail(s:i64)->i64 {if s==0||s==1 {return 0;}if s==2||s==3 {return 1;}return 0;}
fn write_string(io:i64,str:i64)->i64 {
 let n=cs(1,str,0,0);
 let len=n&4294967295;
 let ok=cs(2,io,len,0);
 if ok==0 {return 0;}
 return cs(3,io,str,len);
}
fn read_string(io:i64,str:i64)->i64 {
 let ok=cs(4,io,0,0);
 if ok==0 {return 0;}
 let len=cs(7,io,0,0);
 if len==0 {return cs(6,str,0,0);}
 return cs(5,io,len,str);
}
// std::set<ggml_backend_buffer_t> semantics: unique, ascending (signed pointer
// compare is identical for the user-space addresses used here).
fn collect(v:i64)->i64 {
 let n=cs(25,v,0,0);
 let out=cs(20,64,0,0);
 var count:i64=0;
 var i:i64=0;
 while i<n {
  let b=cs(26,v,i,0);
  var pos:i64=count;
  var found=false;
  var j:i64=0;
  while j<count {
   let e=cg(out,10,j);
   if e==b {found=true;}
   else {if e>b {if pos==count {pos=j;}}}
   j=j+1;
  }
  if !found {
   var k:i64=count;
   while k>pos {cp(out,10,k,cg(out,10,k-1));k=k-1;}
   cp(out,10,pos,b);
   count=count+1;
  }
  i=i+1;
 }
 cp(out,11,0,count);
 cp(out,12,0,cs(24,v,0,0));
 return out;
}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 cw(mode);
 if mode==0 {return status_combine(a,b);}
 if mode==1 {return status_is_fail(a);}
 if mode==2 {return write_string(a,b);}
 if mode==3 {return read_string(a,b);}
 if mode==4 {return collect(a);}
 if mode==9 {return 2147483647;}
 cs(0,0,0,0);return 0;
}
fn main(){print(entry(9,0,0,0,0,0));}
