fn gelu(x:f32)->f32 {return (0.5 as f32)*x*((1 as f32)+tanh32((0.7978845608028654 as f32)*x*((1 as f32)+(0.044715 as f32)*x*x)));}
fn gelu_erf(x:f32)->f32 {return (0.5 as f32)*x*((1 as f32)+erf32(x*(0.7071067811865475 as f32)));}
fn gelu_quick(x:f32)->f32 {return x/((1 as f32)+exp32(-(1.702 as f32)*x));}
fn silu(x:f32)->f32 {return x/((1 as f32)+exp32(-x));}
fn unary_value(p:const Node&,op:i64,x:f32)->f32 {
    if op==UNARY_OP_ABS {return abs32(x);}
    if op==UNARY_OP_SGN {return if x>(0.0 as f32) {(1.0 as f32)} else if x<(0.0 as f32) {-(1.0 as f32)} else {(0.0 as f32)};}
    if op==UNARY_OP_NEG {return -x;}
    if op==UNARY_OP_STEP {return if x>(0.0 as f32) {(1.0 as f32)} else {(0.0 as f32)};}
    if op==UNARY_OP_TANH {return tanh32(x);}
    if op==UNARY_OP_ELU {return if x>(0.0 as f32) {x} else {expm132(x)};}
    if op==UNARY_OP_RELU {return if x>(0.0 as f32) {x} else {(0.0 as f32)};}
    if op==UNARY_OP_SIGMOID {return (1.0 as f32)/((1.0 as f32)+exp32(-x));}
    if op==UNARY_OP_HARDSIGMOID {return min32((1.0 as f32),max32((0.0 as f32),(x+(3.0 as f32))/(6.0 as f32)));}
    if op==UNARY_OP_EXP {return exp32(x);}
    if op==UNARY_OP_HARDSWISH {return x*min32((1.0 as f32),max32((0.0 as f32),(x+(3.0 as f32))/(6.0 as f32)));}
    if op==UNARY_OP_EXPM1 {return exp32(x)-(1.0 as f32);}
    if op==UNARY_OP_SOFTPLUS {return if x>(20.0 as f32) {x} else {log32((1.0 as f32)+exp32(x))};}
    if op==UNARY_OP_FLOOR {return floor32(x);}
    if op==UNARY_OP_CEIL {return ceil32(x);}
    if op==UNARY_OP_ROUND {return round32(x);}
    if op==UNARY_OP_TRUNC {return trunc32(x);}
    if op==UNARY_OP_XIELU {return if x>(0.0 as f32) {fma32(p.fp[1]*x,x,p.fp[2]*x)} else {fma32(expm132(min32(x,p.fp[3]))-x,p.fp[0],p.fp[2]*x)};}
    if op==UNARY_OP_GELU {return gelu(x);}if op==UNARY_OP_GELU_ERF {return gelu_erf(x);}if op==UNARY_OP_GELU_QUICK {return gelu_quick(x);}assert(op==UNARY_OP_SILU);return silu(x);
}
fn pointwise(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=OP_ADD&&p.op!=OP_ADD1&&p.op!=OP_SUB&&p.op!=OP_MUL&&p.op!=OP_DIV&&p.op!=OP_SCALE&&p.op!=OP_SQR&&p.op!=OP_SQRT&&p.op!=OP_SIN&&p.op!=OP_COS&&p.op!=OP_LOG&&p.op!=OP_CLAMP&&p.op!=OP_LEAKY_RELU&&p.op!=OP_UNARY&&p.op!=OP_SILU_BACK {return false;}
    var i=thread_first(p);let count=elements(p.d);
    while i<count {
        let a=load_at(p.x,x,broadcast(p.x,p.d,i));var value:f32=(0 as f32);
        if p.op==OP_ADD||p.op==OP_ADD1||p.op==OP_SUB||p.op==OP_MUL||p.op==OP_DIV||p.op==OP_SILU_BACK {
            let b=load_at(p.y,y,broadcast(p.y,p.d,i));
            if p.op==OP_ADD||p.op==OP_ADD1 {value=a+b;}
            else if p.op==OP_SUB {value=a-b;}else if p.op==OP_MUL {value=a*b;}else if p.op==OP_DIV {value=a/b;}
            else {let sig=(1 as f32)/((1 as f32)+exp32(-a));value=b*sig*((1 as f32)+a*((1 as f32)-sig));}
        }else if p.op==OP_SCALE {value=a*p.fp[0]+p.fp[1];}
        else if p.op==OP_SQR {value=a*a;}else if p.op==OP_SQRT {value=sqrt32(a);}
        else if p.op==OP_SIN {value=sin32(a);}else if p.op==OP_COS {value=cos32(a);}else if p.op==OP_LOG {value=log32(a);}
        else if p.op==OP_CLAMP {value=min32(max32(a,p.fp[0]),p.fp[1]);}
        else if p.op==OP_LEAKY_RELU {value=if a>(0 as f32) {a} else {a*p.fp[0]};}
        else {value=unary_value(p,p.ip[0],a);}
        store(p.d,dst,i,value);i+=p.nth;
    }
    return true;
}
fn glu(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=OP_GLU {return false;}let width=p.d.ne[0];assert(width>0&&rows(p.x)==rows(p.d));let separate=p.ip[2]!=0;let swapped=p.ip[1]!=0;
    if separate {assert(same_shape(p.x,p.y)&&same_shape(p.x,p.d));}else {assert(p.x.ne[0]==2*width);}
    var row=thread_first(p);
    while row<rows(p.d) {
        for col in width {
            let a=load(p.x,x,row*p.x.ne[0]+col+(if separate||!swapped {0} else {width}));
            let b=if separate {load(p.y,y,row*p.y.ne[0]+col)} else {load(p.x,x,row*p.x.ne[0]+col+(if swapped {0} else {width}))};
            let op=p.ip[0];var value:f32=(0 as f32);
            if op==GLU_OP_REGLU {value=(if a>(0 as f32) {a} else {(0 as f32)})*b;}
            else if op==GLU_OP_GEGLU {value=gelu(a)*b;}else if op==GLU_OP_GEGLU_ERF {value=gelu_erf(a)*b;}else if op==GLU_OP_GEGLU_QUICK {value=gelu_quick(a)*b;}
            else if op==GLU_OP_SWIGLU {value=silu(a)*b;}
            else if op==GLU_OP_SWIGLU_OAI {let aa=min32(a,p.fp[1]);let bb=max32(min32(b,p.fp[1]),-p.fp[1]);value=(aa/((1 as f32)+exp32(-p.fp[0]*aa)))*((1 as f32)+bb);}
            else {assert(op==GLU_OP_SWIGLU_CLAMP);let aa=min32(a,p.fp[0]);let bb=min32(max32(b,-p.fp[0]),p.fp[0]);value=silu(aa)*bb;}
            store(p.d,dst,row*width+col,value);
        }
        row+=p.nth;
    }
    return true;
}
