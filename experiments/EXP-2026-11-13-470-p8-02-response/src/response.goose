// Response assembly and request-logging decisions in Goose, from
// server-http.cpp (process_handler_response, log_server_request) and
// server_http_res helpers.
//
// Owned decisions:
//   - a response with a chunk generator is sent chunked, otherwise as one body;
//   - a streamed response overrides any user value of X-Accel-Buffering with
//     "no", because the header is set before the user headers are copied;
//   - the default status is 200 and an error response takes the code from its
//     payload;
//   - routine requests (health, metrics, props, models GET) are not logged.
//
// The socket, header serialization and body transfer stay native.
extern "fg470_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

// 0 one body with its length, 1 chunked.
export fn body_mode(streaming:i64)->i64{
 if streaming!=0 as i64 { return 1 as i64; }
 return 0 as i64;
}

// 1 when X-Accel-Buffering must be forced to "no".
export fn accel_override(streaming:i64)->i64{
 if streaming!=0 as i64 { return 1 as i64; }
 return 0 as i64;
}

// The status of a successful response is always 200; an error carries its own.
export fn status_for(is_error:i64,payload_code:i64)->i64{
 if is_error!=0 as i64 { return payload_code; }
 return 200 as i64;
}

// Route categories from the route table: 1 health, 2 metrics, 3 props_get,
// 4 props_post, 5 models_get. These are excluded from request logging.
export fn log_skip(cat:i64)->i64{
 if cat==1 as i64 { return 1 as i64; }
 if cat==2 as i64 { return 1 as i64; }
 if cat==3 as i64 { return 1 as i64; }
 if cat==4 as i64 { return 1 as i64; }
 if cat==5 as i64 { return 1 as i64; }
 return 0 as i64;
}

// Content type handling: a stream keeps its declared content type, a plain body
// keeps the default unless the handler replaced it.
export fn content_type(streaming:i64,handler_set:i64,out:u64)->i64{
 var kind:i64=0 as i64;  // 0 default json, 1 handler value, 2 stream value
 if handler_set!=0 as i64 { kind=1 as i64; }
 if streaming!=0 as i64 { kind=2 as i64; }
 put(out,0 as i64,kind);
 return kind;
}
