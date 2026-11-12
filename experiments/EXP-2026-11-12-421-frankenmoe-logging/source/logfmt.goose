// FrankenMoE terminal log formatter in Goose.
//
// The C++ logger passes one already-formatted log line plus its level, mode,
// timestamp text and the previous event id. This module classifies the event
// from the text, writes the line into a caller buffer and reports the event id.
//
// Modes: 0 = classic (text verbatim), 1 = franken (banner + level prefix),
// 2 = JSON (one object per line, message escaped).
//
// Only byte load/store and an i64 store are extern primitives. Every
// comparison, scan, decision and output byte is Goose.
extern "fglog_ld_u8" fn ld_u8(p:u64,o:u64)->u8;
extern "fglog_st_u8" fn st_u8(p:u64,o:u64,v:u8);
extern "fglog_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put_byte(out:u64,cap:i64,pos:i64,b:u8)->i64{
 if pos<cap{ st_u8(out,pos as u64,b); return pos+(1 as i64); }
 return pos;
}

fn put_raw(out:u64,cap:i64,pos:i64,p:u64,n:i64)->i64{
 var i:i64=0 as i64;
 var q:i64=pos;
 while i<n{
  q=put_byte(out,cap,q,ld_u8(p,i as u64));
  i=i+(1 as i64);
 }
 return q;
}

fn put_lit(out:u64,cap:i64,pos:i64,s:u8[:])->i64{
 var q:i64=pos;
 for c in s{ q=put_byte(out,cap,q,c); }
 return q;
}

fn has_at(p:u64,n:i64,at:i64,s:u8[:])->bool{
 if at<0 as i64{ return false; }
 if at+(s.len as i64)>n{ return false; }
 var i:i64=0 as i64;
 for c in s{
  if ld_u8(p,((at+i) as u64))!=c{ return false; }
  i=i+(1 as i64);
 }
 return true;
}

fn has(p:u64,n:i64,s:u8[:])->bool{
 var at:i64=0 as i64;
 while at+(s.len as i64)<=n{
  if has_at(p,n,at,s){ return true; }
  at=at+(1 as i64);
 }
 return false;
}

fn classify(p:u64,n:i64,level:i64)->i64{
 if level==(1 as i64){ return 8 as i64; }
 if level==(2 as i64){ return 10 as i64; }
 if has(p,n,"load_model: initializing"){ return 5 as i64; }
 if has(p,n,"loading model")||has(p,n,"load_model")||has(p,n,"llama_model_loader")||has(p,n,"load_tensors"){ return 3 as i64; }
 if has(p,n,"llama_server: initializing")||has(p,n,"starting server")||has(p,n,"server is starting"){ return 1 as i64; }
 if has(p,n,"listening")||has(p,n,"server is ready"){ return 6 as i64; }
 if (has(p,n,"CUDA")||has(p,n,"cuda")||has(p,n,"NVIDIA"))&&(has(p,n,"device")||has(p,n,"GPU")||has(p,n,"backend")){ return 2 as i64; }
 if has(p,n,"model loaded")||has(p,n,"load time"){ return 4 as i64; }
 if has(p,n,"KV")||has(p,n,"cache")||has(p,n,"buffer size"){ return 5 as i64; }
 if has(p,n,"all systems")||has(p,n,"model loaded successfully"){ return 6 as i64; }
 if has(p,n,"prompt eval time")||has(p,n,"eval time")||has(p,n,"print_timing")||has(p,n,"tokens per second")||has(p,n,"tok/s"){ return 7 as i64; }
 if has(p,n,"shutting down")||has(p,n,"cleaning up")||has(p,n,"server stopped"){ return 9 as i64; }
 if has(p,n,"HTTP")||has(p,n,"request")||has(p,n,"POST")||has(p,n,"GET "){ return 11 as i64; }
 if has(p,n,"MoE")||has(p,n,"expert"){ return 12 as i64; }
 return 0 as i64;
}

fn banner(out:u64,cap:i64,pos:i64,ev:i64)->i64{
 if ev==(1 as i64){ return put_lit(out,cap,pos,"🔥 FRANKENMOE IS ALIVE!\n"); }
 if ev==(2 as i64){ return put_lit(out,cap,pos,"⚡ GPU DETECTED — RAW POWER ONLINE!\n"); }
 if ev==(3 as i64){ return put_lit(out,cap,pos,"⚡ UNLEASH THE MONSTER!\n"); }
 if ev==(4 as i64){ return put_lit(out,cap,pos,"🔥 THE BEAST IS LOADED!\n"); }
 if ev==(5 as i64){ return put_lit(out,cap,pos,"🧠 CACHE ARMED!\n"); }
 if ev==(6 as i64){ return put_lit(out,cap,pos,"🔥 ALL SYSTEMS BURNING!\n"); }
 if ev==(7 as i64){ return put_lit(out,cap,pos,"🚀 FULL THROTTLE!\n"); }
 if ev==(8 as i64){ return put_lit(out,cap,pos,"💥 THE MONSTER HIT A WALL!\n"); }
 if ev==(9 as i64){ return put_lit(out,cap,pos,"🛑 FRANKENMOE POWERING DOWN.\n"); }
 if ev==(10 as i64){ return pos; }
 return pos;
}

fn level_tag(level:i64)->u8[:]{
 if level==(1 as i64){ return "[ERROR] "; }
 if level==(2 as i64){ return "[WARN] "; }
 if level==(4 as i64){ return "[DEBUG] "; }
 if level==(0 as i64){ return ""; }
 return "[INFO] ";
}

fn level_name(level:i64)->u8[:]{
 if level==(1 as i64){ return "error"; }
 if level==(2 as i64){ return "warn"; }
 if level==(4 as i64){ return "debug"; }
 if level==(5 as i64){ return "cont"; }
 if level==(0 as i64){ return "none"; }
 return "info";
}

fn event_name(ev:i64)->u8[:]{
 if ev==(1 as i64){ return "server_start"; }
 if ev==(2 as i64){ return "gpu_detected"; }
 if ev==(3 as i64){ return "model_loading"; }
 if ev==(4 as i64){ return "model_loaded"; }
 if ev==(5 as i64){ return "memory_cache"; }
 if ev==(6 as i64){ return "ready"; }
 if ev==(7 as i64){ return "performance"; }
 if ev==(8 as i64){ return "error"; }
 if ev==(9 as i64){ return "shutdown"; }
 if ev==(10 as i64){ return "warning"; }
 if ev==(11 as i64){ return "http"; }
 if ev==(12 as i64){ return "moe"; }
 return "log";
}

fn json_escape(out:u64,cap:i64,pos:i64,p:u64,n:i64)->i64{
 var i:i64=0 as i64;
 var q:i64=pos;
 while i<n{
  let c:u8=ld_u8(p,i as u64);
  if c==(34 as u8){ q=put_lit(out,cap,q,"\\\""); }
  else {
   if c==(92 as u8){ q=put_lit(out,cap,q,"\\\\"); }
   else {
    if c==(10 as u8){ q=put_lit(out,cap,q,"\\n"); }
    else {
     if c==(13 as u8){ q=put_lit(out,cap,q,"\\r"); }
     else {
      if c==(9 as u8){ q=put_lit(out,cap,q,"\\t"); }
      else { q=put_byte(out,cap,q,c); }
     }
    }
   }
  }
  i=i+(1 as i64);
 }
 return q;
}

fn run(out:u64,cap:i64,event_out:u64,level:i64,mode:i64,last:i64,
       text:u64,text_len:i64,ts:u64,ts_len:i64)->i64{
 let ev:i64=classify(text,text_len,level);
 st_i64(event_out,0 as u64,ev);
 if mode==(0 as i64){ return put_raw(out,cap,0 as i64,text,text_len); }
 if mode==(1 as i64){
  var pos:i64=0 as i64;
  if ev!=last{ pos=banner(out,cap,pos,ev); }
  pos=put_lit(out,cap,pos,level_tag(level));
  pos=put_raw(out,cap,pos,text,text_len);
  if text_len>(0 as i64){
   if ld_u8(text,(text_len-(1 as i64)) as u64)!=(10 as u8){ pos=put_byte(out,cap,pos,10 as u8); }
  }
  return pos;
 }
 var pos:i64=0 as i64;
 pos=put_lit(out,cap,pos,"{\"time\":\"");
 pos=put_raw(out,cap,pos,ts,ts_len);
 pos=put_lit(out,cap,pos,"\",\"level\":\"");
 pos=put_lit(out,cap,pos,level_name(level));
 pos=put_lit(out,cap,pos,"\",\"event\":\"");
 pos=put_lit(out,cap,pos,event_name(ev));
 pos=put_lit(out,cap,pos,"\",\"msg\":\"");
 var mlen:i64=text_len;
 if mlen>(0 as i64){
  if ld_u8(text,(mlen-(1 as i64)) as u64)==(10 as u8){ mlen=mlen-(1 as i64); }
 }
 pos=json_escape(out,cap,pos,text,mlen);
 pos=put_lit(out,cap,pos,"\"}\n");
 return pos;
}

fn main(){run(0 as u64,0 as i64,0 as u64,3 as i64,0 as i64,0 as i64,0 as u64,0 as i64,0 as u64,0 as i64);}
