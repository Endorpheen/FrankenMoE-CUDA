#pragma once
/* worker-8 APX030 canonical storage: internal POD identifiers.
 *
 * These constants are private to worker-8's canonical containers.  Other
 * modules must use the typed API in canonical-storage.h and must never
 * reinterpret a handle or guess field offsets.
 *
 * Each object header starts with an alive magic guarded by CAS on free.
 * FREED is 0.  Field indices below are i64 slots from the object pointer. */

#include <stdint.h>

/* Alive magics (distinct per container kind). */
#define FGC_VEC_MAGIC INT64_C(1447379761) /* 0x56454331 "VEC1" */
#define FGC_REC_MAGIC INT64_C(1380270897) /* 0x52454331 "REC1" */
#define FGC_MAP_MAGIC INT64_C(1296126001) /* 0x4d415031 "MAP1" */
#define FGC_SET_MAGIC INT64_C(1397052465) /* 0x53455431 "SET1" */

#define FGC_VEC_HEADER_I64 5   /* magic,item_bytes,len,cap,data */
#define FGC_REC_HEADER_I64 6   /* magic,kind,n,cap,keys,vals */
#define FGC_MAP_HEADER_I64 7   /* magic,cap,used,tombs,keys,vals,states */
#define FGC_SET_HEADER_I64 7   /* magic,size,root,n_nodes,cap,free_head,nodes */
#define FGC_SET_NODE_I64   6   /* key,left,right,parent,height,next_free */

#define FGC_MAP_STATE_EMPTY 0
#define FGC_MAP_STATE_USED 1
#define FGC_MAP_STATE_TOMB 2
