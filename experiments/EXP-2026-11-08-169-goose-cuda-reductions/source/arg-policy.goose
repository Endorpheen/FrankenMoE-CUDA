// Argmax host launch policy from native ggml_cuda_argmax;pure Goose,no imports,no launch effects.
struct Launch{ok:bool,bs:i32,blocks:i64,shared:i64}
fn minimum(a:i64,b:i64)->i64{return if a<b{a}else{b};}
// Native:std::min<int64_t>(1024,(ne00+WARP_SIZE-1)/WARP_SIZE*WARP_SIZE);result is a multiple of 32,at least 32.
fn block_size(cols:i64)->i32{return (minimum(1024,((cols+31)/32)*32)) as! i32;}
fn grid(rows:i64)->i64{return rows;}
// Device touches shared slots only when bs>32;256 bytes cover 32 float and 32 int slots.
fn shared_bytes(bs:i32)->i64{return if bs>32{256 as i64}else{0 as i64};}
// Own validation bounds:positive sizes;argmax must fit I32;rows within CUDA grid.x limit;rows*cols then stays inside i64.
fn valid(cols:i64,rows:i64)->bool{
 if cols<=0||rows<=0{return false;}
 if cols>2147483647||rows>2147483647{return false;}
 return true;
}
// Single query entry for the bridge:ok=false must reject before any launch effect.
fn policy(cols:i64,rows:i64)->Launch{
 if !valid(cols,rows){return Launch{false,0,0,0};}
 let bs=block_size(cols);
 return Launch{true,bs,grid(rows),shared_bytes(bs)};
}
// Dummy retained entry;parent must not execute it.
fn main(){policy(1,1);}