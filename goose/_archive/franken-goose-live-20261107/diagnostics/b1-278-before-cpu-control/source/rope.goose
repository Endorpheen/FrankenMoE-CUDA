fn yarn_pair(theta:f32,scale:f32,corr:f32[2],i:i64,ext:f32,amplitude:f32)->f32[2] {
    var angle=theta*scale;var magnitude=amplitude;
    if ext!=(0 as f32) {let ramp=(1 as f32)-min32((1 as f32),max32((0 as f32),((i/2) as f32-corr[0])/max32((0.001 as f32),corr[1]-corr[0])));let mix=ramp*ext;angle=angle*((1 as f32)-mix)+theta*mix;magnitude*=(1 as f32)+(0.1 as f32)*log32((1 as f32)/scale);}
    var out:f32[2]=default<f32[2]>();out[0]=cos32(angle)*magnitude;out[1]=sin32(angle)*magnitude;return out;
}
fn rope(p:const Node&,x:const u8[:],positions:const u8[:],factors:const u8[:],dst:u8[:],cache:f32[:])->bool {
    if p.op!=OP_ROPE&&p.op!=OP_ROPE_BACK {return false;}assert(same_shape(p.x,p.d)&&p.y.kind==26&&(p.x.kind==0||p.x.kind==1));let dims=p.ip[0];let mode=p.ip[1];let original=p.ip[2];let offs=p.ip[3];let width=p.x.ne[0];assert(dims>0&&dims%2==0&&offs>=0&&offs%2==0&&offs+dims<=width&&width%2==0);assert(cache.len>=mul_size(width,p.nth));assert(p.fp[0]>(0 as f32)&&p.fp[1]>(0 as f32));let vision=mode==24;let multiple=(mode&8)!=0;let interleave=mode==40;
    if vision {assert(dims==width/2&&offs==0);}assert(mode==0||mode==2||mode==8||mode==24||mode==40);
    let theta_scale=pow32(p.fp[0],-(2 as f32)/(dims as f32));let sign=if p.op==OP_ROPE {(1 as f32)} else {-(1 as f32)};
    let pi2=(6.283185307179586 as f32);var corr:f32[2]=default<f32[2]>();let divisor=(2 as f32)*log32(p.fp[0]);corr[0]=max32((0 as f32),floor32((dims as f32)*log32((original as f32)/(p.fp[4]*pi2))/divisor));corr[1]=min32((dims-1) as f32,ceil32((dims as f32)*log32((original as f32)/(p.fp[5]*pi2))/divisor));
    let base=p.ith*width;var row=thread_first(p);
    while row<rows(p.x) {
        let token=(row/p.x.ne[1])%p.x.ne[2];var initial:f32[4]=default<f32[4]>();var theta:f32[4]=default<f32[4]>();
        for axis in 4 {let at=token+(if multiple {axis*p.x.ne[2]} else {0});initial[axis]=(read32(positions,offset(p.y,at)) as! i32) as f32;theta[axis]=initial[axis];}
        let total=p.ip[4]+p.ip[5]+p.ip[6]+p.ip[7];if multiple {assert(total>0&&total<=width);}
        for pair in width/2 {
            let i=2*pair;var angle=theta[0];
            if multiple {
                let sector=pair%total;let secw=p.ip[4]+p.ip[5];let sece=secw+p.ip[6];
                if vision {if sector==0 {theta[0]=initial[0];}else if sector==p.ip[4] {theta[1]=initial[1];}else if sector==secw {theta[2]=initial[2];}else if sector==sece {theta[3]=initial[3];}}
                if interleave {if sector%3==1&&sector<3*p.ip[5] {angle=theta[1];}else if sector%3==2&&sector<3*p.ip[6] {angle=theta[2];}else if sector%3==0&&sector<3*p.ip[4] {angle=theta[0];}else {angle=theta[3];}}
                else {let axis=if sector<p.ip[4] {0} else if sector<secw {1} else if sector<sece {2} else {3};angle=theta[axis];}
            }
            let ff=if p.ip[8]!=0 {load(p.z,factors,pair)} else {(1 as f32)};let rotation=yarn_pair(angle/ff,p.fp[1],corr,i,p.fp[2],p.fp[3]);cache[base+i]=rotation[0];cache[base+i+1]=rotation[1]*sign;for axis in 4 {theta[axis]*=theta_scale;}
        }
        let rotated=if vision {width} else {dims};let distance=if mode==0 {1} else if vision {dims} else {dims/2};let stride=if mode==0 {1} else {2};
        for pair in rotated/2 {let i=2*pair;let index=offs+i/stride;let a=load(p.x,x,row*width+index);let b=load(p.x,x,row*width+index+distance);let c=cache[base+i];let s=cache[base+i+1];store(p.d,dst,row*width+index,a*c-b*s);store(p.d,dst,row*width+index+distance,a*s+b*c);}
        if !vision {let bytes=scalar_bytes(p.x.kind);for col in width {if col<offs||col>=offs+dims {let source=offset(p.x,row*width+col);let target=offset(p.d,row*width+col);for byte in bytes {dst[target+byte]=x[source+byte];}}}}
        row+=p.nth;
    }
    return true;
}
