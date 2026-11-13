// APX027 Apex tokenizer/Unicode/BPE core.
// Source anchors: inputs/native/src/llama-vocab.cpp (BPE session,
// tokenizer_st_partition, tokenize/detokenize/token_to_piece),
// inputs/native/src/unicode.cpp (UTF-8, Qwen3.5 splitter, GPT-2 byte map),
// inputs/native/src/unicode-data.* (immutable tables through primitives).
//
// Algorithm ownership: UTF-8 decode/encode, Qwen3.5 pretokenization, GPT-2
// byte encoding, BPE heap/merge control, special-token partition, byte
// fallback, detokenization and clean-spaces passes are all implemented here.
// C++ services are typed storage/table projections only.

extern "apx027_cfg" fn cfg(v:i64,k:i64)->i64;
extern "apx027_tok_text_addr" fn tok_text_addr(v:i64,id:i64)->i64;
extern "apx027_tok_text_len" fn tok_text_len(v:i64,id:i64)->i64;
extern "apx027_tok_attr" fn tok_attr(v:i64,id:i64)->i64;
extern "apx027_text_to_token" fn text_to_token(v:i64,addr:i64,len:i64)->i64;
extern "apx027_bpe_rank" fn bpe_rank(v:i64,la:i64,ll:i64,ra:i64,rl:i64)->i64;
extern "apx027_special_raw_count" fn special_raw_count(v:i64)->i64;
extern "apx027_special_raw_id" fn special_raw_id(v:i64,i:i64)->i64;
extern "apx027_cpt_flags" fn cpt_flags(cpt:i64)->i64;
extern "apx027_lower_count" fn lower_count()->i64;
extern "apx027_lower_key" fn lower_key(i:i64)->i64;
extern "apx027_lower_value" fn lower_value(i:i64)->i64;
extern "apx027_mem_byte" fn mem_byte(addr:i64,i:i64)->i64;
extern "apx027_mem_i32" fn mem_i32(addr:i64,i:i64)->i64;
extern "apx027_out_count" fn out_count(s:i64)->i64;
extern "apx027_out_get" fn out_get(s:i64,i:i64)->i64;
extern "apx027_out_push" fn out_push(s:i64,t:i64);
extern "apx027_out_reset" fn out_reset(s:i64);
extern "apx027_cpt_reset" fn cpt_reset(s:i64);
extern "apx027_cpt_count" fn cpt_count(s:i64)->i64;
extern "apx027_cpt_get" fn cpt_get(s:i64,i:i64)->i64;
extern "apx027_cpt_push" fn cpt_push(s:i64,c:i64);
extern "apx027_word_reset" fn word_reset(s:i64);
extern "apx027_word_len" fn word_len(s:i64)->i64;
extern "apx027_word_addr" fn word_addr(s:i64)->i64;
extern "apx027_word_get" fn word_get(s:i64,i:i64)->i64;
extern "apx027_word_push" fn word_push(s:i64,b:i64);
extern "apx027_sym_reset" fn sym_reset(s:i64);
extern "apx027_sym_count" fn sym_count(s:i64)->i64;
extern "apx027_sym_get" fn sym_get(s:i64,i:i64,f:i64)->i64;
extern "apx027_sym_set" fn sym_set(s:i64,i:i64,f:i64,val:i64);
extern "apx027_sym_push" fn sym_push(s:i64,prev:i64,next:i64,off:i64,len:i64);
extern "apx027_heap_reset" fn heap_reset(s:i64);
extern "apx027_heap_count" fn heap_count(s:i64)->i64;
extern "apx027_heap_get" fn heap_get(s:i64,i:i64,f:i64)->i64;
extern "apx027_heap_set" fn heap_set(s:i64,i:i64,f:i64,val:i64);
extern "apx027_heap_push" fn heap_push(s:i64,l:i64,r:i64,rank:i64,off:i64,len:i64);
extern "apx027_heap_truncate" fn heap_truncate(s:i64,n:i64);
extern "apx027_frag_next_clear" fn frag_next_clear(s:i64);
extern "apx027_frag_count" fn frag_count(s:i64)->i64;
extern "apx027_frag_get" fn frag_get(s:i64,i:i64,f:i64)->i64;
extern "apx027_frag_push" fn frag_push(s:i64,typ:i64,tok:i64,addr:i64,len:i64);
extern "apx027_frag_next_push" fn frag_next_push(s:i64,typ:i64,tok:i64,addr:i64,len:i64);
extern "apx027_frag_swap" fn frag_swap(s:i64);
extern "apx027_special_reset" fn special_reset(s:i64);
extern "apx027_special_count" fn special_count(s:i64)->i64;
extern "apx027_special_get" fn special_get(s:i64,i:i64)->i64;
extern "apx027_special_push" fn special_push(s:i64,id:i64);
extern "apx027_special_set" fn special_set(s:i64,i:i64,id:i64);
extern "apx027_text_reset" fn text_reset(s:i64);
extern "apx027_text_count" fn text_count(s:i64)->i64;
extern "apx027_text_get" fn text_get(s:i64,i:i64)->i64;
extern "apx027_text_addr" fn text_addr(s:i64)->i64;
extern "apx027_text_push" fn text_push(s:i64,b:i64);
extern "apx027_text_set" fn text_set(s:i64,i:i64,b:i64);
extern "apx027_text_truncate" fn text_truncate(s:i64,n:i64);
extern "apx027_text_append_mem" fn text_append_mem(s:i64,addr:i64,len:i64);
extern "apx027_abort" fn fail(code:i64);

struct Utf8 { cpt:i64, next:i64, ok:i64 }

fn ensure(ok:bool) { if !ok { fail(1); } }
fn minimum(a:i64,b:i64)->i64 { return if a<b {a}else{b}; }
fn maximum(a:i64,b:i64)->i64 { return if a>b {a}else{b}; }
fn out_of_range()->i64 { return 4294967295; }

// Unicode category bits (unicode-data.h unicode_cpt_flags).
fn f_number(f:i64)->bool { return (f&0x0002)!=0; }
fn f_letter(f:i64)->bool { return (f&0x0004)!=0; }
fn f_accent(f:i64)->bool { return (f&0x0010)!=0; }
fn f_space(f:i64)->bool { return (f&0x0100)!=0; }

fn t_space_byte(b:i64)->bool { return b==32||b==9||b==10||b==11||b==12||b==13; }

// unicode_len_utf8 lookup equivalent.
fn t_utf8_len(b:i64)->i64 {
    let hi=b>>4;
    if hi==12||hi==13 {return 2;}
    if hi==14 {return 3;}
    if hi==15 {return 4;}
    return 1;
}

// unicode_cpt_from_utf8 with the same invalid-byte replacement contract as
// unicode_cpts_from_utf8: consume one byte and emit U+FFFD.
fn t_utf8_next(addr:i64,len:i64,pos:i64)->Utf8 {
    if pos>=len { return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 }; }
    let b0=mem_byte(addr,pos);
    if b0<0x80 { return Utf8 { cpt:b0, next:pos+1, ok:1 }; }
    if (b0&0x40)==0 { return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 }; }
    if (b0&0x20)==0 {
        if pos+1>=len||(mem_byte(addr,pos+1)&0xC0)!=0x80 { return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 }; }
        let c=((b0&0x1F)<<6)|(mem_byte(addr,pos+1)&0x3F);
        if c<0x80 { return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 }; }
        return Utf8 { cpt:c, next:pos+2, ok:1 };
    }
    if (b0&0x10)==0 {
        if pos+2>=len||(mem_byte(addr,pos+1)&0xC0)!=0x80||(mem_byte(addr,pos+2)&0xC0)!=0x80 { return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 }; }
        let c=((b0&0x0F)<<12)|((mem_byte(addr,pos+1)&0x3F)<<6)|(mem_byte(addr,pos+2)&0x3F);
        if c<0x800 { return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 }; }
        return Utf8 { cpt:c, next:pos+3, ok:1 };
    }
    if (b0&0x08)==0 {
        if pos+3>=len||(mem_byte(addr,pos+1)&0xC0)!=0x80||(mem_byte(addr,pos+2)&0xC0)!=0x80||(mem_byte(addr,pos+3)&0xC0)!=0x80 { return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 }; }
        let c=((b0&0x07)<<18)|((mem_byte(addr,pos+1)&0x3F)<<12)|((mem_byte(addr,pos+2)&0x3F)<<6)|(mem_byte(addr,pos+3)&0x3F);
        if c<0x10000||c>0x10FFFF { return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 }; }
        return Utf8 { cpt:c, next:pos+4, ok:1 };
    }
    return Utf8 { cpt:0xFFFD, next:pos+1, ok:0 };
}

// GPT-2 byte map from unicode.cpp: printable ranges keep their byte value,
// every other byte receives the next codepoint after 0xFF.
fn t_byte_direct(b:i64)->bool {
    return (b>=0x21&&b<=0x7E)||(b>=0xA1&&b<=0xAC)||(b>=0xAE&&b<=0xFF);
}
fn t_byte_to_cpt(b:i64)->i64 {
    if t_byte_direct(b) { return b; }
    var n:i64=0; var i:i64=0;
    while i<b { if !t_byte_direct(i) { n+=1; } i+=1; }
    return 256+n;
}
fn t_cpt_to_byte(cpt:i64)->i64 {
    if cpt<0 {return -1;}
    if cpt<=0xFF&&t_byte_direct(cpt) {return cpt;}
    if cpt<256 {return -1;}
    var n:i64=cpt-256; var b:i64=0;
    while b<256 { if !t_byte_direct(b) { if n==0 {return b;} n-=1; } b+=1; }
    return -1;
}

fn t_word_utf8(s:i64,cpt:i64) {
    if cpt<=0x7F { word_push(s,cpt); return; }
    if cpt<=0x7FF {
        word_push(s,0xC0|((cpt>>6)&0x1F)); word_push(s,0x80|(cpt&0x3F)); return;
    }
    if cpt<=0xFFFF {
        word_push(s,0xE0|((cpt>>12)&0x0F)); word_push(s,0x80|((cpt>>6)&0x3F)); word_push(s,0x80|(cpt&0x3F)); return;
    }
    if cpt<=0x10FFFF {
        word_push(s,0xF0|((cpt>>18)&0x07)); word_push(s,0x80|((cpt>>12)&0x3F));
        word_push(s,0x80|((cpt>>6)&0x3F)); word_push(s,0x80|(cpt&0x3F)); return;
    }
    ensure(false);
}
fn t_word_mapped_byte(s:i64,b:i64) { t_word_utf8(s,t_byte_to_cpt(b)); }
fn t_word_mapped_cpt(s:i64,cpt:i64) {
    if cpt<=0x7F { t_word_mapped_byte(s,cpt); return; }
    if cpt<=0x7FF { t_word_mapped_byte(s,0xC0|((cpt>>6)&0x1F)); t_word_mapped_byte(s,0x80|(cpt&0x3F)); return; }
    if cpt<=0xFFFF {
        t_word_mapped_byte(s,0xE0|((cpt>>12)&0x0F)); t_word_mapped_byte(s,0x80|((cpt>>6)&0x3F)); t_word_mapped_byte(s,0x80|(cpt&0x3F)); return;
    }
    if cpt<=0x10FFFF {
        t_word_mapped_byte(s,0xF0|((cpt>>18)&0x07)); t_word_mapped_byte(s,0x80|((cpt>>12)&0x3F));
        t_word_mapped_byte(s,0x80|((cpt>>6)&0x3F)); t_word_mapped_byte(s,0x80|(cpt&0x3F)); return;
    }
    ensure(false);
}

fn t_tolower(cpt:i64)->i64 {
    var lo:i64=0; var hi:i64=lower_count();
    while lo<hi {
        let mid=(lo+hi)/2;
        if lower_key(mid)<cpt { lo=mid+1; } else { hi=mid; }
    }
    if lo<lower_count()&&lower_key(lo)==cpt { return lower_value(lo); }
    return cpt;
}

fn t_cpt_at(s:i64,pos:i64)->i64 {
    if pos<0||pos>=cpt_count(s) {return out_of_range();}
    return cpt_get(s,pos);
}
fn t_flag_at(s:i64,pos:i64)->i64 {
    if pos<0||pos>=cpt_count(s) {return 0;}
    return cpt_flags(cpt_get(s,pos));
}

fn t_find(addr:i64,len:i64,naddr:i64,nlen:i64)->i64 {
    if nlen<=0 {return 0;}
    var i:i64=0;
    while i+nlen<=len {
        var j:i64=0; var ok:i64=1;
        while j<nlen {
            if mem_byte(addr,i+j)!=mem_byte(naddr,j) { ok=0; break; }
            j+=1;
        }
        if ok!=0 {return i;}
        i+=1;
    }
    return -1;
}

// Special tokens sorted by text length descending.  Ties use id ascending so
// this synthetic/qualified path is deterministic; upstream leaves equal-length
// ties unspecified.
fn t_prepare_specials(v:i64,s:i64)->i64 {
    special_reset(s);
    let n=special_raw_count(v);
    var i:i64=0;
    while i<n { special_push(s,special_raw_id(v,i)); i+=1; }
    i=1;
    while i<special_count(s) {
        let id=special_get(s,i);
        let len=tok_text_len(v,id);
        var j:i64=i;
        while j>0 {
            let p=special_get(s,j-1);
            let plen=tok_text_len(v,p);
            if plen<len||(plen==len&&p>id) { special_set(s,j,p); j-=1; } else { break; }
        }
        special_set(s,j,id);
        i+=1;
    }
    return special_count(s);
}

// tokenizer_st_partition equivalent.  Fragment type 0=token, 1=raw.
fn t_partition(v:i64,s:i64,parse_special:i64) {
    var si:i64=0;
    while si<special_count(s) {
        let id=special_get(s,si);
        let attr=tok_attr(v,id);
        var skip:i64=0;
        if parse_special==0 && (attr&9)!=0 { skip=1; }
        if skip==0 {
            let saddr=tok_text_addr(v,id);
            let slen=tok_text_len(v,id);
            if slen<=0 { si+=1; continue; }
            frag_next_clear(s);
            let fc=frag_count(s);
            var fi:i64=0;
            while fi<fc {
                let typ=frag_get(s,fi,0);
                if typ==0 {
                    frag_next_push(s,0,frag_get(s,fi,1),0,0);
                } else {
                    var fa=frag_get(s,fi,2);
                    var fl=frag_get(s,fi,3);
                    var alive:i64=1;
                    while alive!=0 {
                        let pos=t_find(fa,fl,saddr,slen);
                        if pos<0 { alive=0; break; }
                        var left_len=pos;
                        if (attr&128)!=0 { while left_len>0&&t_space_byte(mem_byte(fa,left_len-1)) { left_len-=1; } }
                        if left_len>0 { frag_next_push(s,1,-1,fa,left_len); }
                        frag_next_push(s,0,id,0,0);
                        var roff=pos+slen;
                        var rlen=fl-roff;
                        if (attr&256)!=0 { while rlen>0&&t_space_byte(mem_byte(fa,roff)) { roff+=1; rlen-=1; } }
                        fa=fa+roff; fl=rlen;
                        if fl<=0 { alive=0; }
                    }
                    if fl>0 { frag_next_push(s,1,-1,fa,fl); }
                }
                fi+=1;
            }
            frag_swap(s);
        }
        si+=1;
    }
}

// Qwen3.5 pretokenizer, direct translation of unicode_regex_split_custom_qwen35.
fn t_split_qwen35(v:i64,s:i64) {
    let n=cpt_count(s);
    var prev:i64=0;
    var pos:i64=0;
    while pos<n {
        let cpt=cpt_get(s,pos);
        let flags=cpt_flags(cpt);
        if cpt==39&&pos+1<n {
            let nx=t_tolower(t_cpt_at(s,pos+1));
            if nx==115||nx==116||nx==109||nx==100 {
                let end=pos+2;
                if end>prev { t_bpe_span(v,s,prev,end); }
                prev=end; pos=end; continue;
            }
            if pos+2<n {
                let nn=t_tolower(t_cpt_at(s,pos+2));
                if (nx==114&&nn==101)||(nx==118&&nn==101)||(nx==108&&nn==108) {
                    let end=pos+3;
                    if end>prev { t_bpe_span(v,s,prev,end); }
                    prev=end; pos=end; continue;
                }
            }
        }
        if !(cpt==13||cpt==10||f_number(flags)) {
            if f_letter(flags)||f_accent(flags)||f_accent(t_flag_at(s,pos+1))||f_letter(t_flag_at(s,pos+1)) {
                pos+=1;
                while f_letter(t_flag_at(s,pos))||f_accent(t_flag_at(s,pos)) { pos+=1; }
                let end=pos;
                if end>prev { t_bpe_span(v,s,prev,end); }
                prev=end; continue;
            }
        }
        if f_number(flags) {
            pos+=1;
            let end=pos;
            if end>prev { t_bpe_span(v,s,prev,end); }
            prev=end; continue;
        }
        var flags2=if cpt==32 {t_flag_at(s,pos+1)}else{flags};
        if !(f_space(flags2)||f_letter(flags2)||f_accent(flags2)||f_number(flags2))&&flags!=0 {
            if cpt==32 { pos+=1; }
            while !(f_space(flags2)||f_letter(flags2)||f_accent(flags2)||f_number(flags2))&&flags2!=0 {
                pos+=1; flags2=t_flag_at(s,pos);
            }
            var cpt2=t_cpt_at(s,pos);
            while cpt2==13||cpt2==10 { pos+=1; cpt2=t_cpt_at(s,pos); }
            let end=pos;
            if end>prev { t_bpe_span(v,s,prev,end); }
            prev=end; continue;
        }
        var num_ws:i64=0; var last_rn:i64=0;
        while f_space(t_flag_at(s,pos+num_ws)) {
            let cpt2=t_cpt_at(s,pos+num_ws);
            if cpt2==13||cpt2==10 { last_rn=pos+num_ws+1; }
            num_ws+=1;
        }
        if last_rn>0 {
            pos=last_rn;
            let end=pos;
            if end>prev { t_bpe_span(v,s,prev,end); }
            prev=end; continue;
        }
        if num_ws>1&&t_cpt_at(s,pos+num_ws)!=out_of_range() {
            pos+=num_ws-1;
            let end=pos;
            if end>prev { t_bpe_span(v,s,prev,end); }
            prev=end; continue;
        }
        if num_ws>0 {
            pos+=num_ws;
            let end=pos;
            if end>prev { t_bpe_span(v,s,prev,end); }
            prev=end; continue;
        }
        pos+=1;
        let end=pos;
        if end>prev { t_bpe_span(v,s,prev,end); }
        prev=end;
    }
    if prev<n { t_bpe_span(v,s,prev,n); }
}

fn t_heap_better(s:i64,a:i64,b:i64)->bool {
    let ra=heap_get(s,a,2); let rb=heap_get(s,b,2);
    if ra!=rb { return ra<rb; }
    let la=heap_get(s,a,0); let lb=heap_get(s,b,0);
    if la!=lb { return la<lb; }
    return heap_get(s,a,1)<heap_get(s,b,1);
}
fn t_heap_swap(s:i64,a:i64,b:i64) {
    var f:i64=0;
    while f<5 {
        let x=heap_get(s,a,f); let y=heap_get(s,b,f);
        heap_set(s,a,f,y); heap_set(s,b,f,x);
        f+=1;
    }
}
fn t_heap_insert(s:i64,l:i64,r:i64,rank:i64,off:i64,len:i64) {
    let n=heap_count(s);
    heap_push(s,l,r,rank,off,len);
    var i:i64=n;
    while i>0 {
        let p=(i-1)/2;
        if t_heap_better(s,i,p) { t_heap_swap(s,i,p); i=p; } else { break; }
    }
}
fn t_heap_pop(s:i64) {
    let n=heap_count(s);
    if n<=0 { return; }
    let last=n-1;
    if last>0 {
        var f:i64=0;
        while f<5 { heap_set(s,0,f,heap_get(s,last,f)); f+=1; }
    }
    heap_truncate(s,last);
    if last>0 {
        var i:i64=0; var alive:i64=1;
        while alive!=0 {
            let l=2*i+1;
            if l>=last { break; }
            var best=l;
            let r=l+1;
            if r<last&&t_heap_better(s,r,l) { best=r; }
            if t_heap_better(s,best,i) { t_heap_swap(s,i,best); i=best; } else { alive=0; }
        }
    }
}

fn t_add_bigram(v:i64,s:i64,l:i64,r:i64) {
    if l==-1||r==-1 { return; }
    let ll=sym_get(s,l,3); let rl=sym_get(s,r,3);
    if ll<=0||rl<=0 { return; }
    let lo=sym_get(s,l,2); let ro=sym_get(s,r,2);
    let wa=word_addr(s);
    let rank=bpe_rank(v,wa+lo,ll,wa+ro,rl);
    if rank<0 { return; }
    t_heap_insert(s,l,r,rank,lo,ll+rl);
}

fn t_merge_one(v:i64,s:i64) {
    let n=heap_count(s);
    if n<=0 { return; }
    let l=heap_get(s,0,0); let r=heap_get(s,0,1);
    let want=heap_get(s,0,4);
    t_heap_pop(s);
    if l<0||r<0 { return; }
    let ll=sym_get(s,l,3); let rl=sym_get(s,r,3);
    if ll<=0||rl<=0 { return; }
    let lo=sym_get(s,l,2); let ro=sym_get(s,r,2);
    if ll+rl!=want||lo+ll!=ro { return; }
    sym_set(s,l,3,ll+rl);
    sym_set(s,r,3,0);
    let rnext=sym_get(s,r,1);
    sym_set(s,l,1,rnext);
    if rnext>=0 { sym_set(s,rnext,0,l); }
    let lprev=sym_get(s,l,0);
    t_add_bigram(v,s,lprev,l);
    t_add_bigram(v,s,l,sym_get(s,l,1));
}

fn t_emit_symbol(v:i64,s:i64,i:i64) {
    let off=sym_get(s,i,2); let len=sym_get(s,i,3);
    if len<=0 { return; }
    let wa=word_addr(s);
    let id=text_to_token(v,wa+off,len);
    if id>=0 { out_push(s,id); return; }
    var j:i64=0;
    while j<len {
        let one=text_to_token(v,wa+off+j,1);
        if one>=0 { out_push(s,one); }
        j+=1;
    }
}

// One Qwen3.5 regex word: UTF-8 cpts -> GPT-2 byte-encoded text -> BPE.
fn t_bpe_span(v:i64,s:i64,start:i64,end:i64) {
    if end<=start { return; }
    word_reset(s);
    var i:i64=start;
    while i<end { t_word_mapped_cpt(s,cpt_get(s,i)); i+=1; }
    let wl=word_len(s);
    if wl<=0 { return; }
    sym_reset(s);
    if cfg(v,6)!=0&&text_to_token(v,word_addr(s),wl)>=0 {
        sym_push(s,-1,-1,0,wl);
    } else {
        var off:i64=0; var index:i64=0;
        while off<wl {
            let first=word_get(s,off);
            var clen=t_utf8_len(first);
            let rem=wl-off;
            if clen>rem { clen=rem; }
            let prev=index-1;
            var nxt:i64=-1;
            if off+clen<wl { nxt=index+1; }
            sym_push(s,prev,nxt,off,clen);
            off+=clen; index+=1;
        }
        heap_reset(s);
        var bi:i64=1;
        while bi<index { t_add_bigram(v,s,bi-1,bi); bi+=1; }
        while heap_count(s)>0 { t_merge_one(v,s); }
    }
    var si:i64=0;
    while si!=-1 {
        let ln=sym_get(s,si,3);
        if ln>0 { t_emit_symbol(v,s,si); }
        si=sym_get(s,si,1);
    }
}

fn t_tokenize_raw(v:i64,s:i64,addr:i64,len:i64) {
    var a=addr; var l=len;
    if cfg(v,7)!=0 {
        text_reset(s);
        var i:i64=0;
        while i<l {
            let b=mem_byte(a,i);
            if b==32 { text_push(s,0xE2); text_push(s,0x96); text_push(s,0x81); } else { text_push(s,b); }
            i+=1;
        }
        a=text_addr(s); l=text_count(s);
    }
    cpt_reset(s);
    var pos:i64=0;
    while pos<l {
        let u=t_utf8_next(a,l,pos);
        cpt_push(s,u.cpt);
        pos=u.next;
    }
    t_split_qwen35(v,s);
}

fn t_tokenize(v:i64,s:i64,addr:i64,len:i64,add_special:i64,parse_special:i64)->i64 {
    out_reset(s);
    frag_next_clear(s);
    frag_swap(s);
    frag_next_clear(s);
    special_reset(s);
    t_prepare_specials(v,s);
    if len>0 { frag_push(s,1,-1,addr,len); }
    t_partition(v,s,parse_special);
    if add_special!=0&&cfg(v,1)!=0 { out_push(s,cfg(v,2)); }
    let nf=frag_count(s);
    var i:i64=0;
    while i<nf {
        if frag_get(s,i,0)==0 { out_push(s,frag_get(s,i,1)); }
        else { t_tokenize_raw(v,s,frag_get(s,i,2),frag_get(s,i,3)); }
        i+=1;
    }
    if add_special!=0&&cfg(v,3)!=0 { out_push(s,cfg(v,4)); }
    return out_count(s);
}

fn t_hexval(b:i64)->i64 {
    if b>=48&&b<=57 {return b-48;}
    if b>=65&&b<=70 {return b-55;}
    if b>=97&&b<=102 {return b-87;}
    return -1;
}
fn t_hexchar(n:i64)->i64 { if n<10 {return 48+n;} return 87+n; }

fn t_word_literal(s:i64,txt:i64) { word_push(s,txt); }
fn t_word_unk(s:i64,cpt:i64,taddr:i64,tlen:i64) {
    t_word_literal(s,91); t_word_literal(s,85); t_word_literal(s,78); t_word_literal(s,75);
    t_word_literal(s,95); t_word_literal(s,66); t_word_literal(s,89); t_word_literal(s,84);
    t_word_literal(s,69); t_word_literal(s,95); t_word_literal(s,48); t_word_literal(s,120);
    if cpt<=0x7F {
        word_push(s,t_hexchar((cpt>>4)&15)); word_push(s,t_hexchar(cpt&15));
    } else if cpt<=0x7FF {
        word_push(s,t_hexchar(((0xC0|((cpt>>6)&0x1F))>>4)&15)); word_push(s,t_hexchar((0xC0|((cpt>>6)&0x1F))&15));
        word_push(s,t_hexchar(((0x80|(cpt&0x3F))>>4)&15)); word_push(s,t_hexchar((0x80|(cpt&0x3F))&15));
    } else if cpt<=0xFFFF {
        word_push(s,t_hexchar(((0xE0|((cpt>>12)&0x0F))>>4)&15)); word_push(s,t_hexchar((0xE0|((cpt>>12)&0x0F))&15));
        word_push(s,t_hexchar(((0x80|((cpt>>6)&0x3F))>>4)&15)); word_push(s,t_hexchar((0x80|((cpt>>6)&0x3F))&15));
        word_push(s,t_hexchar(((0x80|(cpt&0x3F))>>4)&15)); word_push(s,t_hexchar((0x80|(cpt&0x3F))&15));
    } else {
        word_push(s,t_hexchar(((0xF0|((cpt>>18)&0x07))>>4)&15)); word_push(s,t_hexchar((0xF0|((cpt>>18)&0x07))&15));
        word_push(s,t_hexchar(((0x80|((cpt>>12)&0x3F))>>4)&15)); word_push(s,t_hexchar((0x80|((cpt>>12)&0x3F))&15));
        word_push(s,t_hexchar(((0x80|((cpt>>6)&0x3F))>>4)&15)); word_push(s,t_hexchar((0x80|((cpt>>6)&0x3F))&15));
        word_push(s,t_hexchar(((0x80|(cpt&0x3F))>>4)&15)); word_push(s,t_hexchar((0x80|(cpt&0x3F))&15));
    }
    var i:i64=0;
    while i<tlen { word_push(s,mem_byte(taddr,i)); i+=1; }
    t_word_literal(s,93);
}
fn t_word_unescape(s:i64,addr:i64,len:i64) {
    var i:i64=0;
    while i<len {
        if i+2<len&&mem_byte(addr,i)==0xE2&&mem_byte(addr,i+1)==0x96&&mem_byte(addr,i+2)==0x81 {
            word_push(s,32); i+=3;
        } else { word_push(s,mem_byte(addr,i)); i+=1; }
    }
}
fn t_word_decode(s:i64,addr:i64,len:i64) {
    var pos:i64=0;
    while pos<len {
        let u=t_utf8_next(addr,len,pos);
        let b=t_cpt_to_byte(u.cpt);
        if b>=0 { word_push(s,b); } else { t_word_unk(s,u.cpt,addr,len); }
        pos=u.next;
    }
}
fn t_word_byte_token(s:i64,addr:i64,len:i64) {
    if len>=5 {
        let h1=t_hexval(mem_byte(addr,3));
        let h2=t_hexval(mem_byte(addr,4));
        if h1>=0&&h2>=0 { word_push(s,h1*16+h2); }
    }
}
fn t_append_piece(s:i64,lstrip:i64)->i64 {
    let wl=word_len(s); let wa=word_addr(s);
    var i:i64=0; var sk:i64=0;
    while i<wl {
        if sk<lstrip&&word_get(s,i)==32 { i+=1; sk+=1; } else { break; }
    }
    let n=wl-i;
    if n>0 { text_append_mem(s,wa+i,n); }
    return n;
}

fn t_piece(v:i64,s:i64,token:i64,lstrip:i64,special:i64)->i64 {
    let attr=tok_attr(v,token);
    if attr<0 { return 0; }
    if special==0&&(attr&9)!=0 { return 0; }
    let ta=tok_text_addr(v,token); let tl=tok_text_len(v,token);
    word_reset(s);
    if (attr&25)!=0 {
        var i:i64=0; while i<tl { word_push(s,mem_byte(ta,i)); i+=1; }
    } else if (attr&4)!=0 {
        if cfg(v,7)!=0 { t_word_unescape(s,ta,tl); } else { t_word_decode(s,ta,tl); }
    } else if (attr&32)!=0 {
        t_word_byte_token(s,ta,tl);
    } else {
        return 0;
    }
    return t_append_piece(s,lstrip);
}

fn t_clean_spaces(s:i64) {
    let n=text_count(s);
    if n<=0 { return; }
    var total:i64=1; var i:i64=1;
    while i<n {
        let x=text_get(s,i);
        if text_get(s,i-1)==32 { if x==63||x==33||x==46||x==44 { total-=1; } }
        text_set(s,total,x); total+=1; i+=1;
    }
    let total2=total; total=1; i=1;
    while i<total2 {
        let x=text_get(s,i);
        if x==39&&i+1<total2&&text_get(s,i-1)==32&&text_get(s,i+1)==32 {
            total-=1; i+=1; text_set(s,i,0);
        }
        text_set(s,total,x); total+=1; i+=1;
    }
    let total3=total; total=1; i=1;
    while i<total3 {
        let x=text_get(s,i);
        if text_get(s,i-1)==32 {
            if x==39&&i+1<total3 {
                let x1=text_get(s,i+1);
                if x1==116||x1==100 { } else if x1==115||x1==109 { total-=1; }
                else if i+2<total3 {
                    let x2=text_get(s,i+2);
                    if x1==108&&x2==108 { } else if (x1==114&&x2==101)||(x1==118&&x2==101) { total-=1; }
                }
            }
        }
        text_set(s,total,x); total+=1; i+=1;
    }
    text_truncate(s,total);
}

fn t_detokenize(v:i64,s:i64,tokens:i64,n:i64,remove_special:i64,unparse_special:i64)->i64 {
    text_reset(s);
    var start:i64=0; var count:i64=n;
    var remove_space:i64=0;
    if remove_special!=0&&cfg(v,1)!=0 {
        if count>0&&mem_i32(tokens,0)==cfg(v,2) { remove_space=0; start=1; count-=1; }
    }
    if remove_special!=0&&cfg(v,3)!=0 {
        if count>0&&mem_i32(tokens,start+count-1)==cfg(v,4) { count-=1; }
    }
    var i:i64=0;
    while i<count {
        let tok=mem_i32(tokens,start+i);
        let ignored=t_piece(v,s,tok,remove_space,unparse_special);
        remove_space=0; i+=1;
    }
    if cfg(v,5)!=0 { t_clean_spaces(s); }
    return text_count(s);
}

fn t_selftest()->i64 {
    ensure(t_utf8_len(0x41)==1);
    ensure(t_utf8_len(0xC3)==2);
    ensure(t_utf8_len(0xE4)==3);
    ensure(t_utf8_len(0xF0)==4);
    ensure(t_byte_direct(0x41));
    ensure(!t_byte_direct(0x00));
    var b:i64=0;
    while b<256 {
        let c=t_byte_to_cpt(b);
        ensure(t_cpt_to_byte(c)==b);
        b+=1;
    }
    ensure(t_byte_to_cpt(0)==256);
    ensure(t_byte_to_cpt(0x20)==0x120);
    return 1;
}

// op 0 config, 1 tokenize, 2 detokenize, 3 token_to_piece, 4 prepare, 5 selftest, 6 reset
fn entry(op:i64,v:i64,s:i64,a:i64,b:i64,c:i64,d:i64)->i64 {
    if op==0 { return cfg(v,a); }
    if op==1 { return t_tokenize(v,s,a,b,c,d); }
    if op==2 { return t_detokenize(v,s,a,b,c,d); }
    if op==3 { text_reset(s); return t_piece(v,s,a,b,c); }
    if op==4 { return t_prepare_specials(v,s); }
    if op==5 { return t_selftest(); }
    if op==6 { apx027_reset_placeholder(s); return 0; }
    if op==7 { return t_byte_to_cpt(a); }
    if op==8 { return t_cpt_to_byte(a); }
    if op==9 { return t_tolower(a); }
    fail(99);
    return -1;
}

fn apx027_reset_placeholder(s:i64) {
    out_reset(s); cpt_reset(s); word_reset(s); sym_reset(s); heap_reset(s); text_reset(s);
    frag_next_clear(s); frag_swap(s); frag_next_clear(s); special_reset(s);
}

fn main() { print(entry(5,0,0,0,0,0,0)); }
