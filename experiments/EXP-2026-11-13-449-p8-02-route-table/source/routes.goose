// HTTP route table in Goose, derived from tools/server/server.cpp.
//
// Goose has no string type, so each route is keyed by a 64-bit FNV-1a
// value over the method byte followed by the path bytes. The keying and
// the table live in Goose; the gate proves every native registration maps
// to its endpoint category and that no key collides.
//
// Method bytes: GET 71, POST 80, DELETE 68, WS 87.
extern "fg449_ld_u8" fn ld_u8(p:u64,o:u64)->u32;
extern "fg449_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

fn route_key(m:i64,p:u64,n:i64)->u64{
 var h:u64=14695981039346656037 as u64;
 var c:u64=71 as u64;
 if m==2 as i64 { c=80 as u64; }
 if m==3 as i64 { c=68 as u64; }
 if m==4 as i64 { c=87 as u64; }
 h=(h^c)*(1099511628211 as u64);
 var i:i64=0 as i64;
 while i<n {
  h=(h^(ld_u8(p,(i as u64)) as u64))*(1099511628211 as u64);
  i=i+1 as i64;
 }
 return h;
}

// Endpoint category by exact method/path key; 0 means no route.
fn route_id(m:i64,p:u64,n:i64)->i64{
 if n<=0 as i64 {return 0 as i64;}
 if n>64 as i64 {return 0 as i64;}
 let k:u64=route_key(m,p,n);
 if k==10501223494815025837 as u64 {return 1 as i64;} // GET /health -> health
 if k==10699280574590654561 as u64 {return 1 as i64;} // GET /v1/health -> health
 if k==12220293552347144652 as u64 {return 2 as i64;} // GET /metrics -> metrics
 if k==7597832721220393231 as u64 {return 3 as i64;} // GET /props -> props_get
 if k==4943521998184551670 as u64 {return 4 as i64;} // POST /props -> props_post
 if k==8006582481077697889 as u64 {return 5 as i64;} // GET /models -> models_get
 if k==7293894800919693477 as u64 {return 5 as i64;} // GET /v1/models -> models_get
 if k==2898157827274605114 as u64 {return 6 as i64;} // POST /models -> models_post
 if k==9791840147108711367 as u64 {return 7 as i64;} // POST /models/load -> models_load
 if k==13524756130009218204 as u64 {return 8 as i64;} // POST /models/unload -> models_unload
 if k==5243293070701117319 as u64 {return 9 as i64;} // GET /models/sse -> models_sse
 if k==10328991339275863766 as u64 {return 10 as i64;} // DELETE /models -> models_delete
 if k==10881261994218437582 as u64 {return 11 as i64;} // POST /completion -> completions
 if k==17412425316074291751 as u64 {return 11 as i64;} // POST /completions -> completions
 if k==3511958812834063073 as u64 {return 12 as i64;} // POST /v1/completions -> completions_oai
 if k==9340330806016395042 as u64 {return 13 as i64;} // POST /chat/completions -> chat
 if k==17798418640742959248 as u64 {return 13 as i64;} // POST /v1/chat/completions -> chat
 if k==9138293669124494902 as u64 {return 14 as i64;} // POST /v1/chat/completions/control -> chat_control
 if k==7315862352241971844 as u64 {return 15 as i64;} // POST /responses -> responses
 if k==16591844329692161090 as u64 {return 15 as i64;} // POST /v1/responses -> responses
 if k==14707698639797015534 as u64 {return 16 as i64;} // POST /audio/transcriptions -> transcriptions
 if k==16648217084125022368 as u64 {return 16 as i64;} // POST /v1/audio/transcriptions -> transcriptions
 if k==4440074995297731089 as u64 {return 17 as i64;} // POST /tts -> tts
 if k==11234074787290139946 as u64 {return 18 as i64;} // WS /v1/realtime -> realtime
 if k==5197450812956293534 as u64 {return 19 as i64;} // POST /v1/messages -> anthropic
 if k==7738728136773835750 as u64 {return 20 as i64;} // POST /infill -> infill
 if k==8004329022165213261 as u64 {return 21 as i64;} // POST /embedding -> embeddings
 if k==4807042906159319386 as u64 {return 21 as i64;} // POST /embeddings -> embeddings
 if k==14157013336565285732 as u64 {return 22 as i64;} // POST /v1/embeddings -> embeddings_oai
 if k==1609275978024118643 as u64 {return 23 as i64;} // POST /rerank -> rerank
 if k==17060797145577017029 as u64 {return 23 as i64;} // POST /reranking -> rerank
 if k==12295478724642615069 as u64 {return 23 as i64;} // POST /v1/rerank -> rerank
 if k==8980243062412232739 as u64 {return 23 as i64;} // POST /v1/reranking -> rerank
 if k==18187006483575642089 as u64 {return 24 as i64;} // POST /v1/systemone -> systemone
 if k==2170821321189234653 as u64 {return 25 as i64;} // POST /tokenize -> tokenize
 if k==17214012478992054986 as u64 {return 26 as i64;} // POST /detokenize -> detokenize
 if k==1036620416837226409 as u64 {return 27 as i64;} // POST /apply-template -> apply_template
 if k==13051087107915880648 as u64 {return 28 as i64;} // POST /chat/completions/input_tokens -> chat_tokens
 if k==4179593487456304118 as u64 {return 28 as i64;} // POST /v1/chat/completions/input_tokens -> chat_tokens
 if k==1341687336659302058 as u64 {return 29 as i64;} // POST /responses/input_tokens -> responses_tokens
 if k==8492924495378912296 as u64 {return 29 as i64;} // POST /v1/responses/input_tokens -> responses_tokens
 if k==2933315764314756157 as u64 {return 30 as i64;} // POST /v1/messages/count_tokens -> anthropic_count
 if k==13607268479640150674 as u64 {return 31 as i64;} // GET /lora-adapters -> lora_get
 if k==10907349879491615823 as u64 {return 32 as i64;} // POST /lora-adapters -> lora_post
 if k==15073566050695996812 as u64 {return 33 as i64;} // GET /slots -> slots_get
 if k==9491603346054848278 as u64 {return 34 as i64;} // POST /slots/:id_slot -> slots_post
 if k==3162538059094761863 as u64 {return 35 as i64;} // GET /v1/stream -> stream_get
 if k==10062528588462076706 as u64 {return 36 as i64;} // POST /v1/streams/lookup -> stream_lookup
 if k==12380539235207517070 as u64 {return 37 as i64;} // DELETE /v1/stream -> stream_delete
 if k==11204171914628148211 as u64 {return 38 as i64;} // GET /cors-proxy -> cors_proxy
 if k==3259325023006427884 as u64 {return 38 as i64;} // POST /cors-proxy -> cors_proxy
 if k==3780319044633820866 as u64 {return 39 as i64;} // GET /tools -> tools
 if k==16115986212896536491 as u64 {return 39 as i64;} // POST /tools -> tools
 // parameterised route: POST /slots/:id_slot
 if m==2 as i64 && n>7 as i64 {
  if (ld_u8(p,0 as u64) as i64)==47 as i64 {
   if (ld_u8(p,1 as u64) as i64)==115 as i64 {
    if (ld_u8(p,2 as u64) as i64)==108 as i64 {
     if (ld_u8(p,3 as u64) as i64)==111 as i64 {
      if (ld_u8(p,4 as u64) as i64)==116 as i64 {
       if (ld_u8(p,5 as u64) as i64)==115 as i64 {
        if (ld_u8(p,6 as u64) as i64)==47 as i64 { return 34 as i64; }
       }
      }
     }
    }
   }
  }
 }
 return 0 as i64;
}

// Public endpoints skip the API-key middleware: /health and /v1/health.
// Frontend assets are classified outside this table.
fn requires_key(cat:i64)->i64{
 if cat==0 as i64 {return 0 as i64;}
 if cat==1 as i64 {return 0 as i64;}
 return 1 as i64;
}

fn main(){
 put(0 as u64,0 as i64,route_id(1 as i64,0 as u64,0 as i64));
 put(0 as u64,1 as i64,(route_key(1 as i64,0 as u64,0 as i64) as i64));
 put(0 as u64,2 as i64,requires_key(0 as i64));
}
