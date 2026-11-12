fn transforms(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    let op=p.op;
    if op!=OP_DUP&&op!=OP_CPY&&op!=OP_CONT&&op!=OP_REPEAT&&op!=OP_CONCAT&&op!=OP_PAD&&op!=OP_PAD_REFLECT_1D&&op!=OP_ROLL&&op!=OP_ARANGE&&op!=OP_FILL&&op!=OP_DIAG&&op!=OP_DIAG_MASK_INF&&op!=OP_DIAG_MASK_ZERO&&op!=OP_GET_ROWS&&op!=OP_SET_ROWS {return false;}
    var index=thread_first(p);let count=elements(p.d);
    if (op==OP_DUP||op==OP_CPY||op==OP_CONT)&&p.x.kind==p.d.kind {
        assert(count==elements(p.x));let bytes=scalar_bytes(p.x.kind);
        while index<count {let source=offset(p.x,index);let target=offset(p.d,index);for byte in bytes {dst[target+byte]=x[source+byte];}index+=p.nth;}
        return true;
    }

    while index<count {
        var n=index;var c:i64[4]=default<i64[4]>();for axis in 4 {c[axis]=n%p.d.ne[axis];n/=p.d.ne[axis];}
        var value:f32=(0 as f32);var target=index;
        if op==OP_FILL {value=p.fp[0];}
        else if op==OP_ARANGE {value=p.fp[0]+p.fp[2]*(index as f32);}
        else if op==OP_REPEAT {value=load_at(p.x,x,broadcast(p.x,p.d,index));}
        else if op==OP_CONCAT {
            let axis=p.ip[0];assert(axis>=0&&axis<4);var source=c;
            if c[axis]<p.x.ne[axis] {var offset:i64=0;for k in 4 {offset=add_size(offset,mul_size(source[k],p.x.nb[k]));}value=load_at(p.x,x,offset);}
            else {source[axis]-=p.x.ne[axis];var offset:i64=0;for k in 4 {offset=add_size(offset,mul_size(source[k],p.y.nb[k]));}value=load_at(p.y,y,offset);}
        }else if op==OP_PAD {var valid:bool=true;var offset:i64=0;for axis in 4 {if c[axis]>=p.x.ne[axis] {valid=false;}else {offset=add_size(offset,mul_size(c[axis],p.x.nb[axis]));}}if valid {value=load_at(p.x,x,offset);}}
        else if op==OP_PAD_REFLECT_1D {let left=p.ip[0];let at=c[0]-left;let reflected=if at<0 {-at} else if at>=p.x.ne[0] {2*(p.x.ne[0]-1)-at} else {at};assert(reflected>=0&&reflected<p.x.ne[0]);value=load_at(p.x,x,reflected*p.x.nb[0]+c[1]*p.x.nb[1]+c[2]*p.x.nb[2]+c[3]*p.x.nb[3]);}
        else if op==OP_ROLL {var offset:i64=0;for axis in 4 {let size=p.x.ne[axis];assert(size>0);let at=((c[axis]-p.ip[axis])%size+size)%size;offset=add_size(offset,mul_size(at,p.x.nb[axis]));}value=load_at(p.x,x,offset);}
        else if op==OP_DIAG {if c[0]==c[1] {value=load(p.x,x,c[0]+p.x.ne[0]*(c[2]+p.x.ne[2]*c[3]));}}
        else if op==OP_DIAG_MASK_INF||op==OP_DIAG_MASK_ZERO {value=load(p.x,x,index);if c[0]>p.ip[0]+c[1] {value=if op==OP_DIAG_MASK_INF {from_bits(0xff800000)} else {(0 as f32)};}}
        else if op==OP_GET_ROWS {let row=index/p.d.ne[0];let id=read32(y,offset(p.y,row)) as! i32;assert(id>=0&&(id as i64)<p.x.ne[1]);let ix=c[0]*p.x.nb[0]+(id as i64)*p.x.nb[1]+c[2]*p.x.nb[2]+c[3]*p.x.nb[3];value=load_at(p.x,x,ix);}
        else if op==OP_SET_ROWS {return false;}
        else {assert(count==elements(p.x));value=load(p.x,x,index);}
        store(p.d,dst,target,value);index+=p.nth;
    }
    return true;
}
