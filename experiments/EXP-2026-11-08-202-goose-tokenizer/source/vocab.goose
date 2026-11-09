// Tokenizer decisions in Goose (APX: tokenizer).
//
// From unicode.cpp:16 and llama-vocab.cpp:118-230 (the BPE tokenizer). Tokenization runs on the
// Apex path twice per request -- once for the prompt, once per generated token when detokenizing.

extern "fg202t_fail" fn fail(code:i64);
extern "fg202t_st_i64" fn st_i64(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// utf8_len: unicode.cpp:16. The lead byte's top four bits index a table of 1,1,1,1,1,1,1,1,
// 1,1,1,1,2,2,3,4. Written as compares rather than a table so it stays arithmetic.
//   src is the unsigned byte value.
fn utf8_len(src:i64)->i64{
 ensure(src>=0&&src<=255 as i64,10);
 let hi:i64=src/16 as i64;
 if hi<=11 as i64{return 1 as i64;}
 if hi<=13 as i64{return 2 as i64;}
 if hi==14 as i64{return 3 as i64;}
 return 4 as i64;
}

// bigram_valid: llama-vocab.cpp:147-151. A queued merge is skipped when either side was already
// merged away, or when the recorded size no longer matches.
fn bigram_valid(left_n:i64,right_n:i64,bigram_size:i64)->i64{
 ensure(left_n>=0&&right_n>=0&&bigram_size>=0,20);
 if left_n==0 as i64{return 0 as i64;}
 if right_n==0 as i64{return 0 as i64;}
 if left_n+right_n!=bigram_size{return 0 as i64;}
 return 1 as i64;
}

// try_add_allowed: llama-vocab.cpp:203-216. A pair is queued only when both indices exist, the
// concatenation is a real token, and the id is inside the vocabulary.
fn try_add_allowed(left:i64,right:i64,token:i64,null_token:i64,n_tokens:i64)->i64{
 ensure(n_tokens>=0,30);
 if left==(0 as i64)-1 as i64{return 0 as i64;}
 if right==(0 as i64)-1 as i64{return 0 as i64;}
 if token==null_token{return 0 as i64;}
 if token>=n_tokens{return 0 as i64;}
 return 1 as i64;
}

// resegment_kind: llama-vocab.cpp:177-199.
//   0 = the text is a token, emit it
//   1 = it was built from a merge, recurse into the recorded pair
//   2 = neither, emit the bytes
fn resegment_kind(is_token:i64,has_merge:i64)->i64{
 if is_token!=0{return 0 as i64;}
 if has_merge!=0{return 1 as i64;}
 return 2 as i64;
}

// clamp_symbol_len: line 126, `min(utf8_len, text.size() - offs)` -- the last symbol may be a
// truncated sequence.
fn clamp_symbol_len(len:i64,remaining:i64)->i64{
 ensure(len>0&&remaining>0,40);
 if len<remaining{return len;}
 return remaining;
}

fn main(){
 utf8_len(0 as i64);
 bigram_valid(0 as i64,0 as i64,0 as i64);
 try_add_allowed(0 as i64,0 as i64,0 as i64,0 as i64,0 as i64);
 resegment_kind(0 as i64,0 as i64);
 clamp_symbol_len(1 as i64,1 as i64);
}
