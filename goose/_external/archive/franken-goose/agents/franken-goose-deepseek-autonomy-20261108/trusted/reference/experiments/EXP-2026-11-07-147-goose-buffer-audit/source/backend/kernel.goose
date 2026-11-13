extern "fg145_meta" fn meta(context:i64,field:i64)->i64;
extern "fg145_float" fn fp(context:i64,field:i64)->f32;
extern "fg145_action" fn action(context:i64,op:i64,a:i64,b:i64)->i64;
extern "fg145_witness" fn witness(mode:i64);
fn put(c:i64,field:i64,value:i64) {let result=action(c,1,field,value);}
fn plan_put(c:i64,field:i64,value:i64) {let result=action(c,12,field,value);}
fn configure_plan(c:i64) {plan_put(c,1,meta(c,6));plan_put(c,2,meta(c,7));plan_put(c,3,meta(c,8));}
fn create_backend(c:i64)->i64 {
    if action(c,0,0,0)==0 {return 0;}
    put(c,0,meta(c,1));for field in 6 {put(c,field+1,0);}
    let backend=action(c,2,0,0);
    if backend==0 {let ignored=action(c,3,0,0);}return backend;
}
fn release_backend(c:i64) {let work=meta(c,4);let a=action(c,6,work,0);let b=action(c,3,0,0);let d=action(c,4,0,0);}
fn create_plan(c:i64)->i64 {
    let plan=action(c,8,0,0);if plan==0 {return 0;}
    let a=action(c,10,0,0);let b=action(c,11,0,0);let size=meta(c,13);assert(size>=0);
    if size>0 {
        let work=action(c,5,size,0);
        if work==0 {let ignored=action(c,9,0,0);return 0;}
        plan_put(c,0,work);
    }
    configure_plan(c);return plan;
}
fn release_plan(c:i64) {let a=action(c,6,meta(c,14),0);let b=action(c,9,0,0);}
fn compute_graph(c:i64)->i64 {
    let ignored=action(c,10,0,0);let requested=meta(c,13);assert(requested>=0);
    if meta(c,5)<requested {
        let released=action(c,6,meta(c,4),0);put(c,2,0);put(c,3,0);
        let work=action(c,5,requested,0);if work==0 {return meta(c,47);}
        put(c,2,work);put(c,3,requested);
    }
    plan_put(c,0,meta(c,4));configure_plan(c);return action(c,13,0,0);
}
fn extra_member(c:i64,buffer:i64)->bool {
    for index in meta(c,40) {if meta(c,200+index)==buffer {return true;}}return false;
}
fn supports(c:i64)->bool {
    let op=meta(c,20);
    if op==0||op==36||op==37||op==39||op==38 {return true;}
    for source in 4 {let buffer=meta(c,100+source);if buffer!=0&&extra_member(c,buffer) {return action(c,14,source,0)!=0;}}
    let x=meta(c,22);let y=meta(c,23);let kind=meta(c,21);
    if op==34||op==42 {return kind!=18&&kind!=21&&kind!=16&&kind!=17&&kind!=22&&kind!=19&&kind!=29;}
    if op==29 {
        if meta(c,28)==meta(c,29)&&x==0&&kind==0 {return y==0||y==1;}
        let vector=meta(c,24);return y==0||y==vector||(y==30&&vector==0);
    }
    if op==47 {return x==0&&y==0&&fp(c,0)==(0 as f32);}
    if op==53 {return x==0&&(y==0||y==1);}
    if op==41 {return x==0||x==1;}
    if op==31 {return (x==0||((x==1||meta(c,25)!=0)&&meta(c,31)==meta(c,32)&&meta(c,33)==meta(c,34)))&&y==0&&kind==0;}
    if op==56 {return meta(c,26)!=0;}
    if op==77 {return meta(c,27)==1||meta(c,30)==1;}
    return true;
}
fn build_extras(c:i64) {for provider in 4 {let value=action(c,16,provider,0);if value!=0 {let ignored=action(c,15,value,0);}}}
fn copy_extras(c:i64) {for index in meta(c,40) {let ignored=action(c,15,meta(c,200+index),0);}let ignored=action(c,15,0,0);}
fn proc_address(c:i64)->i64 {
    if meta(c,43)==26&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==98&&meta(c,306)==97&&meta(c,307)==99&&meta(c,308)==107&&meta(c,309)==101&&meta(c,310)==110&&meta(c,311)==100&&meta(c,312)==95&&meta(c,313)==115&&meta(c,314)==101&&meta(c,315)==116&&meta(c,316)==95&&meta(c,317)==110&&meta(c,318)==95&&meta(c,319)==116&&meta(c,320)==104&&meta(c,321)==114&&meta(c,322)==101&&meta(c,323)==97&&meta(c,324)==100&&meta(c,325)==115 {return action(c,17,0,0);}
    if meta(c,43)==32&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==98&&meta(c,306)==97&&meta(c,307)==99&&meta(c,308)==107&&meta(c,309)==101&&meta(c,310)==110&&meta(c,311)==100&&meta(c,312)==95&&meta(c,313)==100&&meta(c,314)==101&&meta(c,315)==118&&meta(c,316)==95&&meta(c,317)==103&&meta(c,318)==101&&meta(c,319)==116&&meta(c,320)==95&&meta(c,321)==101&&meta(c,322)==120&&meta(c,323)==116&&meta(c,324)==114&&meta(c,325)==97&&meta(c,326)==95&&meta(c,327)==98&&meta(c,328)==117&&meta(c,329)==102&&meta(c,330)==116&&meta(c,331)==115 {return action(c,17,1,0);}
    if meta(c,43)==25&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==98&&meta(c,306)==97&&meta(c,307)==99&&meta(c,308)==107&&meta(c,309)==101&&meta(c,310)==110&&meta(c,311)==100&&meta(c,312)==95&&meta(c,313)==103&&meta(c,314)==101&&meta(c,315)==116&&meta(c,316)==95&&meta(c,317)==102&&meta(c,318)==101&&meta(c,319)==97&&meta(c,320)==116&&meta(c,321)==117&&meta(c,322)==114&&meta(c,323)==101&&meta(c,324)==115 {return action(c,17,2,0);}
    if meta(c,43)==31&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==98&&meta(c,306)==97&&meta(c,307)==99&&meta(c,308)==107&&meta(c,309)==101&&meta(c,310)==110&&meta(c,311)==100&&meta(c,312)==95&&meta(c,313)==115&&meta(c,314)==101&&meta(c,315)==116&&meta(c,316)==95&&meta(c,317)==97&&meta(c,318)==98&&meta(c,319)==111&&meta(c,320)==114&&meta(c,321)==116&&meta(c,322)==95&&meta(c,323)==99&&meta(c,324)==97&&meta(c,325)==108&&meta(c,326)==108&&meta(c,327)==98&&meta(c,328)==97&&meta(c,329)==99&&meta(c,330)==107 {return action(c,17,3,0);}
    if meta(c,43)==26&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==98&&meta(c,306)==97&&meta(c,307)==99&&meta(c,308)==107&&meta(c,309)==101&&meta(c,310)==110&&meta(c,311)==100&&meta(c,312)==95&&meta(c,313)==99&&meta(c,314)==112&&meta(c,315)==117&&meta(c,316)==95&&meta(c,317)==110&&meta(c,318)==117&&meta(c,319)==109&&meta(c,320)==97&&meta(c,321)==95&&meta(c,322)==105&&meta(c,323)==110&&meta(c,324)==105&&meta(c,325)==116 {return action(c,17,4,0);}
    if meta(c,43)==24&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==98&&meta(c,306)==97&&meta(c,307)==99&&meta(c,308)==107&&meta(c,309)==101&&meta(c,310)==110&&meta(c,311)==100&&meta(c,312)==95&&meta(c,313)==99&&meta(c,314)==112&&meta(c,315)==117&&meta(c,316)==95&&meta(c,317)==105&&meta(c,318)==115&&meta(c,319)==95&&meta(c,320)==110&&meta(c,321)==117&&meta(c,322)==109&&meta(c,323)==97 {return action(c,17,5,0);}
    if meta(c,43)==28&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==98&&meta(c,306)==97&&meta(c,307)==99&&meta(c,308)==107&&meta(c,309)==101&&meta(c,310)==110&&meta(c,311)==100&&meta(c,312)==95&&meta(c,313)==99&&meta(c,314)==112&&meta(c,315)==117&&meta(c,316)==95&&meta(c,317)==115&&meta(c,318)==101&&meta(c,319)==116&&meta(c,320)==95&&meta(c,321)==117&&meta(c,322)==115&&meta(c,323)==101&&meta(c,324)==95&&meta(c,325)==114&&meta(c,326)==101&&meta(c,327)==102 {return action(c,17,6,0);}
    if meta(c,43)==19&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==116&&meta(c,306)==104&&meta(c,307)==114&&meta(c,308)==101&&meta(c,309)==97&&meta(c,310)==100&&meta(c,311)==112&&meta(c,312)==111&&meta(c,313)==111&&meta(c,314)==108&&meta(c,315)==95&&meta(c,316)==110&&meta(c,317)==101&&meta(c,318)==119 {return action(c,17,7,0);}
    if meta(c,43)==20&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==116&&meta(c,306)==104&&meta(c,307)==114&&meta(c,308)==101&&meta(c,309)==97&&meta(c,310)==100&&meta(c,311)==112&&meta(c,312)==111&&meta(c,313)==111&&meta(c,314)==108&&meta(c,315)==95&&meta(c,316)==102&&meta(c,317)==114&&meta(c,318)==101&&meta(c,319)==101 {return action(c,17,8,0);}
    if meta(c,43)==31&&meta(c,300)==103&&meta(c,301)==103&&meta(c,302)==109&&meta(c,303)==108&&meta(c,304)==95&&meta(c,305)==98&&meta(c,306)==97&&meta(c,307)==99&&meta(c,308)==107&&meta(c,309)==101&&meta(c,310)==110&&meta(c,311)==100&&meta(c,312)==95&&meta(c,313)==99&&meta(c,314)==112&&meta(c,315)==117&&meta(c,316)==95&&meta(c,317)==115&&meta(c,318)==101&&meta(c,319)==116&&meta(c,320)==95&&meta(c,321)==116&&meta(c,322)==104&&meta(c,323)==114&&meta(c,324)==101&&meta(c,325)==97&&meta(c,326)==100&&meta(c,327)==112&&meta(c,328)==111&&meta(c,329)==111&&meta(c,330)==108 {return action(c,17,9,0);}
    return 0;
}
fn memory_info(c:i64) {
    var total:i64=0;var available:i64=0;
    if meta(c,44)!=0 {total=meta(c,45);available=meta(c,46);}else {
        let pages=meta(c,45);let page_bytes=meta(c,46);assert(pages>=0&&page_bytes>0&&pages<=9223372036854775807/page_bytes);total=pages*page_bytes;available=total;
    }
    let a=action(c,18,0,available);let b=action(c,18,1,total);
}
fn features(c:i64) {for feature in meta(c,51) {if meta(c,400+feature)>0 {let ignored=action(c,19,feature,0);}}let ignored=action(c,19,-1,0);}
fn checked_add(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn check_span(ptr:i64,off:i64,size:i64,capacity:i64) {
    assert(off>=0&&size>=0&&capacity>=0&&off<=capacity&&size<=capacity-off);
    assert(size==0||ptr>0);let start=checked_add(ptr,off);let end=checked_add(start,size);
}
fn tensor_span(c:i64)->i64 {
    let ptr=meta(c,63);let off=meta(c,64);let size=meta(c,65);
    check_span(ptr,off,size,meta(c,69));
    let base=meta(c,60);let capacity=meta(c,61);assert(base>0&&ptr>=base);
    check_span(base,checked_add(ptr-base,off),size,capacity);
    return checked_add(ptr,off);
}
fn buffer_get_base(c:i64)->i64 {
    let ptr=meta(c,60);let align=meta(c,71);assert(align>0&&ptr>=0);
    if ptr==0 {assert(meta(c,61)==0);return 0;}
    let padding=(align-ptr%align)%align;return checked_add(ptr,padding);
}
fn buffer_alloc(c:i64)->i64 {
    let size=meta(c,61);if size<=0 {return 0;}
    return action(c,20,size,0);
}
fn buffer_free(c:i64)->i64 {
    let kind=meta(c,62);if kind==1 {return 0;}
    assert(kind==0);let ignored=action(c,22,meta(c,60),meta(c,61));return 0;
}
fn buffer_memset(c:i64) {
    let ptr=tensor_span(c);let size=meta(c,65);let value=meta(c,66);assert(value>=0&&value<=255);
    if size>0 {let ignored=action(c,24,ptr,value);}
}
fn buffer_set(c:i64) {
    let ptr=tensor_span(c);let size=meta(c,65);let source=meta(c,67);
    if size>0 {check_span(source,0,size,size);let ignored=action(c,25,ptr,source);}
}
fn buffer_get(c:i64) {
    let ptr=tensor_span(c);let size=meta(c,65);let target=meta(c,68);
    if size>0 {check_span(target,0,size,size);let ignored=action(c,25,target,ptr);}
}
fn buffer_cpy(c:i64)->i64 {
    if meta(c,70)==0 {return 0;}
    let size=meta(c,69);assert(size==meta(c,80));let source=meta(c,63);let target=meta(c,68);
    let src_base=meta(c,81);let dst_base=meta(c,60);assert(source>=src_base&&target>=dst_base);
    check_span(src_base,source-src_base,size,meta(c,82));
    check_span(dst_base,target-dst_base,size,meta(c,61));
    if size>0&&source!=target {let ignored=action(c,25,target,source);}
    return 1;
}
fn buffer_clear(c:i64) {
    let ptr=meta(c,60);let size=meta(c,61);let value=meta(c,66);assert(value>=0&&value<=255);
    check_span(ptr,0,size,size);if size>0 {let ignored=action(c,24,ptr,value);}
}
fn buffer_from_ptr(c:i64)->i64 {
    let ptr=meta(c,60);let align=meta(c,71);assert(ptr>=0&&align>0&&ptr%align==0);
    check_span(ptr,0,meta(c,61),meta(c,61));return ptr;
}
fn traits_forward(c:i64)->i64 {
    for index in meta(c,40) {if action(c,27,index,0)!=0 {return 1;}}return 0;
}
fn traits_worksize(c:i64)->i64 {
    for index in meta(c,40) {if action(c,28,index,0)!=0 {return 1;}}return 0;
}
fn hbm_alloc(c:i64)->i64 {
    let size=meta(c,61);let align=meta(c,71);assert(size>=0&&align>0);
    return align;
}
fn entry(mode:i64,c:i64)->i64 {
    witness(mode);
    if mode==0 {return create_backend(c);}
    if mode==1 {release_backend(c);return 0;}
    if mode==2 {return create_plan(c);}
    if mode==3 {release_plan(c);return 0;}
    if mode==4 {return action(c,13,1,0);}
    if mode==5 {return compute_graph(c);}
    if mode>=6&&mode<=9 {
        assert(meta(c,0)!=0);
        if mode==6 {put(c,0,meta(c,9));}
        else if mode==7 {let old=meta(c,3);let next=meta(c,10);if old!=0&&old!=next {let ignored=action(c,7,old,0);}put(c,1,next);}
        else if mode==8 {put(c,4,meta(c,10));put(c,5,meta(c,11));}
        else {put(c,6,meta(c,9));}return 0;
    }
    if mode==10 {return if supports(c) {1}else {0};}
    if mode==11 {return if meta(c,41)!=0||extra_member(c,meta(c,42)) {1}else {0};}
    if mode==12 {return if extra_member(c,meta(c,42)) {1}else {0};}
    if mode==13 {build_extras(c);return 0;}
    if mode==14 {copy_extras(c);return 0;}
    if mode==15 {return proc_address(c);}
    if mode==16 {memory_info(c);return 0;}
    if mode==17 {return 1;}
    if mode==18 {features(c);return 0;}
    if mode==19 {return buffer_get_base(c);}
    if mode==20 {return buffer_alloc(c);}
    if mode==21 {let x=buffer_free(c);return x;}
    if mode==22 {buffer_memset(c);return 0;}
    if mode==23 {buffer_set(c);return 0;}
    if mode==24 {buffer_get(c);return 0;}
    if mode==25 {return buffer_cpy(c);}
    if mode==26 {buffer_clear(c);return 0;}
    if mode==27 {return buffer_from_ptr(c);}
    if mode==28 {return meta(c,71);}
    if mode==29 {return 1;}
    if mode==30 {return traits_forward(c);}
    if mode==31 {return traits_worksize(c);}
    if mode==32 {return hbm_alloc(c);}
    assert(mode==33);let ignored=action(c,23,meta(c,60),0);return 0;
}
fn main(){let value=entry(17,0);print(value);}
