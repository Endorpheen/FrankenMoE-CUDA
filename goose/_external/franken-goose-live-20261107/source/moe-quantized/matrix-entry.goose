fn row_value(p:const Task&,first:i64,offset:i64,x2:const Q2[:],x5:const Q5[:],xn:const NL4[:],xx:const IQ2X[:],xs:const IQ2S[:],y0:const Y0[:],yk:const YK[:],gx:const u8[8][:],gs:const u8[8][:],signs:const u8[:],palette:const i8[:])->f32 {
    if p.kind==0 {return dot_q2(x2[first..first+p.blocks],y0[offset..offset+2*p.blocks]);}
    if p.kind==1 {return dot_q5(x5[first..first+p.blocks],y0[offset..offset+p.blocks]);}
    if p.kind==2 {return dot_nl(xn[first..first+p.blocks],y0[offset..offset+p.blocks],palette);}
    let out=dot_iq_rows(p.kind,xx,xs,first,yk,p.blocks,1,offset,0,0,gx,gs,signs);
    return out[0];
}
fn input_offset(p:const Task&,r:const Route&)->i64 {
    let slot=(r.slot as i64)%p.slots;let token=r.token as i64;
    if p.input_contiguous!=0 {return (slot+token*p.slots)*p.input_row_blocks;}
    return slot*p.input_stride1+token*p.input_stride2;
}
fn execute_matrix(p:const Task&,x2:const Q2[:],x5:const Q5[:],xn:const NL4[:],xx:const IQ2X[:],xs:const IQ2S[:],y0:const Y0[:],yk:const YK[:],dst:f32[:],gx:const u8[8][:],gs:const u8[8][:],signs:const u8[:],palette:const i8[:],routes:const Route[:]) {
    assert(p.kind>=0&&p.kind<=4&&p.blocks>=0);
    if p.mode==3&&(p.rows<=0||p.cols<=0) {witness(3,p.kind,p.blocks,0,0);return;}
    assert(p.rows>=0);
    if p.mode==2 {
        dequantize(p.kind,p.blocks,x2,x5,xn,xx,xs,dst,gx,gs,signs,palette);
    } else if p.mode==1 {
        assert(p.kind==4&&p.cols>=2&&p.cols<=3&&p.slots>0&&routes.len>=p.route_first+p.cols);
        let r0=routes[p.route_first];let r1=routes[p.route_first+1];
        let o0=input_offset(p,r0);let o1=input_offset(p,r1);var o2:i64=0;
        if p.cols==3 {o2=input_offset(p,routes[p.route_first+2]);}
        for row in p.rows {
            let values=dot_iq_rows(4,xx,xs,(p.first+row)*p.weight_stride,yk,p.blocks,p.cols,o0,o1,o2,gx,gs,signs);
            for col in p.cols {
                let r=routes[p.route_first+col];let offset=(r.slot as i64)*p.dst_stride1+(r.token as i64)*p.dst_stride2+p.first+row;
                dst[offset]=values[col];
            }
        }
    } else if p.mode==3 {
        assert(p.cols>=0&&p.slots>0&&routes.len>=p.route_first+p.cols);
        for col in p.cols {
            let route=routes[p.route_first+col];let offset=input_offset(p,route);
            let output=(route.slot as i64)*p.dst_stride1+(route.token as i64)*p.dst_stride2+p.first;
            for row in p.rows {dst[output+row]=row_value(p,(p.first+row)*p.weight_stride,offset,x2,x5,xn,xx,xs,y0,yk,gx,gs,signs,palette);}
        }
    } else {
        assert(p.mode==0);
        for row in p.rows {dst[row*p.output_stride]=row_value(p,(p.first+row)*p.weight_stride,0,x2,x5,xn,xx,xs,y0,yk,gx,gs,signs,palette);}
    }
    witness(p.mode,p.kind,p.blocks,p.rows,p.cols);
}
fn main() {
    var p:Task=default<Task>();p.blocks=1;p.rows=1;p.weight_stride=1;p.cols=1;p.slots=1;p.output_stride=1;
    let x2:Q2[1]=default<Q2[1]>();let x5:Q5[1]=default<Q5[1]>();let xn:NL4[1]=default<NL4[1]>();
    let xx:IQ2X[1]=default<IQ2X[1]>();let xs:IQ2S[1]=default<IQ2S[1]>();let y0:Y0[2]=default<Y0[2]>();let yk:YK[1]=default<YK[1]>();
    var out:f32[256]=default<f32[256]>();let grid:u8[8][1]=default<u8[8][1]>();let signs:u8[128]=default<u8[128]>();let palette:i8[16]=default<i8[16]>();let routes:Route[3]=default<Route[3]>();
    execute_matrix(p,x2,x5,xn,xx,xs,y0,yk,out,grid,grid,signs,palette,routes);print(out[0]);
}
