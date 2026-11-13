extern "fg153_bc_backend_has" fn bc_backend_has(b:i64,code:i64)->i64;
extern "fg153_bc_backend_call" fn bc_backend_call(be:i64,code:i64,a0:i64,a1:i64,a2:i64,a3:i64,a4:i64)->i64;
extern "fg153_bc_backend_device" fn bc_backend_device(b:i64)->i64;
extern "fg153_bc_backend_name" fn bc_backend_name(b:i64)->i64;
extern "fg153_bc_dev_has" fn bc_dev_has(d:i64,code:i64)->i64;
extern "fg153_bc_dev_call" fn bc_dev_call(dev:i64,code:i64,a0:i64,a1:i64,a2:i64,a3:i64,a4:i64)->i64;
extern "fg153_bc_buffer_has" fn bc_buffer_has(buf:i64,code:i64)->i64;
extern "fg153_bc_buffer_call" fn bc_buffer_call(buf:i64,code:i64,a0:i64,a1:i64,a2:i64,a3:i64,a4:i64)->i64;
extern "fg153_bc_tensor_get" fn bc_tensor_get(t:i64,code:i64,i:i64)->i64;
extern "fg153_bc_tensor_buffer" fn bc_tensor_buffer(t:i64)->i64;
extern "fg153_bc_tensor_nbytes" fn bc_tensor_nbytes(t:i64)->i64;
extern "fg153_bc_same_layout" fn bc_same_layout(a:i64,b:i64)->i64;
extern "fg153_bc_buffer_is_host" fn bc_buffer_is_host(buf:i64)->i64;
extern "fg153_bc_event_get" fn bc_event_get(e:i64,code:i64)->i64;
extern "fg153_bc_alloc" fn bc_alloc(bytes:i64)->i64;
extern "fg153_bc_free_mem" fn bc_free_mem(ptr:i64);
extern "fg153_bc_fail" fn bc_fail(code:i64);

fn bc_ensure(ok:bool) {if !ok {bc_fail(1);}}
fn bc_bounds(off:i64,size:i64,total:i64)->bool {return off>=0&&size>=0&&off<=total&&size<=total-off;}
fn bc_data(t:i64)->i64 {return bc_tensor_get(t,0,0);}
fn bc_buffer(t:i64)->i64 {return bc_tensor_get(t,1,0);}

fn bc_sync(b:i64) {
    if b!=0 {if bc_backend_has(b,10)!=0 {let ignored=bc_backend_call(b,10,0,0,0,0,0);}}
}

fn bc_tensor_set(t:i64,data:i64,off:i64,size:i64) {
    bc_ensure(t!=0);
    let buf=bc_tensor_buffer(t);
    bc_ensure(buf!=0);
    if size==0 {return;}
    bc_ensure(data!=0);
    bc_ensure(bc_data(t)!=0);
    bc_ensure(bc_bounds(off,size,bc_tensor_nbytes(t)));
    bc_ensure(bc_buffer_has(buf,1)!=0);
    let ignored=bc_buffer_call(buf,1,t,data,off,size,0);
}

fn bc_tensor_get(t:i64,data:i64,off:i64,size:i64) {
    bc_ensure(t!=0);
    let buf=bc_tensor_buffer(t);
    bc_ensure(buf!=0);
    if size==0 {return;}
    bc_ensure(data!=0);
    bc_ensure(bc_data(t)!=0);
    bc_ensure(bc_bounds(off,size,bc_tensor_nbytes(t)));
    bc_ensure(bc_buffer_has(buf,2)!=0);
    let ignored=bc_buffer_call(buf,2,t,data,off,size,0);
}

fn bc_set_async(b:i64,t:i64,data:i64,off:i64,size:i64) {
    bc_ensure(b!=0);bc_ensure(t!=0);
    bc_ensure(bc_data(t)!=0);
    bc_ensure(bc_bounds(off,size,bc_tensor_nbytes(t)));
    if bc_backend_has(b,2)==0 {
        bc_sync(b);
        bc_tensor_set(t,data,off,size);
        return;
    }
    let ignored=bc_backend_call(b,2,t,data,off,size,0);
}

fn bc_get_async(b:i64,t:i64,data:i64,off:i64,size:i64) {
    bc_ensure(b!=0);bc_ensure(t!=0);
    bc_ensure(bc_data(t)!=0);
    bc_ensure(bc_bounds(off,size,bc_tensor_nbytes(t)));
    if bc_backend_has(b,3)==0 {
        bc_sync(b);
        bc_tensor_get(t,data,off,size);
        return;
    }
    let ignored=bc_backend_call(b,3,t,data,off,size,0);
}

fn bc_memset(t:i64,value:i64,off:i64,size:i64) {
    bc_ensure(t!=0);
    let buf=bc_tensor_buffer(t);
    if size==0 {return;}
    bc_ensure(buf!=0);
    bc_ensure(bc_data(t)!=0);
    bc_ensure(bc_bounds(off,size,bc_tensor_nbytes(t)));
    bc_ensure(bc_buffer_has(buf,5)!=0);
    let ignored=bc_buffer_call(buf,5,t,value,off,size,0);
}

fn bc_tensor_copy(src:i64,dst:i64) {
    bc_ensure(src!=0);bc_ensure(dst!=0);
    bc_ensure(bc_same_layout(src,dst)!=0);
    if src==dst {return;}
    let sbuf=bc_buffer(src);
    let dbuf=bc_buffer(dst);
    if bc_buffer_is_host(sbuf)!=0 {
        bc_tensor_set(dst,bc_data(src),0,bc_tensor_nbytes(src));
        return;
    }
    if bc_buffer_is_host(dbuf)!=0 {
        bc_tensor_get(src,bc_data(dst),0,bc_tensor_nbytes(src));
        return;
    }
    let dbuf_eff=bc_tensor_buffer(dst);
    if bc_buffer_has(dbuf_eff,0)!=0 {
        let copied=bc_buffer_call(dbuf_eff,0,src,dst,0,0,0);
        if copied!=0 {return;}
    }
    let n=bc_tensor_nbytes(src);
    let tmp=bc_alloc(n);
    bc_ensure(tmp!=0);
    bc_tensor_get(src,tmp,0,n);
    bc_tensor_set(dst,tmp,0,n);
    bc_free_mem(tmp);
}

fn bc_graph_compute_async(b:i64,g:i64)->i64 {
    bc_ensure(b!=0);
    bc_ensure(bc_backend_has(b,9)!=0);
    return bc_backend_call(b,9,g,0,0,0,0);
}

fn bc_graph_compute(b:i64,g:i64)->i64 {
    let status=bc_graph_compute_async(b,g);
    bc_sync(b);
    return status;
}

fn bc_supports_op(b:i64,op:i64)->i64 {
    bc_ensure(b!=0);
    let d=bc_backend_device(b);
    bc_ensure(d!=0);
    bc_ensure(bc_dev_has(d,0)!=0);
    return bc_dev_call(d,0,op,0,0,0,0);
}

fn bc_supports_buft(b:i64,buft:i64)->i64 {
    bc_ensure(b!=0);
    let d=bc_backend_device(b);
    bc_ensure(d!=0);
    bc_ensure(bc_dev_has(d,5)!=0);
    return bc_dev_call(d,5,buft,0,0,0,0);
}

fn bc_offload_op(b:i64,op:i64)->i64 {
    bc_ensure(b!=0);
    let d=bc_backend_device(b);
    bc_ensure(d!=0);
    if bc_dev_has(d,1)==0 {return 0;}
    return bc_dev_call(d,1,op,0,0,0,0);
}

fn bc_graph_optimize(b:i64,g:i64,params:i64) {
    bc_ensure(b!=0);
    if bc_backend_has(b,14)!=0 {let ignored=bc_backend_call(b,14,g,params,0,0,0);}
}

fn bc_event_new(device:i64)->i64 {
    if device==0 {return 0;}
    if bc_dev_has(device,2)==0 {return 0;}
    return bc_dev_call(device,2,0,0,0,0,0);
}

fn bc_event_free(e:i64) {
    if e==0 {return;}
    let d=bc_event_get(e,0);
    bc_ensure(d!=0);
    bc_ensure(bc_dev_has(d,3)!=0);
    let ignored=bc_dev_call(d,3,e,0,0,0,0);
}

fn bc_event_record(e:i64,b:i64) {
    bc_ensure(b!=0);
    bc_ensure(bc_backend_has(b,12)!=0);
    let ignored=bc_backend_call(b,12,e,0,0,0,0);
}

fn bc_event_synchronize(e:i64) {
    bc_ensure(e!=0);
    let d=bc_event_get(e,0);
    bc_ensure(d!=0);
    bc_ensure(bc_dev_has(d,4)!=0);
    let ignored=bc_dev_call(d,4,e,0,0,0,0);
}

fn bc_event_wait(b:i64,e:i64) {
    bc_ensure(b!=0);
    bc_ensure(bc_backend_has(b,13)!=0);
    let ignored=bc_backend_call(b,13,e,0,0,0,0);
}

fn bc_selftest()->i64 {return 1;}

fn bc_entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64,g:i64)->i64 {
    if mode==0 {bc_tensor_set(a,b,c,d);return 0;}
    if mode==1 {bc_tensor_get(a,b,c,d);return 0;}
    if mode==2 {bc_set_async(a,b,c,d,e);return 0;}
    if mode==3 {bc_get_async(a,b,c,d,e);return 0;}
    if mode==4 {bc_memset(a,b,c,d);return 0;}
    if mode==5 {bc_tensor_copy(a,b);return 0;}
    if mode==6 {bc_sync(a);return 0;}
    if mode==7 {return bc_graph_compute(a,b);}
    if mode==8 {return bc_graph_compute_async(a,b);}
    if mode==9 {return bc_supports_op(a,b);}
    if mode==10 {return bc_supports_buft(a,b);}
    if mode==11 {return bc_offload_op(a,b);}
    if mode==12 {bc_graph_optimize(a,b,c);return 0;}
    if mode==13 {return bc_event_new(a);}
    if mode==14 {bc_event_free(a);return 0;}
    if mode==15 {bc_event_record(a,b);return 0;}
    if mode==16 {bc_event_synchronize(a);return 0;}
    if mode==17 {bc_event_wait(a,b);return 0;}
    if mode==18 {return bc_backend_name(a);}
    if mode==19 {return bc_selftest();}
    bc_ensure(false);return 0;
}

fn main() {print(bc_entry(19,0,0,0,0,0,0,0));}
