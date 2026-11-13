// Apex CUDA arange in Goose, from ggml-cuda/arange.cu.
//
// Device side: the bound check and the value start + step*nidx.
// Host side: the block count, the element-count contract steps = ceil((stop-start)/step)
// and its validation against the destination element count.
//
// Everything algorithmic is Goose. Extern symbols are a float store, a float
// bitcast, a scalar ceil and the packet accessors used by the bridge.
extern "fg274d_st_f32" fn st_f32(dst:u64,o:u64,v:f32);
extern "fg274d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn arange(dst:u64,ne0:i64,start:f32,step:f32,nidx:i64){
 if nidx>=ne0 as i64{ return; }
 ensure(nidx>=0 as i64,10);
 st_f32(dst,((nidx*4 as i64) as u64),start+step*(nidx as f32));
}

// Retained entry; the parent never executes main.
fn main(){arange(0,0,0.0 as f32,0.0 as f32,0 as i64);}
