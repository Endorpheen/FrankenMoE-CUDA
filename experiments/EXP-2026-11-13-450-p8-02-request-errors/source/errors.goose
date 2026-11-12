// Request error policy in Goose, from tools/server/server.cpp (ex_wrapper) and
// server-common.cpp (format_error_response).
//
// Owned decisions:
//   - exception class -> error_type: std::invalid_argument and common_json_error
//     become INVALID_REQUEST (400), any other std::exception and the catch-all
//     become SERVER (500);
//   - error_type -> HTTP code, exactly as format_error_response builds it;
//   - response status taken from the formatted payload's "code" with 500 as the
//     fallback;
// A per-endpoint body/required-field contract is deliberately not modelled
// here: post_props reads no body at all and post_slots reads it only after its
// feature checks, so "POST implies a parsed body" is false. That contract needs
// a per-handler audit and is the next slice.
//
// The error-type strings ("invalid_request_error", ...) and the JSON document
// itself stay native: Goose has no string type, so the module returns the
// numeric type and the wrapper maps it to the message.
extern "fg450_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

// Exception classes: 1 invalid_argument, 2 json_error, 3 other std::exception,
// 4 unknown/catch-all.
// Error types follow the native enum: 0 invalid_request, 1 authentication,
// 2 server, 3 not_found, 4 permission, 5 unavailable, 6 not_supported,
// 7 exceed_context_size.
fn classify_exception(kind:i64,out:u64)->i64{
 var t:i64=2 as i64;
 if kind==1 as i64 { t=0 as i64; }
 if kind==2 as i64 { t=0 as i64; }
 put(out,0 as i64,t);
 return 0 as i64;
}

fn error_code(t:i64,out:u64)->i64{
 var code:i64=500 as i64;
 if t==0 as i64 { code=400 as i64; }
 if t==1 as i64 { code=401 as i64; }
 if t==2 as i64 { code=500 as i64; }
 if t==3 as i64 { code=404 as i64; }
 if t==4 as i64 { code=403 as i64; }
 if t==5 as i64 { code=503 as i64; }
 if t==6 as i64 { code=501 as i64; }
 if t==7 as i64 { code=400 as i64; }
 put(out,0 as i64,code);
 return 0 as i64;
}

// The response status is json_value(error_data, "code", 500); the caller says
// whether the payload carried a code.
fn status_from_payload(has_code:i64,payload_code:i64,out:u64)->i64{
 var s:i64=500 as i64;
 if has_code!=0 as i64 { s=payload_code; }
 put(out,0 as i64,s);
 return 0 as i64;
}

fn main(){
 classify_exception(1 as i64,0 as u64);
 error_code(0 as i64,0 as u64);
 status_from_payload(0 as i64,0 as i64,0 as u64);
}
