extern "fg141_quantize" fn convert(context:i64,source:i64,target:i64,values:i64);
extern "fg141_reset" fn reset(context:i64,value:i32);
extern "fg141_barrier" fn barrier(context:i64);
extern "fg141_claim" fn claim(context:i64)->i32;
fn checked_add(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=0x7fffffffffffffff-b);return a+b;}
fn checked_mul(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);assert(a==0||b<=0x7fffffffffffffff/a);return a*b;}
fn workspace_offset(p:const Plan&)->i64 {
    assert(p.width>0&&p.width%256==0&&p.n11>0&&p.n12>0&&p.n13>0&&p.nth>0&&p.ith>=0&&p.ith<p.nth&&p.workspace_mod>=0&&p.workspace_mod<64);
    var converted:i64=0;
    if p.need_quant!=0 {converted=checked_mul(checked_mul(checked_mul(p.width/256*292,p.n11),p.n12),p.n13);converted=(checked_add(converted,63)/64)*64;}
    let base=(checked_add(checked_add(p.workspace_mod,converted),63)/64)*64-p.workspace_mod;
    let offset=checked_add(base,checked_mul(p.ith,524288));assert(checked_add(offset,445440)<=p.wdata_bytes);return offset;
}
fn narrow_extent(rows:i64,width:i64)->i64 {
    let count=if rows<16 {16} else {rows};var extent=(31*1024)/(16+count);extent=(extent/256)*256;
    if extent>4096 {extent=4096;}
    while extent>=256&&width%extent!=0 {extent-=256;}
    return if extent<256 {0} else {extent};
}
fn repack_y(w:Workspace&,first:i64,nk:i64,apply:bool) {
    if apply {for row in 16 {for slab in nk {for j in 8 {let index=(first+row)*nk+slab;w.b.q[index][j]=biased(w.b.q[index][j]);}}}}
}
fn compute_window(p:const Plan&,first:i64,count:i64,colfirst:i64,columns:i64,x:const u8[:],y:const Q8[:],routes:const Route[:],w:Workspace&,dst:f32[:],gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:]) {
    assert(first>=0&&count>0&&count<=256&&colfirst>=0&&columns>0&&columns<=256);
    let blocks=p.width/256;let f=tile_traits(p.kind);var q:Plan=p;q.defer_bias=1;q.route_first=colfirst;q.input_first=colfirst;
    for i in round16(count) {for j in round16(columns)/8 {w.acc[i*32+j]=default<f32[8]>();}}
    if columns<=16 {
        let extent=narrow_extent(columns,p.width);assert(extent>0);let nk=extent/256;var kb:i64=0;
        while kb<blocks {
            q.count=columns;q.start=kb;q.nk=nk;gather_y(q,y,routes,w);repack_y(w,0,nk,f.act!=0);
            var row=first;
            while row<first+count {
                q.first=row;q.count=minimum(16,first+count-row);unpack_panel(q,x,w,gxx,gxs,gs,g3x,g3s,g1,signs,palette);
                for slab in nk {q.row0=0;q.col0=0;q.out0=row-first;q.slab=slab;micro(q,w);}
                row+=16;
            }
            kb+=nk;
        }
    } else {
        for kb in blocks {
            q.first=first;q.count=count;q.start=kb;q.nk=1;unpack_panel(q,x,w,gxx,gxs,gs,g3x,g3s,g1,signs,palette);
            q.count=columns;gather_y(q,y,routes,w);var col:i64=0;
            while col<columns {
                repack_y(w,col,1,f.act!=0);var row:i64=0;
                while row<count {q.row0=row;q.col0=col;q.out0=row;q.slab=0;micro(q,w);row+=16;}
                col+=16;
            }
        }
    }
    for col in columns {
        var base:i64=0;
        if p.dense!=0 {base=p.output_base+col*p.out_stride1+first;}
        else {let route=routes[colfirst+col];base=(route.slot as i64)*p.out_stride1+(route.token as i64)*p.out_stride2+first;}
        for row in count {dst[base+row]=w.acc[row*32+col/8][col%8];}
    }
    witness(3,p.kind,1);
}
fn run_moe(p:const Plan&,x:const u8[:],y:const Q8[:],routes:const Route[:],w:Workspace&,dst:f32[:],gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:])->bool {
    if p.enabled==0||(p.count<8&&p.forced==0) {return false;}
    assert(p.width>0&&p.width%256==0&&p.output_rows>0&&p.nth>0&&p.ith>=0&&p.ith<p.nth&&p.slots>0&&p.count>=0&&p.count<=routes.len&&p.y_stride>=p.width/256&&p.origin>=0);
    let groups=(p.output_rows+63)/64;let begin=groups*p.ith/p.nth;let end=groups*(p.ith+1)/p.nth;var q:Plan=p;q.dense=0;var col:i64=0;
    while col<p.count {
        let columns=minimum(256,p.count-col);
        for group in end-begin {let first=(begin+group)*64;let count=minimum(64,p.output_rows-first);compute_window(q,first,count,col,columns,x,y,routes,w,dst,gxx,gxs,gs,g3x,g3s,g1,signs,palette);}
        col+=256;
    }
    witness(4,p.kind,1);return true;
}
fn dense_chunk(p:const Plan&,first0:i64,last0:i64,first1:i64,last1:i64,x:const u8[:],y:const Q8[:],routes:const Route[:],w:Workspace&,dst:f32[:],gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:]) {
    if first0>=last0||first1>=last1 {return;}
    assert(p.n02>0&&p.n03>0&&p.n12%p.n02==0&&p.n13%p.n03==0);var q:Plan=p;q.dense=1;
    let r2=p.n12/p.n02;let r3=p.n13/p.n03;var col=first1;
    while col<last1 {
        let end=minimum(minimum(col+256,last1),(col/p.n11+1)*p.n11);
        let i13=col/(p.n12*p.n11);let i12=(col-i13*p.n12*p.n11)/p.n11;let i11=col-i13*p.n12*p.n11-i12*p.n11;
        q.origin=(i12/r2)*p.nb02+(i13/r3)*p.nb03;q.output_base=i12*p.out_stride2+i13*p.out_stride3+i11*p.out_stride1;
        var row=first0;
        while row<last0 {let count=minimum(256,last0-row);compute_window(q,row,count,col,end-col,x,y,routes,w,dst,gxx,gxs,gs,g3x,g3s,g1,signs,palette);row+=256;}
        col=end;
    }
}
fn run_dense(p:const Plan&,x:const u8[:],y:const Q8[:],routes:const Route[:],w:Workspace&,dst:f32[:],gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:])->bool {
    if p.enabled==0||(p.n11<8&&p.forced==0) {return false;}
    assert(p.width>0&&p.width%256==0&&p.output_rows>0&&p.n11>0&&p.n12>0&&p.n13>0&&p.nth>0&&p.ith>=0&&p.ith<p.nth);
    if p.need_quant!=0 {
        let rowbytes=p.width/256*292;let nb2=rowbytes*p.n11;let nb3=nb2*p.n12;
        let first=(p.ith*p.width/256)/p.nth;let last=((p.ith+1)*p.width/256)/p.nth;
        for sample in p.n13 {for token in p.n12 {for row in p.n11 {let source=sample*p.src_nb3+token*p.src_nb2+row*p.src_nb1+first*256*p.src_nb0;let target=sample*nb3+token*nb2+row*rowbytes+first*292;convert(p.context,source,target,(last-first)*256);}}}
    }
    if p.ith==0 {reset(p.context,p.nth as! i32);}barrier(p.context);
    let nr0=p.output_rows;let nr1=p.n11*p.n12*p.n13;var size:i64=256;var nc0=(nr0+size-1)/size;var nc1=(nr1+size-1)/size;
    while nc0*nc1<p.nth*4&&size>16 {size/=2;nc0=(nr0+size-1)/size;nc1=(nr1+size-1)/size;}
    let dr0=(nr0+nc0-1)/nc0;let dr1=(nr1+nc1-1)/nc1;var current=p.ith;
    while current<nc0*nc1 {
        let r0=dr0*(current%nc0);let r1=dr1*(current/nc0);dense_chunk(p,r0,minimum(r0+dr0,nr0),r1,minimum(r1+dr1,nr1),x,y,routes,w,dst,gxx,gxs,gs,g3x,g3s,g1,signs,palette);
        if p.nth>=nc0*nc1 {break;}current=claim(p.context) as i64;
    }
    witness(5,p.kind,1);return true;
}
