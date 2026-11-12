// HTTP route table in Goose, derived from tools/server/server.cpp.
//
// Paths are Goose byte-slice strings compared with literals; the only
// helpers used are the standard library's starts_with and find. There is
// no hand-written hash and no byte loop.
//
// The parameterised POST /slots/:id_slot rule matches exactly one path
// segment, like the native httplib pattern.
import std;

export fn route_id(m:i64,p:const u8[:])->i64{
 if m==1 as i64 && p=="/health" {return 1 as i64;} // GET /health -> health
 if m==1 as i64 && p=="/v1/health" {return 1 as i64;} // GET /v1/health -> health
 if m==1 as i64 && p=="/metrics" {return 2 as i64;} // GET /metrics -> metrics
 if m==1 as i64 && p=="/props" {return 3 as i64;} // GET /props -> props_get
 if m==2 as i64 && p=="/props" {return 4 as i64;} // POST /props -> props_post
 if m==1 as i64 && p=="/models" {return 5 as i64;} // GET /models -> models_get
 if m==1 as i64 && p=="/v1/models" {return 5 as i64;} // GET /v1/models -> models_get
 if m==2 as i64 && p=="/models" {return 6 as i64;} // POST /models -> models_post
 if m==2 as i64 && p=="/models/load" {return 7 as i64;} // POST /models/load -> models_load
 if m==2 as i64 && p=="/models/unload" {return 8 as i64;} // POST /models/unload -> models_unload
 if m==1 as i64 && p=="/models/sse" {return 9 as i64;} // GET /models/sse -> models_sse
 if m==3 as i64 && p=="/models" {return 10 as i64;} // DELETE /models -> models_delete
 if m==2 as i64 && p=="/completion" {return 11 as i64;} // POST /completion -> completions
 if m==2 as i64 && p=="/completions" {return 11 as i64;} // POST /completions -> completions
 if m==2 as i64 && p=="/v1/completions" {return 12 as i64;} // POST /v1/completions -> completions_oai
 if m==2 as i64 && p=="/chat/completions" {return 13 as i64;} // POST /chat/completions -> chat
 if m==2 as i64 && p=="/v1/chat/completions" {return 13 as i64;} // POST /v1/chat/completions -> chat
 if m==2 as i64 && p=="/v1/chat/completions/control" {return 14 as i64;} // POST /v1/chat/completions/control -> chat_control
 if m==2 as i64 && p=="/responses" {return 15 as i64;} // POST /responses -> responses
 if m==2 as i64 && p=="/v1/responses" {return 15 as i64;} // POST /v1/responses -> responses
 if m==2 as i64 && p=="/audio/transcriptions" {return 16 as i64;} // POST /audio/transcriptions -> transcriptions
 if m==2 as i64 && p=="/v1/audio/transcriptions" {return 16 as i64;} // POST /v1/audio/transcriptions -> transcriptions
 if m==2 as i64 && p=="/tts" {return 17 as i64;} // POST /tts -> tts
 if m==4 as i64 && p=="/v1/realtime" {return 18 as i64;} // WS /v1/realtime -> realtime
 if m==2 as i64 && p=="/v1/messages" {return 19 as i64;} // POST /v1/messages -> anthropic
 if m==2 as i64 && p=="/infill" {return 20 as i64;} // POST /infill -> infill
 if m==2 as i64 && p=="/embedding" {return 21 as i64;} // POST /embedding -> embeddings
 if m==2 as i64 && p=="/embeddings" {return 21 as i64;} // POST /embeddings -> embeddings
 if m==2 as i64 && p=="/v1/embeddings" {return 22 as i64;} // POST /v1/embeddings -> embeddings_oai
 if m==2 as i64 && p=="/rerank" {return 23 as i64;} // POST /rerank -> rerank
 if m==2 as i64 && p=="/reranking" {return 23 as i64;} // POST /reranking -> rerank
 if m==2 as i64 && p=="/v1/rerank" {return 23 as i64;} // POST /v1/rerank -> rerank
 if m==2 as i64 && p=="/v1/reranking" {return 23 as i64;} // POST /v1/reranking -> rerank
 if m==2 as i64 && p=="/v1/systemone" {return 24 as i64;} // POST /v1/systemone -> systemone
 if m==2 as i64 && p=="/tokenize" {return 25 as i64;} // POST /tokenize -> tokenize
 if m==2 as i64 && p=="/detokenize" {return 26 as i64;} // POST /detokenize -> detokenize
 if m==2 as i64 && p=="/apply-template" {return 27 as i64;} // POST /apply-template -> apply_template
 if m==2 as i64 && p=="/chat/completions/input_tokens" {return 28 as i64;} // POST /chat/completions/input_tokens -> chat_tokens
 if m==2 as i64 && p=="/v1/chat/completions/input_tokens" {return 28 as i64;} // POST /v1/chat/completions/input_tokens -> chat_tokens
 if m==2 as i64 && p=="/responses/input_tokens" {return 29 as i64;} // POST /responses/input_tokens -> responses_tokens
 if m==2 as i64 && p=="/v1/responses/input_tokens" {return 29 as i64;} // POST /v1/responses/input_tokens -> responses_tokens
 if m==2 as i64 && p=="/v1/messages/count_tokens" {return 30 as i64;} // POST /v1/messages/count_tokens -> anthropic_count
 if m==1 as i64 && p=="/lora-adapters" {return 31 as i64;} // GET /lora-adapters -> lora_get
 if m==2 as i64 && p=="/lora-adapters" {return 32 as i64;} // POST /lora-adapters -> lora_post
 if m==1 as i64 && p=="/slots" {return 33 as i64;} // GET /slots -> slots_get
 if m==2 as i64 && p=="/slots/:id_slot" {return 34 as i64;} // POST /slots/:id_slot -> slots_post
 if m==1 as i64 && p=="/v1/stream" {return 35 as i64;} // GET /v1/stream -> stream_get
 if m==2 as i64 && p=="/v1/streams/lookup" {return 36 as i64;} // POST /v1/streams/lookup -> stream_lookup
 if m==3 as i64 && p=="/v1/stream" {return 37 as i64;} // DELETE /v1/stream -> stream_delete
 if m==1 as i64 && p=="/cors-proxy" {return 38 as i64;} // GET /cors-proxy -> cors_proxy
 if m==2 as i64 && p=="/cors-proxy" {return 38 as i64;} // POST /cors-proxy -> cors_proxy
 if m==1 as i64 && p=="/tools" {return 39 as i64;} // GET /tools -> tools
 if m==2 as i64 && p=="/tools" {return 39 as i64;} // POST /tools -> tools
 // POST /slots/<one segment> -> slots_post
 if m==2 as i64 {
  if starts_with(p, "/slots/") {
   if p.len > 7 as i64 {
    if find(p[7..], "/") < 0 as i64 { return 34 as i64; }
   }
  }
 }
 return 0 as i64;
}

// Public endpoints skip the API-key middleware: /health and /v1/health.
// Frontend assets are classified outside this table.
export fn requires_key(cat:i64)->i64{
 if cat==0 as i64 {return 0 as i64;}
 if cat==1 as i64 {return 0 as i64;}
 return 1 as i64;
}

