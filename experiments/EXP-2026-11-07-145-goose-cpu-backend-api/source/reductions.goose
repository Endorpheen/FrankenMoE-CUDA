fn reduction(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=OP_SUM&&p.op!=OP_SUM_ROWS&&p.op!=OP_MEAN&&p.op!=OP_CUMSUM&&p.op!=OP_ARGMAX&&p.op!=OP_COUNT_EQUAL {return false;}
    if p.op==OP_SUM {
        if p.ith==0 {var total:f64=(0 as f64);for row in rows(p.x) {var value:f64=(0 as f64);for col in p.x.ne[0] {value+=load(p.x,x,row*p.x.ne[0]+col) as f64;}total+=value;}store(p.d,dst,0,total as f32);}return true;
    }
    if p.op==OP_COUNT_EQUAL {
        assert(p.x.kind==26&&p.y.kind==26&&same_shape(p.x,p.y));
        if p.ith==0 {var total:i64=0;for i in elements(p.x) {if read32(x,offset(p.x,i))==read32(y,offset(p.y,i)) {total+=1;}}assert(p.d.bytes>=8);let u=total as u64;for i in 8 {dst[i]=(u>>((8*i) as u64)) as! u8;}}return true;
    }
    var row=thread_first(p);let width=p.x.ne[0];
    while row<rows(p.x) {
        if p.op==OP_ARGMAX {assert(width>0);var at:i64=0;var best=load(p.x,x,row*width);for col in width {let v=load(p.x,x,row*width+col);if v>best {best=v;at=col;}}assert(p.d.kind==26&&at<=0x7fffffff);write32(dst,offset(p.d,row),at as u32);}
        else if p.op==OP_CUMSUM {var value:f32=(0 as f32);for col in width {value+=load(p.x,x,row*width+col);store(p.d,dst,row*width+col,value);}}
        else {var total:f64=(0 as f64);for col in width {total+=load(p.x,x,row*width+col) as f64;}let value=if p.op==OP_MEAN {(total/(width as f64)) as f32} else {total as f32};store(p.d,dst,row,value);}
        row+=p.nth;
    }
    return true;
}
fn normalize(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=OP_NORM&&p.op!=OP_RMS_NORM&&p.op!=OP_L2_NORM&&p.op!=OP_RMS_NORM_BACK {return false;}
    assert(same_shape(p.x,p.d));let width=p.x.ne[0];assert(width>0);var row=thread_first(p);
    while row<rows(p.x) {
        var sum:f64=(0 as f64);var sq:f64=(0 as f64);var cross:f64=(0 as f64);
        for col in width {let v=load(p.x,x,row*width+col);sum+=v as f64;if p.op==OP_RMS_NORM_BACK {let a=load(p.y,y,row*width+col);sq+=(a*a) as f64;cross+=(a*v) as f64;}}
        let mean=if p.op==OP_NORM {(sum/(width as f64)) as f32} else {(0 as f32)};
        if p.op!=OP_RMS_NORM_BACK {for col in width {let v=load(p.x,x,row*width+col)-mean;sq+=(v*v) as f64;}}
        let variance=if p.op==OP_L2_NORM {sq as f32} else if p.op==OP_RMS_NORM_BACK {(sq as f32)/(width as f32)} else {(sq/(width as f64)) as f32};
        let scale=if p.op==OP_L2_NORM {(1 as f32)/max32(sqrt32(variance),p.fp[0])} else {(1 as f32)/sqrt32(variance+p.fp[0])};
        for col in width {
            let v=load(p.x,x,row*width+col);let value=if p.op==OP_RMS_NORM_BACK {(v-load(p.y,y,row*width+col)*(cross as f32)/((sq as f32)+p.fp[0]*(width as f32)))*scale} else {(v-mean)*scale};
            let result=if p.ip[14]!=0&&p.op==OP_RMS_NORM {value*load_at(p.y,y,broadcast(p.y,p.d,row*width+col))} else {value};store(p.d,dst,row*width+col,result);
        }
        row+=p.nth;
    }
    return true;
}
fn sort_rows(p:const Node&,x:const u8[:],dst:u8[:],indices:i64[:])->bool {
    if p.op!=OP_ARGSORT&&p.op!=OP_TOP_K {return false;}let n=p.x.ne[0];assert(p.d.kind==26&&p.d.ne[0]<=n);assert(indices.len>=mul_size(n,p.nth));var row=thread_first(p);let base=p.ith*n;
    while row<rows(p.x) {
        for i in n {indices[base+i]=i;}
        for i in n {var j=i;while j>0 {
            let ia=indices[base+j-1];let ib=indices[base+j];let a=load(p.x,x,row*n+ia);let b=load(p.x,x,row*n+ib);
            let ordered=if p.ip[0]==0 {a<=b} else {a>=b};if ordered {break;}indices[base+j-1]=ib;indices[base+j]=ia;j-=1;
        }}
        for i in p.d.ne[0] {assert(indices[base+i]<=0x7fffffff);write32(dst,offset(p.d,row*p.d.ne[0]+i),indices[base+i] as u32);}
        row+=p.nth;
    }
    return true;
}
