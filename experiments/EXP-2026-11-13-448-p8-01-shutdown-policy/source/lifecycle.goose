// Server signal and shutdown policy in Goose, from tools/server/server.cpp
// (signal_handler, shutdown_handler per mode, clean_up per mode) and
// server-context.cpp (terminate -> queue_tasks.terminate).
//
// The OS registration (sigaction/SetConsoleCtrlHandler) and the actual stop
// calls stay native; this module owns the decisions: what a signal does, the
// ordered shutdown steps per mode, and the process exit code.
//
// Step ids: 1 stop session GC, 2 set models_routes.stopping, 3 unload all
// models, 4 mcp shutdown, 5 llama_backend_free, 6 http stop, 7 ctx terminate.
extern "fg448_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

// Signal dispatch: first interrupt runs the handler, second forces exit(1).
// Writes action (0 nothing, 1 run shutdown, 2 force exit), exit code,
// and a "second interrupt" message flag.
fn on_signal(is_terminating:i64,has_handler:i64,out:u64)->i64{
 var action:i64=0 as i64;
 var code:i64=0 as i64;
 var msg:i64=0 as i64;
 if is_terminating!=0 as i64 {
  action=2 as i64;
  code=1 as i64;
  msg=1 as i64;
 }else{
  if has_handler!=0 as i64 { action=1 as i64; }
 }
 put(out,0 as i64,action);
 put(out,1 as i64,code);
 put(out,2 as i64,msg);
 return 0 as i64;
}

// Ordered shutdown plan. kind 0 = signal handler, 1 = clean_up before exit.
// mode 0 = router, 1 = single-model.
fn shutdown_plan(kind:i64,mode:i64,out:u64)->i64{
 var n:i64=0 as i64;
 if kind==0 as i64 {
  if mode==0 as i64 {
   put(out,1 as i64,2 as i64);
   put(out,2 as i64,4 as i64);
   put(out,3 as i64,6 as i64);
   n=3 as i64;
  }else{
   put(out,1 as i64,4 as i64);
   put(out,2 as i64,7 as i64);
   n=2 as i64;
  }
 }else{
  put(out,1 as i64,1 as i64);
  if mode==0 as i64 {
   put(out,2 as i64,2 as i64);
   put(out,3 as i64,3 as i64);
   put(out,4 as i64,4 as i64);
   put(out,5 as i64,5 as i64);
   n=5 as i64;
  }else{
   put(out,2 as i64,6 as i64);
   put(out,3 as i64,7 as i64);
   put(out,4 as i64,4 as i64);
   put(out,5 as i64,5 as i64);
   n=5 as i64;
  }
 }
 put(out,0 as i64,n);
 return 0 as i64;
}

// Process exit code by outcome: 0 normal, 1 http start error, 2 model load
// error, 3 connect error, 4 second interrupt.
fn exit_code(outcome:i64,out:u64)->i64{
 var code:i64=0 as i64;
 if outcome!=0 as i64 { code=1 as i64; }
 put(out,0 as i64,code);
 return 0 as i64;
}

fn main(){on_signal(0 as i64,1 as i64,0 as u64); shutdown_plan(0 as i64,0 as i64,0 as u64); exit_code(0 as i64,0 as u64);}
