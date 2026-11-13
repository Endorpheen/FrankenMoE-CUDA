// stream-io.goose - owned byte/tensor bounds, accounting and ordering for the
// host/dummy/file concrete IO classes. Byte transfers and allocation are typed
// primitives (services.cpp); host destructor flush stays a transfer primitive.
// Source anchors: native/src/llama-context.cpp:2877-3060.
extern "fg153_stream_get" fn sg(kind:i64,s:i64,k:i64,idx:i64)->i64;
extern "fg153_stream_act" fn sa(kind:i64,s:i64,op:i64,a:i64,b:i64,c:i64,d:i64)->i64;
fn add(a:i64,b:i64)->i64 {return a+b;}
fn dummy_write(s:i64,size:i64)->i64 {let ignored=sa(0,s,13,size,0,0,0);return 1;}
fn dummy_tensor(s:i64,size:i64)->i64 {
 if sg(0,s,2,0)!=0 {return 1;}
 let ignored=sa(0,s,13,size,0,0,0);return 1;
}
fn hostw_write(s:i64,src:i64,size:i64)->i64 {
 let bs=sg(1,s,0,0);
 if size>bs {return 0;}
 let ignored=sa(1,s,0,src,size,0,0); // memcpy at current ptr
 let ignored2=sa(1,s,1,0,size,0,0); // advance ptr/written/buf_size
 return 1;
}
fn hostw_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {
 let bs=sg(1,s,0,0);
 if size>bs {return 0;}
 let ignored=sa(1,s,2,t,size,off,0); // append (ptr/offset recorded)
 let ignored2=sa(1,s,1,0,size,0,0);
 return 1;
}
fn hostr_read(s:i64,dst:i64,size:i64)->i64 {
 let bs=sg(2,s,0,0);
 if size>bs {return 0;}
 let ignored=sa(2,s,3,dst,size,0,0);
 let ignored2=sa(2,s,4,0,size,0,0);
 return 1;
}
fn hostr_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {
 let bs=sg(2,s,0,0);
 if size>bs {return 0;}
 let ignored=sa(2,s,5,t,size,off,0);
 let ignored2=sa(2,s,4,0,size,0,0);
 return 1;
}
fn hostr_discard(s:i64)->i64 {let ignored=sa(2,s,6,0,0,0,0);return 1;}
fn filew_write(s:i64,src:i64,size:i64)->i64 {
 let ignored=sa(3,s,7,src,size,0,0);
 let ignored2=sa(3,s,19,size,0,0,0);
 return 1;
}
fn filew_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {
 let ignored=sa(3,s,18,size,0,0,0);   // temp_buffer.resize(size)
 let ignored2=sa(3,s,14,t,off,size,0); // tensor_get into temp
 let ignored3=sa(3,s,17,size,0,0,0);  // write_raw(temp,size)
 let ignored4=sa(3,s,19,size,0,0,0);  // size_written += size
 return 1;
}
fn filer_read(s:i64,dst:i64,size:i64)->i64 {
 let ignored=sa(4,s,8,dst,size,0,0);
 let ignored2=sa(4,s,20,size,0,0,0);
 return 1;
}
fn filer_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {
 let ignored=sa(4,s,15,size,0,0,0);    // temp resize + read_raw
 let ignored2=sa(4,s,16,t,off,size,0); // tensor_set from temp
 let ignored3=sa(4,s,20,size,0,0,0);
 return 1;
}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 if mode==0 {return dummy_write(a,b);}
 if mode==1 {return dummy_tensor(a,b);}
 if mode==2 {return hostw_write(a,b,c);}
 if mode==3 {return hostw_tensor(a,b,c,d);}
 if mode==4 {return hostr_read(a,b,c);}
 if mode==5 {return hostr_tensor(a,b,c,d);}
 if mode==6 {return hostr_discard(a);}
 if mode==7 {return filew_write(a,b,c);}
 if mode==8 {return filew_tensor(a,b,c,d);}
 if mode==9 {return filer_read(a,b,c);}
 if mode==10 {return filer_tensor(a,b,c,d);}
 if mode==11 {return 1;}
 return 0;
}
fn main(){print(entry(11,0,0,0,0,0));}
