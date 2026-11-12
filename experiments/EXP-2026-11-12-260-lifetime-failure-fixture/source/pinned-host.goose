// Pinned-memory and host-registration policy in Goose, from ggml-cuda.cu:
//   ggml_cuda_host_malloc
//   ggml_backend_cuda_host_buffer_type_alloc_buffer
//   ggml_backend_cuda_register_host_buffer
//   ggml_backend_cuda_unregister_host_buffer
//
// Confirmed on the live path by probe, not by reading. An LD_PRELOAD counter over the
// CUDA runtime entry points recorded in one Apex run:
//
//   cudaMallocHost=3 bytes=677719168 cudaFreeHost=3
//   cudaHostRegister=120 cudaHostUnregister=120
//   cudaMemcpyAsync_h2d=60077 cudaMemcpyAsync_d2h=10962
//
// so both the pinned allocator and the registration path execute during inference.
//
// Goose owns the decisions: whether to attempt at all, what the failure log says, whether a
// failed allocation falls back to the CPU buffer type, and the reverse decision on free and
// unregister. The cudaMallocHost/cudaHostRegister/cudaHostUnregister calls stay with the
// caller as extern effects -- they are the hardware boundary.
//
// The MiB figure in the failure log is computed here in integer thousandths rather than in
// floating point: the native code prints size/1024/1024 with printf, and a fixed-point value
// gives the same digits without dragging float formatting into Goose.
extern "fg196p_fail" fn fail(code:i64);
extern "fg196p_st_i64" fn st_i64(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// MiB in thousandths: size * 1000 / 1048576, split so the product cannot overflow.
fn mib_milli(size:i64)->i64{
 ensure(size>=0,20);
 let mib:i64=size/1048576 as i64;
 let rem:i64=size%1048576 as i64;
 return mib*1000 as i64+rem*1000 as i64/1048576 as i64;
}

// Host pinning plan. GGML_CUDA_NO_PINNED set means the allocator is off entirely.
//   r[0] attempt (0 = return null without touching the driver)
//   r[1] MiB thousandths, used only if the attempt fails
fn plan_host_alloc(disabled:i64,size:i64,r:u64){
 ensure(size>=0,21);
 st_i64(r,8,mib_milli(size));
 // A zero-byte request cannot produce a buffer. Native cudaMallocHost(0) fails and the caller falls
 // back to the CPU buffer type; refusing here takes the same route without touching the driver and
 // matches the VMM plan, which rejects size <= 0 (EXP-195, ensure code 10).
 if disabled!=0 || size==0 {
  st_i64(r,0,0);
  return;
 }
 st_i64(r,0,1);
}

// Outcome of the driver call; err 0 is success.
//   r[2] ok -- the pointer is handed back to ggml as a host buffer
//   r[3] log -- clear the sticky error and print the MiB figure
fn plan_host_alloc_result(err:i64,r:u64){
 if err==0 {
  st_i64(r,16,1);
  st_i64(r,24,0);
  return;
 }
 st_i64(r,16,0);
 st_i64(r,24,1);
}

// After a failed pin, ggml_backend_cuda_host_buffer_type_alloc_buffer falls back to the CPU
// buffer type rather than failing the model load.
//   r[0] use the CPU buffer type
fn plan_fallback(ptr_zero:i64,r:u64){
 if ptr_zero!=0 {
  st_i64(r,0,1);
  return;
 }
 st_i64(r,0,0);
}

// Registration plan. GGML_CUDA_REGISTER_HOST is opt-in, and the registration itself needs
// CUDART >= 11.10, which this build has.
//   r[0] attempt
//   r[1] MiB thousandths for the failure log
fn plan_register(enabled:i64,size:i64,r:u64){
 ensure(size>=0,22);
 st_i64(r,8,mib_milli(size));
 if enabled==0 {
  st_i64(r,0,0);
  return;
 }
 st_i64(r,0,1);
}

//   r[2] ok -- reported back to ggml as "this range is registered"
//   r[3] log failure
fn plan_register_result(err:i64,r:u64){
 if err==0 {
  st_i64(r,16,1);
  st_i64(r,24,0);
  return;
 }
 st_i64(r,16,0);
 st_i64(r,24,1);
}

// Unregistration. A failed unregister is silent in the native code: the error is cleared and
// nothing is logged, so r[3] stays 0 here.
//   r[0] call, r[2] ok, r[3] clear the error
fn plan_unregister(enabled:i64,err:i64,r:u64){
 if enabled==0 {
  st_i64(r,0,0);
  st_i64(r,16,1);
  st_i64(r,24,0);
  return;
 }
 st_i64(r,0,1);
 if err==0 {
  st_i64(r,16,1);
  st_i64(r,24,0);
  return;
 }
 st_i64(r,16,0);
 st_i64(r,24,1);
}

// Every entry point is called once from main with distinct arguments: the producer emits a
// function per body and folds identical bodies together, so a body never called from main
// does not get extracted at all.
fn main(){
 plan_host_alloc(0 as i64,0 as i64,0 as u64);
 plan_host_alloc_result(0 as i64,0 as u64);
 plan_fallback(0 as i64,0 as u64);
 plan_register(0 as i64,0 as i64,0 as u64);
 plan_register_result(0 as i64,0 as u64);
 plan_unregister(0 as i64,0 as i64,0 as u64);
}
