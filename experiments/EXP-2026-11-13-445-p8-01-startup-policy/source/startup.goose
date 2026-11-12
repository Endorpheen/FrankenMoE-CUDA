// Server startup/config policy in Goose, from tools/server/server.cpp and
// server-context.cpp.
//
// Owned decisions, in the native order:
//   1. router mode = no model path, repo or docker repo;
//   2. inside !router: embeddings clamp n_batch down to n_ubatch;
//   3. inside !router: n_parallel < 0 -> 4 and kv_unified = true;
//   4. outside the router guard: kv-unified-per-slot sizes n_ctx when the user
//      did not pin it (-c 0) and fit did not pin a minimum context;
//   5. outside the router guard: empty alias falls back to the model name;
//   6. n_ctx_slot = min(ctx_seq, kv_unified_per_slot if > 0, ctx_train).
//
// Outputs eight i64 words: valid, n_batch, n_ubatch, n_parallel, kv_unified,
// n_ctx, alias_use, n_ctx_slot.
extern "fg445_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

fn decide(has_model:i64,embedding:i64,n_batch:i64,n_ubatch:i64,
          n_parallel:i64,kv_unified:i64,n_ctx:i64,kv_per_slot:i64,fit_min_ctx:i64,
          alias_empty:i64,model_name_len:i64,ctx_seq:i64,ctx_train:i64,out:u64)->i64{
 var valid:i64=1 as i64;
 var nb:i64=n_batch;
 var nub:i64=n_ubatch;
 var np:i64=n_parallel;
 var ku:i64=kv_unified;
 var nc:i64=n_ctx;
 var alias_use:i64=0 as i64;
 if nb <= 0 as i64 {valid=0 as i64;}
 if nub <= 0 as i64 {valid=0 as i64;}
 if nc < 0 as i64 {valid=0 as i64;}
 if kv_per_slot < 0 as i64 {valid=0 as i64;}
 if ctx_seq <= 0 as i64 {valid=0 as i64;}
 if ctx_train <= 0 as i64 {valid=0 as i64;}
 // router mode skips only the model-dependent startup adjustments.
 if has_model != 0 as i64 {
  if embedding != 0 as i64 && nb > nub { nb = nub; }
  if np < 0 as i64 { np = 4 as i64; ku = 1 as i64; }
 }
 // Sizing and alias fallback run for router and model mode alike.
 if kv_per_slot > 0 as i64 && nc == 0 as i64 && fit_min_ctx != 4294967295 as i64 {
  nc = np * kv_per_slot;
 }
 if alias_empty != 0 as i64 && model_name_len > 0 as i64 { alias_use = 1 as i64; }
 if np <= 0 as i64 {valid=0 as i64;}
 var slot:i64=ctx_seq;
 if kv_per_slot > 0 as i64 && kv_per_slot < slot { slot = kv_per_slot; }
 if ctx_train < slot { slot = ctx_train; }
 put(out,0 as i64,valid);
 put(out,1 as i64,nb);
 put(out,2 as i64,nub);
 put(out,3 as i64,np);
 put(out,4 as i64,ku);
 put(out,5 as i64,nc);
 put(out,6 as i64,alias_use);
 put(out,7 as i64,slot);
 return 0 as i64;
}

fn main(){decide(1 as i64,0 as i64,1 as i64,1 as i64,1 as i64,0 as i64,0 as i64,0 as i64,0 as i64,1 as i64,1 as i64,1 as i64,1 as i64,0 as u64);}
