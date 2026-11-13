fn vector_dot(a:const Tensor&,x:const u8[:],ax:i64,as0:i64,b:const Tensor&,y:const u8[:],by:i64,bs0:i64,count:i64)->f32 {
    assert(count>=0);var acc:f32[8][4]=default<f32[8][4]>();let end=(count/32)*32;var k:i64=0;
    while k<end {for group in 4 {for lane in 8 {let i=k+8*group+lane;let va=load_at(a,x,ax+i*as0);let vb=load_at(b,y,by+i*bs0);acc[group][lane]=if a.kind==30 {acc[group][lane]+va*vb} else {fma32(va,vb,acc[group][lane])};}}k+=32;}
    var v:f32[8]=default<f32[8]>();for lane in 8 {v[lane]=(acc[0][lane]+acc[2][lane])+(acc[1][lane]+acc[3][lane]);}
    let s0=v[0]+v[4];let s1=v[1]+v[5];let s2=v[2]+v[6];let s3=v[3]+v[7];
    if a.kind==0 {var result=(s0+s1)+(s2+s3);while k<count {result+=load_at(a,x,ax+k*as0)*load_at(b,y,by+k*bs0);k+=1;}return result;}
    var result=if a.kind==30 {((s0+s2)+(s1+s3)) as f64} else {((s0+s1)+(s2+s3)) as f64};
    while k<count {result+=(load_at(a,x,ax+k*as0)*load_at(b,y,by+k*bs0)) as f64;k+=1;}return result as f32;
}
fn matrix(p:const Node&,x:const u8[:],y:const u8[:],ids:const u8[:],dst:u8[:])->bool {
    if p.op!=OP_MUL_MAT&&p.op!=OP_MUL_MAT_ID&&p.op!=OP_OUT_PROD {return false;}
    if p.x.kind!=0&&p.x.kind!=1&&p.x.kind!=30 {return false;}if p.y.kind!=0&&p.y.kind!=1&&p.y.kind!=30 {return false;}
    let d0=p.d.ne[0];assert(d0>0&&p.d.kind==0);var row=thread_first(p);
    while row<rows(p.d) {
        let i1=row%p.d.ne[1];let i2=(row/p.d.ne[1])%p.d.ne[2];let i3=row/(p.d.ne[1]*p.d.ne[2]);
        for i0 in d0 {
            var xa:i64=0;var ya:i64=0;var xs:i64=0;var ys:i64=0;var count:i64=0;
            if p.op==OP_MUL_MAT {
                assert(p.x.ne[0]==p.y.ne[0]&&p.x.ne[1]==d0&&p.x.ne[2]>0&&p.x.ne[3]>0&&p.y.ne[2]%p.x.ne[2]==0&&p.y.ne[3]%p.x.ne[3]==0);
                let x2=i2/(p.y.ne[2]/p.x.ne[2]);let x3=i3/(p.y.ne[3]/p.x.ne[3]);xa=i0*p.x.nb[1]+x2*p.x.nb[2]+x3*p.x.nb[3];ya=i1*p.y.nb[1]+i2*p.y.nb[2]+i3*p.y.nb[3];xs=p.x.nb[0];ys=p.y.nb[0];count=p.x.ne[0];
            }else if p.op==OP_MUL_MAT_ID {
                assert(p.z.kind==26&&p.x.ne[0]==p.y.ne[0]&&p.x.ne[1]==d0&&p.y.ne[1]>0);let id=read32(ids,offset(p.z,i1+p.z.ne[0]*i2)) as! i32;assert(id>=0&&(id as i64)<p.x.ne[2]);xa=i0*p.x.nb[1]+(id as i64)*p.x.nb[2];ya=(i1%p.y.ne[1])*p.y.nb[1]+i2*p.y.nb[2];xs=p.x.nb[0];ys=p.y.nb[0];count=p.x.ne[0];
            }else {
                assert(p.x.ne[1]==p.y.ne[1]&&p.x.ne[0]==d0&&p.y.ne[0]==p.d.ne[1]);let x2=i2%p.x.ne[2];let x3=i3%p.x.ne[3];let y2=i2%p.y.ne[2];let y3=i3%p.y.ne[3];xa=i0*p.x.nb[0]+x2*p.x.nb[2]+x3*p.x.nb[3];ya=i1*p.y.nb[0]+y2*p.y.nb[2]+y3*p.y.nb[3];xs=p.x.nb[1];ys=p.y.nb[1];count=p.x.ne[1];
            }
            store(p.d,dst,row*d0+i0,vector_dot(p.x,x,xa,xs,p.y,y,ya,ys,count));
        }
        row+=p.nth;
    }
    return true;
}
