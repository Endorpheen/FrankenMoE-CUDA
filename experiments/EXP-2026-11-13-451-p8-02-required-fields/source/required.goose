// Per-endpoint required-field contract in Goose, audited from the native POST
// handlers (server.cpp registrations -> server-context.cpp / server-chat.cpp /
// server-common.cpp / server-models.cpp).
//
// The module answers one question: given an endpoint category from EXP449 and a
// bitmask of the fields present in the request, which required fields are
// missing. The wrapper maps each bit to the native message text; keeping the
// messages there is a slice boundary, not a language limitation, since Goose has
// byte-slice strings.
//
// Field bits:
//   1 prompt, 2 messages, 4 input, 8 input_prefix, 16 input_suffix,
//   32 content, 64 query, 128 documents/texts, 256 model, 512 control id,
//   1024 whole body must be an array, 2048 multipart file.
//
// Categories with no explicit requirement are not invented here: /props and
// /slots read no request body, /tokenize and /detokenize treat content/tokens
// as optional, /tts, /systemone, /models/load and /models/unload read optional
// fields through json_value, and the OpenAI-style endpoints without a body
// requirement only validate values, not presence.
extern "fg451_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

fn req_mask(cat:i64)->i64{
 if cat==6 as i64 {return 256 as i64;}             // POST /models
 if cat==11 as i64 {return 1 as i64;}              // POST /completion
 if cat==12 as i64 {return 1 as i64;}              // POST /completions, /v1/completions
 if cat==13 as i64 {return 2 as i64;}              // POST /chat/completions
 if cat==14 as i64 {return 512 as i64;}            // POST /v1/chat/completions/control
 if cat==15 as i64 {return 4 as i64;}              // POST /responses, /v1/responses
 if cat==16 as i64 {return 2048 as i64;}           // POST /audio/transcriptions
 if cat==19 as i64 {return 2 as i64;}              // POST /v1/messages
 if cat==20 as i64 {return 24 as i64;}             // POST /infill: input_prefix + input_suffix
 if cat==21 as i64 {return 36 as i64;}             // POST /embedding, /embeddings: input OR content
 if cat==22 as i64 {return 36 as i64;}             // POST /v1/embeddings: input OR content
 if cat==23 as i64 {return 192 as i64;}            // POST rerank: query + documents/texts
 if cat==27 as i64 {return 2 as i64;}              // POST /apply-template (messages via chat parse)
 if cat==28 as i64 {return 2 as i64;}              // POST chat input_tokens
 if cat==29 as i64 {return 4 as i64;}              // POST responses input_tokens
 if cat==30 as i64 {return 2 as i64;}              // POST /v1/messages/count_tokens
 if cat==32 as i64 {return 1024 as i64;}           // POST /lora-adapters: body must be an array
 return 0 as i64;
}

// Missing-field mask. The embeddings endpoints accept either "input" or
// "content", so they are the only any-of cases.
export fn missing(cat:i64,mask:i64,out:u64)->i64{
 let need:i64=req_mask(cat);
 var miss:i64=need ^ (mask & need);
 if cat==21 as i64 || cat==22 as i64 {
  if (mask & 36 as i64)==0 as i64 { miss=36 as i64; } else { miss=0 as i64; }
 }
 put(out,0 as i64,miss);
 return 0 as i64;
}


