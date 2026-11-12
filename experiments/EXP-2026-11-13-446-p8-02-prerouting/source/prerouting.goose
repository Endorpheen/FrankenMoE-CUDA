// Server pre-routing policy in Goose, from tools/server/server-http.cpp
// (set_pre_routing_handler, middleware_server_state, middleware_validate_api_key,
// origin_is_localhost).
//
// Owned decisions:
//   - CORS Access-Control-Allow-Origin mode: echo the request Origin when
//     credentials are enabled together with "*", echo only for a localhost
//     origin when the configured value is "localhost", otherwise emit the
//     configured literal;
//   - OPTIONS short-circuit with the allow-credentials/methods/headers triple;
//   - readiness gate: 503 with the "Loading model" body, except frontend assets;
//   - API-key gate: public/frontend paths skip it, otherwise Authorization or
//     X-Api-Key with an optional "Bearer " prefix must match a configured key.
//
// Outputs six i64 words: action (0 continue, 1 handled), status (0/401/503),
// allow mode (0 none, 1 echo origin, 2 literal), literal length,
// options headers (0/1), reject reason (0 none, 1 not ready, 2 bad key).
extern "fg446_ld_u8" fn ld_u8(p:u64,o:u64)->u32;
extern "fg446_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

fn eq_lit(p:u64,from:i64,to:i64,w0:i64,w1:i64,w2:i64,w3:i64,w4:i64,w5:i64,w6:i64,w7:i64,w8:i64)->i64{
 let n:i64=to-from;
 if n!=9 as i64 {return 0 as i64;}
 if (ld_u8(p,from as u64) as i64)!=w0{return 0 as i64;}
 if (ld_u8(p,(from+1 as i64) as u64) as i64)!=w1{return 0 as i64;}
 if (ld_u8(p,(from+2 as i64) as u64) as i64)!=w2{return 0 as i64;}
 if (ld_u8(p,(from+3 as i64) as u64) as i64)!=w3{return 0 as i64;}
 if (ld_u8(p,(from+4 as i64) as u64) as i64)!=w4{return 0 as i64;}
 if (ld_u8(p,(from+5 as i64) as u64) as i64)!=w5{return 0 as i64;}
 if (ld_u8(p,(from+6 as i64) as u64) as i64)!=w6{return 0 as i64;}
 if (ld_u8(p,(from+7 as i64) as u64) as i64)!=w7{return 0 as i64;}
 if (ld_u8(p,(from+8 as i64) as u64) as i64)!=w8{return 0 as i64;}
 return 1 as i64;
}

// host == "localhost" (9 bytes)
fn is_localhost(p:u64,from:i64,to:i64)->i64{
 return eq_lit(p,from,to,108 as i64,111 as i64,99 as i64,97 as i64,108 as i64,104 as i64,111 as i64,115 as i64,116 as i64);
}
// host == "127.0.0.1" (9 bytes)
fn is_loopback4(p:u64,from:i64,to:i64)->i64{
 return eq_lit(p,from,to,49 as i64,50 as i64,55 as i64,46 as i64,48 as i64,46 as i64,48 as i64,46 as i64,49 as i64);
}
// host == "::1" (3 bytes)
fn is_loopback6(p:u64,from:i64,to:i64)->i64{
 let n:i64=to-from;
 if n!=3 as i64 {return 0 as i64;}
 if (ld_u8(p,from as u64) as i64)!=58 as i64{return 0 as i64;}
 if (ld_u8(p,(from+1 as i64) as u64) as i64)!=58 as i64{return 0 as i64;}
 if (ld_u8(p,(from+2 as i64) as u64) as i64)!=49 as i64{return 0 as i64;}
 return 1 as i64;
}

// Host extraction equivalent to common_http_parse_url for the accepted inputs:
// optional scheme, optional userinfo, bracketed IPv6, or host[:port].
fn origin_is_localhost(o:u64,n:i64)->i64{
 // optional scheme: authority starts after the first "://" or at byte 0
 var start:i64=0 as i64;
 var p:i64=0 as i64;
 var stop:i64=0 as i64;
 while stop==0 as i64 && p+3 <= n {
  let c0:i64=(ld_u8(o,p as u64) as i64);
  let c1:i64=(ld_u8(o,(p+1 as i64) as u64) as i64);
  let c2:i64=(ld_u8(o,(p+2 as i64) as u64) as i64);
  if c0==58 as i64 && c1==47 as i64 && c2==47 as i64 { start=p+3 as i64; stop=1 as i64; } else { p=p+1 as i64; }
 }
 // authority ends at '/', '?' or '#'
 var e:i64=start;
 var done:i64=0 as i64;
 while done==0 as i64 && e<n {
  let c:i64=(ld_u8(o,e as u64) as i64);
  if c==47 as i64 || c==63 as i64 || c==35 as i64 { done=1 as i64; } else { e=e+1 as i64; }
 }
 // last '@' before the authority end selects the userinfo boundary
 var s:i64=start;
 var q:i64=start;
 while q<e {
  if (ld_u8(o,q as u64) as i64)==64 as i64 { s=q+1 as i64; }
  q=q+1 as i64;
 }
 if s<e {
  if (ld_u8(o,s as u64) as i64)==91 as i64 {
   var r:i64=s+1 as i64;
   while r<e {
    if (ld_u8(o,r as u64) as i64)==93 as i64 { return is_loopback6(o,s+1 as i64,r); }
    r=r+1 as i64;
   }
   return 0 as i64;
  }
 }
 var colon:i64=-(1 as i64);
 var c:i64=s;
 while c<e {
  if (ld_u8(o,c as u64) as i64)==58 as i64 { colon=c; }
  c=c+1 as i64;
 }
 var he:i64=e;
 if colon>=(0 as i64) { he=colon; }
 var r1:i64=is_localhost(o,s,he);
 if r1!=0 as i64 {return 1 as i64;}
 var r2:i64=is_loopback4(o,s,he);
 if r2!=0 as i64 {return 1 as i64;}
 return 0 as i64;
}

// Authorization value match: optional "Bearer " prefix, then exact key bytes.
fn key_matches(a:u64,an:i64,b:u64,bn:i64,k:u64,kn:i64)->i64{
 var src:u64=a;
 var len:i64=an;
 if len==0 as i64 { src=b; len=bn; }
 if len>=7 as i64 {
  if (ld_u8(src,0 as u64) as i64)==66 as i64 {
   if (ld_u8(src,1 as u64) as i64)==101 as i64 {
    if (ld_u8(src,2 as u64) as i64)==97 as i64 {
     if (ld_u8(src,3 as u64) as i64)==114 as i64 {
      if (ld_u8(src,4 as u64) as i64)==101 as i64 {
       if (ld_u8(src,5 as u64) as i64)==114 as i64 {
        if (ld_u8(src,6 as u64) as i64)==32 as i64 { src=src+7 as u64; len=len-7 as i64; }
       }
      }
     }
    }
   }
  }
 }
 if len!=kn {return 0 as i64;}
 var i:i64=0 as i64;
 while i<len {
  if ld_u8(src,(i as u64))!=ld_u8(k,(i as u64)){return 0 as i64;}
  i=i+1 as i64;
 }
 return 1 as i64;
}

fn preroute(cors_cred:i64,cors_mode:i64,cors_lit_len:i64,origin:u64,origin_len:i64,
            is_options:i64,is_ready:i64,is_frontend:i64,is_public:i64,n_keys:i64,
            auth:u64,auth_len:i64,xapi:u64,xapi_len:i64,key:u64,key_len:i64,out:u64)->i64{
 var action:i64=0 as i64;
 var status:i64=0 as i64;
 var allow_mode:i64=0 as i64;
 var allow_len:i64=0 as i64;
 var options_hdr:i64=0 as i64;
 var reason:i64=0 as i64;

 // CORS allow-origin header, set on every response.
 if cors_mode==1 as i64 {
  if cors_cred!=0 as i64 { allow_mode=1 as i64; allow_len=origin_len; }
  else { allow_mode=2 as i64; allow_len=cors_lit_len; }
 }else{
  if cors_mode==2 as i64 {
   if origin_len>0 as i64 {
    let ok:i64=origin_is_localhost(origin,origin_len);
    if ok!=0 as i64 { allow_mode=1 as i64; allow_len=origin_len; }
   }
  }else{ allow_mode=2 as i64; allow_len=cors_lit_len; }
 }

 // Browsers send no Authorization on OPTIONS, so answer before the gates.
 if is_options!=0 as i64 {
  options_hdr=1 as i64;
  action=1 as i64;
  status=200 as i64;
 }else{
  if is_ready==0 as i64 && is_frontend==0 as i64 {
   action=1 as i64; status=503 as i64; reason=1 as i64;
  }else{
   if n_keys>0 as i64 && is_public==0 as i64 {
    let ok:i64=key_matches(auth,auth_len,xapi,xapi_len,key,key_len);
    if ok==0 as i64 { action=1 as i64; status=401 as i64; reason=2 as i64; }
   }
  }
 }

 put(out,0 as i64,action);
 put(out,1 as i64,status);
 put(out,2 as i64,allow_mode);
 put(out,3 as i64,allow_len);
 put(out,4 as i64,options_hdr);
 put(out,5 as i64,reason);
 return 0 as i64;
}

fn main(){preroute(0 as i64,0 as i64,0 as i64,0 as u64,0 as i64,0 as i64,1 as i64,0 as i64,0 as i64,0 as i64,0 as u64,0 as i64,0 as u64,0 as i64,0 as u64,0 as i64,0 as u64);}
