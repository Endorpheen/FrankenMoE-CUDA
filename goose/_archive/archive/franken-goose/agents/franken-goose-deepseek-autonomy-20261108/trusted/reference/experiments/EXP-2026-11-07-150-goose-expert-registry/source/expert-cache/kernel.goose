extern "fg150_cache_get" fn get(c:i64,field:i64,index:i64)->i64;
extern "fg150_cache_put" fn put(c:i64,field:i64,index:i64,value:i64);
extern "fg150_cache_action" fn action(c:i64,op:i64,a:i64,b:i64);
extern "fg150_cache_witness" fn witness(mode:i64);
fn move(c:i64,s:i64,protect:i64) {
    let from=get(c,13,s);if from==protect&&s==get(c,17,from) {return;}
    let prev=get(c,11,s);let next=get(c,12,s);
    if prev>=0 {put(c,12,prev,next);}else {put(c,16,from,next);}
    if next>=0 {put(c,11,next,prev);}else {put(c,17,from,prev);}
    let tail=get(c,17,protect);put(c,11,s,tail);put(c,12,s,-1);
    if tail>=0 {put(c,12,tail,s);}else {put(c,16,protect,s);}
    put(c,17,protect,s);put(c,13,s,protect);put(c,3,0,get(c,3,0)+protect-from);
}
fn touch(c:i64,s:i64) {move(c,s,1);if get(c,3,0)>get(c,1,0)/2 {move(c,get(c,16,1),0);}}
fn victim(c:i64,layer:i64)->i64 {
    let experts=get(c,0,0);let generation=get(c,4,0);
    for segment in 2 {
        var slot=get(c,16,segment);var checked:i64=0;
        while slot>=0 {assert(checked<get(c,1,0));checked+=1;let key=get(c,10,slot);
            if key<0||key/experts!=layer||get(c,14,key%experts)!=generation {return slot;}
            slot=get(c,12,slot);
        }
    }
    assert(false);return -1;
}
fn initialize(c:i64) {
    let layers=get(c,30,0);let experts=get(c,31,0);let slots=get(c,32,0);
    assert(layers>=0&&experts>0&&slots>0&&layers<=2147483647/experts);
    action(c,0,0,0);put(c,0,0,experts);put(c,1,0,slots);put(c,3,0,0);put(c,4,0,0);
    for s in slots {put(c,10,s,-1);put(c,11,s,s-1);put(c,12,s,if s+1<slots {s+1}else {-1});put(c,13,s,0);}
    put(c,16,0,0);put(c,17,0,slots-1);put(c,16,1,-1);put(c,17,1,-1);
    for e in experts {put(c,14,e,0);}
}
fn plan(c:i64)->i64 {
    action(c,3,0,0);action(c,1,0,0);let experts=get(c,0,0);let layer=get(c,33,0);assert(layer>=0&&layer<get(c,2,0));
    let generation=((get(c,4,0) as u32)+(1 as u32)) as i64;
    if generation==0 {for e in experts {put(c,14,e,0);}put(c,4,0,1);}else {put(c,4,0,generation);}
    for i in get(c,34,0) {let expert=get(c,18,i);assert(expert>=0&&expert<experts);
        if get(c,14,expert)!=get(c,4,0) {put(c,14,expert,get(c,4,0));action(c,2,expert,0);}
    }
    let count=get(c,5,0);if count>get(c,1,0) {return 0;}
    for i in count {let expert=get(c,15,i);let slot=get(c,19,layer*experts+expert);if slot>=0 {touch(c,slot);put(c,6,0,get(c,6,0)+1);}}
    // Insertion sort preserves the original ascending miss-upload order without a native algorithm call.
    var i:i64=1;while i<count {let expert=get(c,15,i);var j=i;while j>0&&get(c,15,j-1)>expert {put(c,15,j,get(c,15,j-1));j-=1;}put(c,15,j,expert);i+=1;}
    for k in count {let expert=get(c,15,k);if get(c,19,layer*experts+expert)<0 {
        let slot=victim(c,layer);let old=get(c,10,slot);if old>=0 {put(c,19,old,-1);}
        put(c,10,slot,layer*experts+expert);put(c,19,layer*experts+expert,slot);move(c,slot,0);action(c,4,expert,slot);
    }}return 1;
}
fn eligible(c:i64)->i64 {
    let layer=get(c,40,0);let count=get(c,41,0);if layer<0||layer>=count||get(c,42,0)<0 {return 0;}
    let tokens=get(c,43,0);let used=get(c,44,0);let total=get(c,46,0);let slots=get(c,45,0);
    if tokens<=0||tokens>get(c,47,0)||used<0 {return 0;}
    assert(used==0||tokens<=9223372036854775807/used);
    let requested=tokens*used;return if (if requested<total {requested}else {total})<=slots {1}else {0};
}
fn read_span(c:i64)->i64 {
    let first=get(c,40,0);let last=get(c,41,0);let count=get(c,34,0);
    if count==0||first<0||last<first||first>=count||last>=count {return 0;}
    let slot=get(c,18,first);if slot<0 {return 0;}
    var n:i64=1;while first+n<=last&&get(c,18,first+n)==slot+n {n+=1;}return n;
}
fn fill_span(c:i64)->i64 {
    let first=get(c,40,0);let count=get(c,34,0);assert(first>=0&&first<count);
    let expert=get(c,18,2*first);let slot=get(c,18,2*first+1);var n:i64=1;
    while first+n<count&&get(c,18,2*(first+n))==expert+n&&get(c,18,2*(first+n)+1)==slot+n {n+=1;}return n;
}
extern "fg150_budget_get" fn budget_get(c:i64,field:i64,index:i64)->i64;
extern "fg150_budget_float" fn budget_float(c:i64,field:i64,index:i64)->f64;
extern "fg150_budget_put" fn budget_put(c:i64,field:i64,index:i64,value:i64);
extern "fg150_budget_put_float" fn budget_put_float(c:i64,index:i64,value:f64);
extern "fg150_budget_double_bits" fn double_bits(value:f64)->u64;
fn budget_integer(value:f64)->i64 {
    assert(value>=(0 as f64)&&value<(9223372036854775807 as f64));
    if value<(1 as f64) {return 0;}
    let bits=double_bits(value);let exponent=((bits>>52)&2047) as i64;let shift=exponent-1023;
    assert((bits>>63)==0&&shift>=0&&shift<63);
    let mantissa=(bits&4503599627370495)|(1 as u64)<<52;
    let integer=if shift>=52 {mantissa<<(shift-52)}else {mantissa>>(52-shift)};
    assert(integer<=9223372036854775807);return integer as i64;
}
fn safe_add(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn safe_mul(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);assert(a==0||b<=9223372036854775807/a);return a*b;}
fn bank_bytes(c:i64,group:i64,slots:i64)->i64 {
    let device=budget_get(c,20,group);let alignment=budget_get(c,12,device);assert(alignment>0&&(alignment&(alignment-1))==0);
    var bytes:i64=0;let first=budget_get(c,24,group);
    for bank in budget_get(c,25,group) {let raw=safe_mul(budget_get(c,30,first+bank),safe_add(slots,1));bytes=safe_add(bytes,safe_mul(safe_add(raw,alignment-1)/alignment,alignment));}
    return bytes;
}
fn budget(c:i64)->i64 {
    let devices=budget_get(c,2,0);let groups=budget_get(c,3,0);let size=budget_get(c,0,0);let used=budget_get(c,1,0);assert(size>=0&&used>0);
    var by_free=true;for i in budget_get(c,4,0) {if budget_float(c,0,i)!=(0 as f64) {by_free=false;}}
    var total:f64=0 as f64;
    for i in devices {
        budget_put(c,1,i,0);var split:f64=0 as f64;
        if budget_get(c,10,i)>0 {
            if by_free {split=budget_get(c,11,i) as f64;}
            else {let index=budget_get(c,13,i);assert(index>=0&&index<budget_get(c,4,0));split=budget_float(c,0,index);}
        }
        assert(split>=(0 as f64));budget_put_float(c,i,split);total+=split;
    }
    if total==(0 as f64) {
        for i in devices {let split=if budget_get(c,10,i)>0 {1 as f64}else {0 as f64};budget_put_float(c,i,split);total+=split;}
    }
    assert(total>(0 as f64)||groups==0);var host:i64=0;
    for group in groups {
        let device=budget_get(c,20,group);assert(device>=0&&device<devices&&budget_get(c,10,device)>0);
        let raw=(size as f64)*budget_float(c,1,device)/total*(budget_get(c,21,group) as f64)/(budget_get(c,10,device) as f64);
        assert(raw>=(0 as f64)&&raw<(9223372036854775807 as f64));let limit=budget_integer(raw);
        let layers=budget_get(c,22,group);let maximum=safe_mul(layers,budget_get(c,23,group));assert(maximum<=2147483647);
        var slots:i64=0;while slots<maximum {if bank_bytes(c,group,slots+1)>limit {break;}slots+=1;}
        if slots<used {slots=0;}else {
            let tensors=safe_mul(budget_get(c,25,group),safe_add(1,layers));budget_put(c,1,device,safe_add(budget_get(c,40,device),tensors));host=safe_add(host,layers);
        }
        budget_put(c,0,group,slots);
    }
    budget_put(c,2,0,host);return if host>0 {1}else {0};
}
fn entry(mode:i64,c:i64)->i64 {
    witness(mode);
    if mode==0 {initialize(c);return 0;}
    if mode==1 {move(c,get(c,35,0),get(c,36,0));return 0;}
    if mode==2 {touch(c,get(c,35,0));return 0;}
    if mode==3 {return victim(c,get(c,33,0));}
    if mode==4 {return plan(c);}
    if mode==5 {return eligible(c);}
    if mode==6 {return read_span(c);}
    if mode==7 {return fill_span(c);}
    assert(mode==8);return budget(c);
}
fn main(){print(entry(5,0));}
