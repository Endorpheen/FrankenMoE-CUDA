extern "fg149_meta" fn meta(c:i64,field:i64)->i64;
extern "fg149_set" fn put(c:i64,field:i64,value:i64);
extern "fg149_action" fn action(c:i64,op:i64,a:i64,b:i64)->i64;
extern "fg149_witness" fn witness(mode:i64);
fn checked_mul(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
fn valid(mask:const u8[:])->bool {for i in mask.len {if mask[i]!=0 {return true;}}return false;}
fn select_mask(mask:const u8[:],out:u8[:],strict:bool,first:i64)->i64 {
    assert(mask.len==512&&out.len>=512&&first>=0&&first<=512);
    if !strict {for i in 512 {out[i]=mask[i];}return first;}
    for i in 512 {out[i]=0;}
    for i in 512 {let at=(first+i)%512;if mask[at]!=0 {out[at]=1;return at+1;}}
    return first;
}
fn priority(c:i64,value:i64)->i64 {
    if value==0 {return 1;}
    var policy=meta(c,80);var level:i64=0;
    if value== -1 {policy=meta(c,81);}else if value>=1&&value<=3 {policy=meta(c,82);level=if value==1 {40}else if value==2 {80}else {90};}
    return action(c,26,policy,level);
}
fn placement(c:i64,mask:const u8[:]) {let a=priority(c,meta(c,14));if valid(mask) {let ignored=action(c,27,0,0);}}
fn store_mask(c:i64,worker:i64,mask:const u8[:]) {for i in 512 {put(c,10000+worker*512+i,mask[i] as i64);}}
fn shutdown(c:i64,created:i64,mutex_ready:bool,cond_ready:bool) {
    if mutex_ready {let a=action(c,9,0,0);put(c,6,1);put(c,7,0);if cond_ready {let b=action(c,12,0,0);}let b=action(c,10,0,0);}
    for i in created {let result=action(c,14,i+1,0);assert(result==meta(c,22)||result==meta(c,23));}
    if cond_ready {let a=action(c,8,0,0);}if mutex_ready {let a=action(c,7,0,0);}
    let a=action(c,3,checked_mul(meta(c,7),meta(c,10)),0);let b=action(c,1,meta(c,6),0);
}
fn create(c:i64,mask:const u8[:])->i64 {
    let threads=meta(c,1);if threads<=0||threads>512||mask.len!=512 {return 0;}
    let size=checked_mul(threads,meta(c,7));if action(c,0,meta(c,6),0)==0 {return 0;}
    put(c,0,0);put(c,1,0);put(c,2,0);put(c,3,0);put(c,4,0);put(c,5,0);put(c,6,0);put(c,7,meta(c,2));put(c,8,-1);put(c,9,threads);put(c,10,meta(c,3));put(c,11,meta(c,4));put(c,12,0);put(c,17,0);
    if action(c,2,size,0)==0 {let ignored=action(c,1,meta(c,6),0);return 0;}
    let ignored=action(c,4,size,0);for worker in threads {put(c,16,worker);}
    var selected:u8[512]=default<u8[512]>();var cursor:i64=0;
    if meta(c,9)!=0 {
        for worker in threads {cursor=select_mask(mask,selected,meta(c,5)!=0,cursor);store_mask(c,worker,selected);}
    }else {
        if action(c,5,0,0)!=0 {shutdown(c,0,false,false);return 0;}
        if action(c,6,0,0)!=0 {shutdown(c,0,true,false);return 0;}
        var created:i64=0;var worker:i64=1;
        while worker<threads {
            cursor=select_mask(mask,selected,meta(c,5)!=0,cursor);store_mask(c,worker,selected);
            if action(c,13,worker,0)!=0 {shutdown(c,created,true,true);return 0;}
            created+=1;worker+=1;
        }
        cursor=select_mask(mask,selected,meta(c,5)!=0,cursor);store_mask(c,0,selected);
        if meta(c,11)==0 {placement(c,selected);}
    }
    return meta(c,0);
}
fn release(c:i64) {if meta(c,0)==0 {return;}let threaded=meta(c,9)==0;shutdown(c,if threaded {meta(c,10)-1}else {0},threaded,threaded);}
fn set_pause(c:i64,value:i64) {put(c,7,value);let ignored=action(c,12,0,0);}
fn pause(c:i64,value:i64) {
    if meta(c,9)!=0 {return;}let a=action(c,9,0,0);if meta(c,11)!=value {set_pause(c,value);}let b=action(c,10,0,0);
}
fn barrier(c:i64) {
    let threads=meta(c,18)&65535;if threads==1 {return;}
    if meta(c,9)!=0 {let a=action(c,20,0,0);return;}
    let passed=meta(c,19);let arrived=action(c,16,1,0);
    if arrived==threads-1 {let a=action(c,17,0,0);let b=action(c,18,1,0);return;}
    while meta(c,19)==passed {let ignored=action(c,15,0,0);}
    let ignored=action(c,19,0,0);
}
fn ready(c:i64)->bool {
    if meta(c,15)!=0||meta(c,12)!=0||meta(c,11)!=0 {return true;}
    let graph=meta(c,18);if graph!=meta(c,17) {put(c,13,if meta(c,16)<(graph&65535) {1}else {0});put(c,14,graph);return true;}
    return false;
}
fn poll(c:i64)->bool {
    let rounds=checked_mul(131072,meta(c,13));var i:i64=0;
    while !ready(c) {if i>=rounds {break;}let ignored=action(c,15,0,0);i+=1;}
    return meta(c,15)!=0;
}
fn check_work(c:i64)->bool {
    if poll(c) {let ignored=action(c,19,1,0);return meta(c,15)!=0;}
    let a=action(c,9,1,0);while !ready(c) {let b=action(c,11,0,0);}let b=action(c,10,1,0);return meta(c,15)!=0;
}
fn secondary(c:i64,mask:const u8[:]) {
    placement(c,mask);
    while true {
        while meta(c,11)!=0 {let a=action(c,9,1,0);if meta(c,11)!=0 {let b=action(c,11,0,0);}let b=action(c,10,1,0);}
        if meta(c,12)!=0 {break;}
        let ignored=check_work(c);if meta(c,15)!=0 {put(c,13,0);let a=action(c,25,meta(c,16),0);}
    }
}
fn kickoff(c:i64,threads:i64,mask:const u8[:]) {
    assert(threads>0&&threads<=meta(c,10));let a=action(c,9,0,0);
    let bits=meta(c,18) as! u32;let generation=((bits>>16)+1)&65535;let next=(generation<<16)|(threads as u32);
    let b=action(c,23,(next as! i32) as i64,0);
    if meta(c,11)!=0 {placement(c,mask);set_pause(c,0);}else {let d=action(c,12,0,0);}
    let d=action(c,10,0,0);
}
fn numa(c:i64,clear:bool) {
    if meta(c,70)<=1 {return;}let cpus=meta(c,73);assert(cpus>0&&cpus<=512);var node:i64=0;
    if !clear {
        let strategy=meta(c,71);if strategy==3 {let ignored=action(c,44,0,0);return;}
        if strategy==1 {node=meta(c,20)%meta(c,70);}else if strategy==2 {node=meta(c,72);}else {return;}
        assert(node>=0&&node<meta(c,70));
    }
    if action(c,40,cpus,0)==0 {return;}
    if clear {for cpu in cpus {let ignored=action(c,41,cpu,0);}}
    else {for i in meta(c,100+node) {let cpu=meta(c,500000+node*512+i);assert(cpu>=0&&cpu<cpus);let ignored=action(c,41,cpu,0);}}
    let a=action(c,42,0,0);let b=action(c,43,0,0);
}
fn entry(mode:i64,c:i64,mask:const u8[:],local:u8[:])->i64 {
    witness(mode);
    if mode==0 {return create(c,mask);}if mode==1 {release(c);return 0;}
    if mode==2 {return if valid(mask) {1}else {0};}
    if mode==3 {return select_mask(mask,local,meta(c,20)!=0,meta(c,21));}
    if mode==4||mode==5 {pause(c,if mode==4 {1}else {0});return 0;}
    if mode==6 {barrier(c);return 0;}
    if mode==7 {let ignored=action(c,21,meta(c,20),0);return 0;}if mode==8 {return action(c,22,meta(c,20),0);}
    if mode==9 {return if ready(c) {1}else {0};}if mode==10 {return if poll(c) {1}else {0};}if mode==11 {return if check_work(c) {1}else {0};}
    if mode==12 {secondary(c,mask);return 0;}if mode==13 {kickoff(c,meta(c,20),mask);return 0;}
    if mode==15 {placement(c,mask);return 0;}if mode==16||mode==17 {numa(c,mode==17);return 0;}
    assert(mode==18);return priority(c,meta(c,20));
}
fn main(){let mask:u8[0]=default<u8[0]>();var local:u8[0]=default<u8[0]>();let ignored=entry(2,0,mask,local);}
