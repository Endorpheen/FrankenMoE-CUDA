extern "fg151_meta" fn get(c:i64,field:i64,i:i64,j:i64)->i64;
extern "fg151_put" fn put(c:i64,field:i64,i:i64,j:i64,value:i64);
extern "fg151_action" fn action(c:i64,op:i64,a:i64,b:i64,d:i64)->i64;
extern "fg151_witness" fn witness(mode:i64);
fn add(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);assert(a==0||b<=9223372036854775807/a);return a*b;}
fn aligned(value:i64,alignment:i64)->i64 {assert(alignment>0&&(alignment&(alignment-1))==0);return mul(add(value,alignment-1)/alignment,alignment);}
fn collect(c:i64,layer:i64) {
    let cleared=action(c,0,0,0,0);
    for kind in 4 {let tensor=get(c,58,layer,kind);if tensor!=0 {let ignored=action(c,1,tensor,0,0);}}
}
fn host_weight(c:i64,t:i64)->bool {return get(c,43,t,0)!=0&&get(c,44,t,0)==get(c,11,0,0)&&get(c,45,t,0)!=0;}
fn same_layout(c:i64,group:i64)->bool {
    let count=get(c,24,0,0);if count!=get(c,27,group,0) {return false;}
    for i in count {
        let a=get(c,25,i,0);let b=get(c,28,group,i);
        if get(c,40,a,0)!=get(c,40,b,0)||get(c,42,a,2)!=get(c,42,b,2) {return false;}
        for axis in 4 {if get(c,41,a,axis)!=get(c,41,b,axis) {return false;}}
    }
    return true;
}
fn bank_size(c:i64,group:i64,slots:i64)->i64 {
    let device=get(c,26,group,0);let alignment=get(c,23,device,0);var bytes:i64=0;
    for bank in get(c,27,group,0) {let t=get(c,28,group,bank);bytes=add(bytes,aligned(mul(get(c,42,t,2),add(slots,1)),alignment));}return bytes;
}
fn cleanup(c:i64) {
    // Temporary views precede their bank parents;each buffer precedes its owning tensor context.
    let a=action(c,26,0,0,0);let views=action(c,17,-2,0,0);
    let host=action(c,18,-1,0,0);let host_context=action(c,17,-1,0,0);
    for device in get(c,2,0,0) {let buffer=action(c,18,device,0,0);let context=action(c,17,device,0,0);}
}
fn fail(c:i64,reason:i64)->i64 {cleanup(c);let ignored=action(c,5,reason,0,0);assert(false);return 0;}
fn context(c:i64,id:i64,tensors:i64)->bool {
    let bytes=mul(tensors,get(c,55,0,0));return action(c,8,id,bytes,0)!=0;
}
fn attach_dummy(c:i64,id:i64,ctx:i64)->bool {
    let buffer=action(c,14,id,1,0);if buffer==0 {return false;}
    var tensor=get(c,47,ctx,0);
    while tensor!=0 {put(c,7,tensor,0,buffer);tensor=get(c,48,ctx,tensor);}return true;
}
fn construct(c:i64)->i64 {
    for i in get(c,0,0,0) {
        let kind=get(c,15,i,0);
        if kind==get(c,8,0,0)||kind==get(c,9,0,0) {
            let device=action(c,2,i,0,0);var selected:i64= -1;
            for j in get(c,12,0,0) {if get(c,13,j,0)==get(c,16,i,0) {selected=j;break;}}
            put(c,8,device,0,selected);
        }
    }
    if get(c,2,0,0)==0 {return fail(c,0);}
    if get(c,4,0,0)==get(c,10,0,0) {return fail(c,1);}
    if get(c,5,0,0)==0||get(c,6,0,0)==0 {return fail(c,2);}
    for layer in get(c,1,0,0) {
        collect(c,layer);var eligible=get(c,24,0,0)>0;
        for i in get(c,24,0,0) {if !host_weight(c,get(c,25,i,0)) {eligible=false;}}
        if eligible {
            var device:i64= -1;
            for i in get(c,2,0,0) {if get(c,17,i,0)==get(c,14,layer,0) {device=i;break;}}
            if device>=0 {
                var group:i64= -1;
                for i in get(c,3,0,0) {if get(c,26,i,0)==device&&same_layout(c,i) {group=i;break;}}
                if group<0 {group=action(c,3,device,0,0);}
                let added=action(c,4,group,layer,0);
                for i in get(c,24,0,0) {let bytes=get(c,46,get(c,25,i,0),0);put(c,0,device,0,add(get(c,18,device,0),bytes));put(c,1,group,0,add(get(c,32,group,0),bytes));}
            }
        }
    }
    if get(c,3,0,0)==0 {let ignored=action(c,6,0,0,0);return 1;}
    let budget=action(c,7,0,0,0);
    if get(c,34,0,0)==0 {return fail(c,3);}
    for group in get(c,3,0,0) {
        if get(c,31,group,0)>0 {let ignored=action(c,9,group,0,0);}else {let ignored=action(c,22,group,0,0);}
    }
    for device in get(c,2,0,0) {let count=get(c,33,device,0);if count>0&&!context(c,device,count) {return fail(c,4);}}
    if !context(c,-1,get(c,34,0,0))||!context(c,-2,2) {return fail(c,4);}
    let host_alignment=get(c,56,0,0);
    for group in get(c,3,0,0) {
        let slots=get(c,31,group,0);
        if slots>0 {
            for bank in get(c,27,group,0) {if action(c,10,group,bank,add(slots,1))==0 {return fail(c,4);}}
            for j in get(c,29,group,0) {
                let layer=get(c,30,group,j);put(c,5,layer,0,group);collect(c,layer);let copied=action(c,11,layer,0,0);
                for bank in get(c,37,layer,0) {if action(c,12,group,layer,bank)==0 {return fail(c,4);}}
                let slot_map=action(c,13,group,layer,0);if slot_map==0 {return fail(c,4);}
                put(c,4,0,0,add(get(c,52,0,0),aligned(get(c,46,slot_map,0),host_alignment)));
            }
            let device=get(c,26,group,0);put(c,3,device,0,add(get(c,21,device,0),bank_size(c,group,slots)));
        }
    }
    if get(c,7,0,0)!=0 {
        for device in get(c,2,0,0) {let ctx=get(c,22,device,0);if ctx!=0&&!attach_dummy(c,device,ctx) {return fail(c,5);}}
        if !attach_dummy(c,-1,get(c,49,0,0)) {return fail(c,5);}
    }else {
        for device in get(c,2,0,0) {if get(c,22,device,0)!=0 {
            let buffer=action(c,14,device,0,0);if buffer==0 {return fail(c,5);}
            let cleared=action(c,15,device,0,0);put(c,3,device,0,get(c,57,buffer,0));
        }}
        let host=action(c,14,-1,0,0);if host==0 {return fail(c,5);}
        let cleared=action(c,15,-1,255,0);put(c,4,0,0,get(c,57,host,0));
        for group in get(c,3,0,0) {for j in get(c,29,group,0) {
            let layer=get(c,30,group,j);if get(c,36,layer,0)!=0 {let linked=action(c,19,group,layer,0);}
        }}
    }
    for device in get(c,2,0,0) {if get(c,20,device,0)!=0 {let ignored=action(c,16,device,0,0);}}
    let weights=action(c,16,-1,0,0);
    for device in get(c,2,0,0) {if get(c,18,device,0)>0 {
        let logged=action(c,20,device,0,0);
        for group in get(c,3,0,0) {if get(c,26,group,0)==device {let logged=action(c,21,group,0,0);}}
    }}return 1;
}
fn copy_read(c:i64)->i64 {
    let layer=get(c,97,0,0);if layer<0 {return 0;}
    put(c,10,0,0,layer);let group=get(c,35,layer,0);assert(group>=0);put(c,11,0,0,group);put(c,12,0,0,get(c,98,0,0));
    let device=get(c,26,group,0);if get(c,74,0,0)!=get(c,91,device,0)||get(c,102,0,0)==0 {return 0;}
    let first=get(c,106,0,0);let last=get(c,107,0,0);let experts=get(c,105,0,0);
    if first<0||last<first||last>=experts||get(c,101,first,0)<0 {return 0;}
    let count=action(c,42,0,0,0);assert(count>0);
    let bank=get(c,103,0,0);let dst=get(c,88,0,0);let slot=get(c,101,first,0);
    let reset=action(c,30,0,0,0);
    put(c,13,0,0,mul(slot,get(c,42,bank,2)));let src_view=action(c,31,bank,count,0);
    put(c,13,0,0,mul(first,get(c,42,dst,2)));let dst_view=action(c,31,dst,count,1);
    assert(src_view!=0&&dst_view!=0);let a=action(c,32,src_view,0,0);let b=action(c,32,dst_view,0,0);let copied=action(c,33,src_view,dst_view,0);
    put(c,21,0,0,add(get(c,111,0,0),count));put(c,22,0,0,add(get(c,112,0,0),get(c,46,src_view,0)));return count;
}
fn upload(c:i64,tensor:i64,source:i64,offset:i64,bytes:i64) {
    assert(source>0&&offset>=0&&bytes>=0);assert(add(offset,bytes)<=get(c,46,tensor,0));
    put(c,14,0,0,bytes);let copied=action(c,40,tensor,source,offset);
}
fn copy_upload(c:i64)->i64 {
    let layer=get(c,70,0,0);if layer<0 {return 0;}
    put(c,10,0,0,layer);let group=get(c,35,layer,0);assert(group>=0);put(c,11,0,0,group);
    assert(get(c,74,0,0)==get(c,91,get(c,26,group,0),0));
    let nodes=get(c,75,0,0);let dst=get(c,88,0,0);var lookup:i64=0;
    var i:i64=0;while i<nodes&&lookup==0 {let node=get(c,76,i,0);if get(c,77,node,0)==get(c,78,0,0)&&get(c,79,node,0)==dst {lookup=node;}i+=1;}
    assert(lookup!=0);let selection=get(c,79,lookup,1);assert(selection!=0);let view=get(c,80,selection,0);
    for i in nodes {let node=get(c,76,i,0);if node==selection||node==view {let ignored=action(c,41,0,layer,0);assert(false);}}
    assert(get(c,81,selection,0)!=0&&get(c,40,selection,0)==get(c,116,0,0));put(c,15,0,0,selection);
    let elements=get(c,115,selection,0);assert(elements>=0);let resized=action(c,34,elements,0,0);
    let experts=get(c,105,0,0);let capacity=if elements<experts {elements}else {experts};let reserved=action(c,37,capacity,0,0);
    let downloaded=action(c,35,0,0,0);let synchronized=action(c,36,0,0,0);
    if action(c,38,0,0,0)==0 {let ignored=action(c,41,1,layer,0);assert(false);}
    var bytes:i64=0;let fills=get(c,84,0,0);
    for bank_index in get(c,37,layer,0) {
        put(c,12,0,0,bank_index);let weights=get(c,38,layer,bank_index);let bank=get(c,103,0,0);let expert_bytes=get(c,42,weights,2);var current:i64=0;
        while current<fills {
            let count=action(c,39,current,0,0);assert(count>0&&count<=fills-current);
            let source_offset=mul(get(c,85,current,0),expert_bytes);let destination_offset=mul(get(c,86,current,0),expert_bytes);let span=mul(count,expert_bytes);
            assert(add(source_offset,span)<=get(c,46,weights,0));let source=add(get(c,113,weights,0),source_offset);
            upload(c,bank,source,destination_offset,span);bytes=add(bytes,span);current=add(current,count);
        }
    }
    let bucket=if get(c,83,0,0)<=mul(8,get(c,6,0,0)) {0}else {1};
    put(c,20,bucket,0,add(get(c,110,bucket,0),get(c,94,0,0)));put(c,20,bucket,1,add(get(c,110,bucket,1),fills));put(c,20,bucket,2,add(get(c,110,bucket,2),bytes));
    let source=get(c,89,0,0);upload(c,dst,get(c,113,source,0),0,get(c,46,source,0));return 1;
}
fn breakdown(c:i64) {
    for device in get(c,2,0,0) {let buffer=get(c,20,device,0);if buffer!=0 {let kind=get(c,60,buffer,0);put(c,23,kind,0,add(get(c,120,kind,0),get(c,21,device,0)));}}
    let buffer=get(c,51,0,0);if buffer!=0 {let kind=get(c,60,buffer,0);put(c,23,kind,0,add(get(c,120,kind,0),get(c,52,0,0)));}
}
fn entry(mode:i64,c:i64)->i64 {
    witness(mode);
    if mode==0 {return construct(c);}
    if mode==1 {let logged=action(c,23,0,0,0);cleanup(c);return 0;}
    if mode==2 {cleanup(c);return 0;}
    if mode==3 {return copy_read(c);}
    if mode==4 {return copy_upload(c);}
    if mode==5 {let layer=get(c,71,0,0);let group=get(c,35,layer,0);assert(group>=0);return get(c,91,get(c,26,group,0),0);}
    assert(mode==6);breakdown(c);return 0;
}
fn main(){print(entry(1,0));}
