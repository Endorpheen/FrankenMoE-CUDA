fn entry(p:const Node&,x:const u8[:],y:const u8[:],z:const u8[:],dst:u8[:],scratch:f32[:],indices:i64[:],gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:])->bool {
    assert(p.nth>0&&p.ith>=0&&p.ith<p.nth);
    if p.ip[15]==1464816203 {
        assert(indices.len>=4);var nf:i64=0;var ni:i64=0;
        if p.op==OP_SOFT_MAX||p.op==OP_SOFT_MAX_BACK||p.op==OP_ROPE||p.op==OP_ROPE_BACK {nf=mul_size(p.x.ne[0],p.nth);}
        if p.op==OP_ARGSORT||p.op==OP_TOP_K {ni=mul_size(p.x.ne[0],p.nth);}
        if (p.op==OP_DUP||p.op==OP_CPY||p.op==OP_CONT||p.op==OP_GET_ROWS)&&!scalar_type(p.x)&&p.d.kind==0 {nf=mul_size(p.x.ne[0],p.nth);}
        let byte=mul_size(nf,4);let aligned=mul_size(add_size(byte,7)/8,8);indices[0]=nf;indices[1]=ni;indices[2]=aligned;indices[3]=add_size(aligned,mul_size(ni,8));return true;
    }
    if codec_transform(p,x,y,dst,scratch,gxx,gxs,gs,g3x,g3s,g1,signs,palette) {witness(p.op,elements(p.d));return true;}
    if !scalar_type(p.x)||!scalar_type(p.d) {return false;}
    validate(p.d,dst.len);validate(p.x,x.len);
    if p.y.bytes>0 {validate(p.y,y.len);}if p.z.bytes>0 {validate(p.z,z.len);}
    if transforms(p,x,y,dst) {witness(p.op,elements(p.d));return true;}
    if p.x.kind==27||p.x.kind==28||p.d.kind==27||p.d.kind==28 {if p.op!=OP_COUNT_EQUAL {return false;}}
    let handled=indexed(p,x,y,dst)||image_ops(p,x,y,dst)||rope(p,x,y,z,dst,scratch)||matrix(p,x,y,z,dst)||ssm_conv(p,x,y,dst)||pointwise(p,x,y,dst)||glu(p,x,y,dst)||reduction(p,x,y,dst)||normalize(p,x,y,dst)||group_normalize(p,x,dst)||softmax(p,x,y,z,dst,scratch)||sort_rows(p,x,dst,indices);
    if handled {witness(p.op,elements(p.d));}return handled;
}
fn main() {
    var p:Node=default<Node>();p.nth=1;
    let x:u8[4]=default<u8[4]>();var d:u8[4]=default<u8[4]>();var f:f32[1]=default<f32[1]>();var i:i64[1]=default<i64[1]>();
    let g8:u8[8][1]=default<u8[8][1]>();let g4:u8[4][1]=default<u8[4][1]>();let g1:i8[8][1]=default<i8[8][1]>();let signs:u8[128]=default<u8[128]>();let palette:i8[16]=default<i8[16]>();let ok=entry(p,x,x,x,d,f,i,g8,g8,g8,g4,g4,g1,signs,palette);print(ok);
}
