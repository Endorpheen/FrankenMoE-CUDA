extern "powf" fn pow32(a:f32,b:f32)->f32;
fn group_normalize(p:const Node&,x:const u8[:],dst:u8[:])->bool {
    if p.op!=OP_GROUP_NORM {return false;}assert(same_shape(p.x,p.d));let groups=p.ip[0];assert(groups>0);let channels=p.x.ne[2];let per=(channels+groups-1)/groups;var group=thread_first(p);
    while group<groups {
        let first=group*per;let last=low(first+per,channels);let count=(last-first)*p.x.ne[0]*p.x.ne[1];assert(count>0);
        for batch in p.x.ne[3] {
            var sum:f64=(0 as f64);
            for ch in last-first {for row in p.x.ne[1] {var part:f64=(0 as f64);for col in p.x.ne[0] {let index=col+p.x.ne[0]*(row+p.x.ne[1]*(ch+first+channels*batch));part+=load(p.x,x,index) as f64;}sum+=part;}}
            let mean=(sum/(count as f64)) as f32;var sq:f64=(0 as f64);
            for ch in last-first {for row in p.x.ne[1] {var part:f64=(0 as f64);for col in p.x.ne[0] {let index=col+p.x.ne[0]*(row+p.x.ne[1]*(ch+first+channels*batch));let v=load(p.x,x,index)-mean;part+=(v*v) as f64;store(p.d,dst,index,v);}sq+=part;}}
            let scale=(1 as f32)/sqrt32((sq/(count as f64)) as f32+p.fp[0]);
            for ch in last-first {for row in p.x.ne[1] {for col in p.x.ne[0] {let index=col+p.x.ne[0]*(row+p.x.ne[1]*(ch+first+channels*batch));store(p.d,dst,index,load(p.d,dst,index)*scale);}}}
        }
        group+=p.nth;
    }
    return true;
}
fn softmax(p:const Node&,x:const u8[:],mask:const u8[:],sinks:const u8[:],dst:u8[:],scratch:f32[:])->bool {
    if p.op!=OP_SOFT_MAX&&p.op!=OP_SOFT_MAX_BACK {return false;}let width=p.x.ne[0];assert(width>0&&same_shape(p.x,p.d));assert(scratch.len>=mul_size(width,p.nth));let base=p.ith*width;
    var row=thread_first(p);
    while row<rows(p.x) {
        if p.op==OP_SOFT_MAX_BACK {
            assert(same_shape(p.x,p.y));var dot:f64=(0 as f64);for col in width {let index=row*width+col;dot+=(load(p.x,x,index)*load(p.y,mask,index)) as f64;}
            for col in width {let index=row*width+col;let grad=load(p.x,x,index);let prob=load(p.y,mask,index);store(p.d,dst,index,p.fp[0]*prob*(grad-dot as f32));}
        }else {
            let r1=row%p.x.ne[1];let head=(row/p.x.ne[1])%p.x.ne[2];let batch=row/(p.x.ne[1]*p.x.ne[2]);var head_power:i64=1;while head_power<=p.x.ne[2]/2 {head_power*=2;}
            var slope:f32=(1 as f32);
            if p.fp[1]>(0 as f32) {let m=pow32((2 as f32),-p.fp[1]/((if head<head_power {head_power} else {2*head_power}) as f32));slope=pow32(m,(if head<head_power {head+1} else {2*(head-head_power)+1}) as f32);}
            var maximum=from_bits(0xff800000);
            for col in width {
                var v=load(p.x,x,row*width+col)*p.fp[0];
                if p.ip[0]!=0 {assert(p.y.ne[0]>=width&&p.y.ne[1]>r1);let off=col*p.y.nb[0]+r1*p.y.nb[1]+(head%p.y.ne[2])*p.y.nb[2]+(batch%p.y.ne[3])*p.y.nb[3];v+=slope*load_at(p.y,mask,off);}
                scratch[base+col]=v;if v>maximum {maximum=v;}
            }
            if p.ip[1]!=0 {let v=load(p.z,sinks,head);if v>maximum {maximum=v;}}
            var sum:f64=(0 as f64);
            for col in width {let v=exp32(scratch[base+col]-maximum);scratch[base+col]=v;sum+=v as f64;}
            if p.ip[1]!=0 {sum+=exp32(load(p.z,sinks,head)-maximum) as f64;}
            assert(sum>(0 as f64));let scale=((1 as f64)/sum) as f32;
            for col in width {store(p.d,dst,row*width+col,scratch[base+col]*scale);}
        }
        row+=p.nth;
    }
    return true;
}
