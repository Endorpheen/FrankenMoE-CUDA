// Engine GGUF v2/v3 reader. No native reads, strings, containers or allocations.
// Borrowed immutable bytes; caller-owned disjoint records and hash slots.
// KV stride8: name_off,len,wire_type,element_type,payload_off,count,bytes,hash.
// Tensor stride12: name_off,len,type,declared_dims,ne[4],rel,bytes,abs,hash.
// State8: magic919,version,n_kv,n_tensors,alignment,data_off,padded_data,meta_end.
// A failed parse clears magic; scratch outputs may be dirty and are unpublished.
//
// This file is the import target used by the loader and trunk modules. The
// implementation lives in the EXP937 reader (experiments/EXP-2026-11-17-937),
// which is a strict superset of this one; keeping both bodies here made
// compose-goose.py emit two copies of every gr_* function and the second copy
// silently won. Only what the EXP937 reader does not define stays below.

// grok: see experiments/EXP-2026-11-17-937-goose-gguf-arrays/source/reader.goose
