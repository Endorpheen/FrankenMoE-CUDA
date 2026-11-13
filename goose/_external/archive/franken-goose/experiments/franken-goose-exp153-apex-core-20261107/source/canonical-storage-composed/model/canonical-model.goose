// worker-4/canonical-model: APX030 MODEL/HPARAMS/layer-binding storage.
// Goose owns the model/hparams records, layer registry and tensor-binding map.
// Worker8 canonical storage supplies record/vec/map primitives; VOCAB and loader
// schemas are typed cross-module imports. No C++ container or original
// llm_graph_context/model algorithm is used here.
//
// Schema keys are published in canonical-model.h. All handles are int64_t.

extern "fgc_record_new" fn rec_new(kind:i64)->i64;
extern "fgc_record_get" fn rec_get(h:i64,key:i64)->i64;
extern "fgc_record_set" fn rec_set(h:i64,key:i64,value:i64)->i64;
extern "fgc_record_kind" fn rec_kind(h:i64)->i64;
extern "fgc_i64_record_free" fn rec_free(h:i64)->i64;
extern "fgc_vec_new" fn vec_new(item:i64)->i64;
extern "fgc_vec_len" fn vec_len(h:i64)->i64;
extern "fgc_vec_resize" fn vec_resize(h:i64,n:i64)->i64;
extern "fgc_i64_vec_free" fn vec_free(h:i64)->i64;
extern "fgc_vec_get_i64" fn vec_get(h:i64,i:i64)->i64;
extern "fgc_vec_set_i64" fn vec_set(h:i64,i:i64,v:i64)->i64;
extern "fgc_map_new" fn map_new()->i64;
extern "fgc_map_put" fn map_put(h:i64,k:i64,v:i64)->i64;
extern "fgc_map_get_i64" fn map_get(h:i64,k:i64)->i64;
extern "fgc_map_has" fn map_has(h:i64,k:i64)->i64;
extern "fgc_map_erase" fn map_erase(h:i64,k:i64)->i64;
extern "fgc_i64_map_free" fn map_free(h:i64)->i64;
extern "fgc_bits_f32" fn fbits(x:f32)->i64;
extern "fgc_from_bits_i64" fn ffrom(b:i64)->f32;
extern "fgc_vocab_n_tokens" fn vocab_n(v:i64)->i64;
extern "fgc_loader_get_tensor" fn loader_tensor(l:i64,layer:i64,kind:i64)->i64;

fn model_new()->i64 {
    let m=rec_new(1);
    if m==0 {return 0;}
    let hp=rec_new(2);
    let lv=vec_new(8);
    let lm=map_new();
    let bm=map_new();
    if hp==0||lv==0||lm==0||bm==0 {
        if hp!=0 {rec_free(hp);} if lv!=0 {vec_free(lv);}
        if lm!=0 {map_free(lm);} if bm!=0 {map_free(bm);}
        rec_free(m);
        return 0;
    }
    rec_set(m,10,hp);
    rec_set(m,11,lv);
    rec_set(m,12,lm);
    rec_set(m,13,bm);
    rec_set(m,14,0);
    rec_set(m,15,0);
    return m;
}

fn model_free(m:i64)->i64 {
    if m==0 {return 0;}
    if rec_kind(m)!=1 {return -1;}
    let hp=rec_get(m,10);
    let lv=rec_get(m,11);
    let lm=rec_get(m,12);
    let bm=rec_get(m,13);
    if lv!=0 {
        let n=vec_len(lv);
        var i:i64=0;
        while i<n {
            let lh=vec_get(lv,i);
            if lh!=0 {rec_free(lh);}
            i+=1;
        }
    }
    if lm!=0 {map_free(lm);}
    if bm!=0 {map_free(bm);}
    if lv!=0 {vec_free(lv);}
    if hp!=0 {rec_free(hp);}
    rec_free(m);
    return 0;
}

fn model_kind(m:i64)->i64 {return rec_kind(m);}

fn model_set_int(m:i64,key:i64,value:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    if key==10||key==11||key==12||key==13 {return -1;}
    return rec_set(m,key,value);
}
fn model_get_int(m:i64,key:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    return rec_get(m,key);
}
fn model_set_float(m:i64,key:i64,value:f32)->i64 {return model_set_int(m,key,fbits(value));}
fn model_get_float(m:i64,key:i64)->f32 {return ffrom(model_get_int(m,key));}

fn model_set_handle(m:i64,key:i64,value:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    if key!=9 {return -1;}                    // only the borrowed VOCAB handle
    return rec_set(m,key,value);
}

fn hparams_get(m:i64,key:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    let hp=rec_get(m,10);
    if hp==0||rec_kind(hp)!=2 {return -1;}
    return rec_get(hp,key);
}
fn hparams_set(m:i64,key:i64,value:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    let hp=rec_get(m,10);
    if hp==0||rec_kind(hp)!=2 {return -1;}
    return rec_set(hp,key,value);
}

fn model_n_layers(m:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    let lv=rec_get(m,11);
    return if lv==0 {-1} else {vec_len(lv)};
}

fn layer_new(m:i64,layer:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return 0;}
    let lm=rec_get(m,12);
    let lv=rec_get(m,11);
    if lm==0||lv==0 {return 0;}
    let ex=map_get(lm,layer);
    if ex!=0 {return ex;}
    let lh=rec_new(3);
    if lh==0 {return 0;}
    rec_set(lh,0,0); rec_set(lh,1,0); rec_set(lh,2,0); rec_set(lh,3,0);
    rec_set(lh,4,0); rec_set(lh,5,0);
    rec_set(lh,6,0); rec_set(lh,7,0); rec_set(lh,8,0); rec_set(lh,9,0);
    let n=vec_len(lv);
    if vec_resize(lv,n+1)!=0 {rec_free(lh);return 0;}
    vec_set(lv,n,lh);
    if map_put(lm,layer,lh)!=0 {rec_free(lh);return 0;}
    rec_set(m,15,n+1);
    return lh;
}
fn layer_get(m:i64,layer:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return 0;}
    let lm=rec_get(m,12);
    return if lm==0 {0} else {map_get(lm,layer)};
}
fn layer_set_int(m:i64,layer:i64,key:i64,value:i64)->i64 {
    let lh=layer_get(m,layer);
    if lh==0||rec_kind(lh)!=3 {return -1;}
    return rec_set(lh,key,value);
}
fn layer_get_int(m:i64,layer:i64,key:i64)->i64 {
    let lh=layer_get(m,layer);
    if lh==0||rec_kind(lh)!=3 {return -1;}
    return rec_get(lh,key);
}

fn bind_set(m:i64,layer:i64,kind:i64,tensor:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    if tensor==0 {return -1;}
    let bm=rec_get(m,13);
    if bm==0 {return -1;}
    let key=layer*64+kind;
    let had=map_has(bm,key);
    let st=map_put(bm,key,tensor);
    if st!=0 {return st;}
    if had==0 {rec_set(m,14,rec_get(m,14)+1);}
    return 0;
}
fn bind_get(m:i64,layer:i64,kind:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return 0;}
    let bm=rec_get(m,13);
    return if bm==0 {0} else {map_get(bm,layer*64+kind)};
}
fn bind_erase(m:i64,layer:i64,kind:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    let bm=rec_get(m,13);
    if bm==0 {return -1;}
    let key=layer*64+kind;
    if map_has(bm,key)==0 {return 0;}
    let st=map_erase(bm,key);
    if st!=0 {return st;}
    rec_set(m,14,rec_get(m,14)-1);
    return 0;
}
fn bind_from_loader(m:i64,loader:i64,layer:i64,kind:i64)->i64 {
    if loader==0 {return -1;}
    let t=loader_tensor(loader,layer,kind);
    if t==0 {return -1;}
    return bind_set(m,layer,kind,t);
}

fn vocab_set(m:i64,v:i64)->i64 {return model_set_handle(m,9,v);}
fn vocab_get(m:i64)->i64 {return model_get_int(m,9);}
fn vocab_n_tokens(m:i64)->i64 {
    let v=vocab_get(m);
    if v==0 {return -1;}
    return vocab_n(v);
}

fn model_validate(m:i64)->i64 {
    if m==0||rec_kind(m)!=1 {return -1;}
    let hp=rec_get(m,10);
    if hp==0||rec_kind(hp)!=2 {return -2;}
    let n=rec_get(m,15);
    let nl=rec_get(hp,1);
    if n!=nl {return -3;}
    let lv=rec_get(m,11);
    let lm=rec_get(m,12);
    let bm=rec_get(m,13);
    if lv==0||lm==0||bm==0 {return -4;}
    if vec_len(lv)!=n {return -5;}
    var i:i64=0;
    while i<n {
        let lh=vec_get(lv,i);
        if lh==0||rec_kind(lh)!=3 {return -6;}
        i+=1;
    }
    if rec_get(m,14)<0 {return -7;}
    return 0;
}

// strong exports used by unit-model.c
fn goose_model_new()->i64 {return model_new();}
fn goose_model_free(m:i64)->i64 {return model_free(m);}
fn goose_model_kind(m:i64)->i64 {return model_kind(m);}
fn goose_model_set_int(m:i64,k:i64,v:i64)->i64 {return model_set_int(m,k,v);}
fn goose_model_get_int(m:i64,k:i64)->i64 {return model_get_int(m,k);}
fn goose_model_set_float(m:i64,k:i64,v:f32)->i64 {return model_set_float(m,k,v);}
fn goose_model_get_float(m:i64,k:i64)->f32 {return model_get_float(m,k);}
fn goose_model_set_handle(m:i64,k:i64,v:i64)->i64 {return model_set_handle(m,k,v);}
fn goose_model_hparams_get(m:i64,k:i64)->i64 {return hparams_get(m,k);}
fn goose_model_hparams_set(m:i64,k:i64,v:i64)->i64 {return hparams_set(m,k,v);}
fn goose_model_n_layers(m:i64)->i64 {return model_n_layers(m);}
fn goose_model_layer_new(m:i64,l:i64)->i64 {return layer_new(m,l);}
fn goose_model_layer_get(m:i64,l:i64)->i64 {return layer_get(m,l);}
fn goose_model_layer_set_int(m:i64,l:i64,k:i64,v:i64)->i64 {return layer_set_int(m,l,k,v);}
fn goose_model_layer_get_int(m:i64,l:i64,k:i64)->i64 {return layer_get_int(m,l,k);}
fn goose_model_bind_set(m:i64,l:i64,k:i64,t:i64)->i64 {return bind_set(m,l,k,t);}
fn goose_model_bind_get(m:i64,l:i64,k:i64)->i64 {return bind_get(m,l,k);}
fn goose_model_bind_erase(m:i64,l:i64,k:i64)->i64 {return bind_erase(m,l,k);}
fn goose_model_bind_from_loader(m:i64,ld:i64,l:i64,k:i64)->i64 {return bind_from_loader(m,ld,l,k);}
fn goose_model_vocab_set(m:i64,v:i64)->i64 {return vocab_set(m,v);}
fn goose_model_vocab_get(m:i64)->i64 {return vocab_get(m);}
fn goose_model_vocab_n_tokens(m:i64)->i64 {return vocab_n_tokens(m);}
fn goose_model_validate(m:i64)->i64 {return model_validate(m);}

fn main() {
    // Reachability for the generated C entry wrappers in unit-model.c.
    let m=goose_model_new();
    let a=goose_model_kind(m);
    let b=goose_model_set_int(m,0,1);
    let c=goose_model_get_int(m,0);
    let d=goose_model_set_float(m,34,10000.0);
    let e=goose_model_get_float(m,34);
    let f=goose_model_set_handle(m,9,0x9000);
    let g=goose_model_hparams_set(m,1,1);
    let h=goose_model_hparams_get(m,1);
    let i=goose_model_n_layers(m);
    let j=goose_model_layer_new(m,0);
    let k=goose_model_layer_get(m,0);
    let l=goose_model_layer_set_int(m,0,0,2);
    let n=goose_model_layer_get_int(m,0,0);
    let o=goose_model_bind_set(m,0,0,0x1000);
    let p=goose_model_bind_get(m,0,0);
    let q=goose_model_bind_erase(m,0,0);
    let r=goose_model_bind_from_loader(m,0x8000,0,1);
    let s=goose_model_vocab_set(m,0x9000);
    let t=goose_model_vocab_get(m);
    let u=goose_model_vocab_n_tokens(m);
    let v=goose_model_validate(m);
    let w=goose_model_free(m);
    print(a+b+c+d+f+g+h+i+j+k+l+n+o+p+q+r+s+t+u+v+w);
    print(e);
}
