#include "edge-probe.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

static int64_t reference(const int64_t *current,const int64_t *next,const int64_t *aux,
        bool edges,bool boundary_enabled,bool history,bool extend,bool full,bool keep) {
    bool rewind=next && next[0]==6 && next[2]>0;
    bool tail=next && next[0]>=18;
    bool ordinary=current[0]<18 && next && next[0]<18 &&
        (next[0]%6!=0 || (boundary_enabled && next[0]==12));
    bool entry=edges && next && next[0]==6 && next[2]==0;
    bool final=edges && !next;
    bool retain=keep && (final || (next && (ordinary || entry || (extend &&
        ((rewind && history) || tail))) && (next[3] || full) && aux[next[0]*30+10]>=0));
    return retain + 2*(retain && (tail || final)) +
        4*(edges && current[0]==6 && current[2]==0) +
        8*(history && current[0]==6 && current[2]>0) + 16*(current[2]<0);
}
int main(void) {
    int64_t visits[8],aux[720]={0},ordinary[2],boundary[2];
    unsigned cases=0;
    goose_init(0,NULL);
    for(int a=0;a<24;++a) for(int b=0;b<24;++b)
    for(int pass=-1;pass<=2;++pass) for(int next=-1;next<=2;++next)
    for(unsigned bits=0;bits<128;++bits) {
        visits[0]=a;visits[2]=pass;visits[4]=b;visits[6]=next;visits[7]=bits&32;
        aux[b*30+10]=bits&64 ? -1 : 0;
        bool edges=bits&1, boundary_enabled=bits&2, extend=bits&4, full=bits&8, keep=bits&16;
        i2_prefill_edge_plan((sl_i64){visits,8},(sl_i64){aux,720},edges,boundary_enabled,
                            extend,full,keep,(sl_i64){ordinary,2},(sl_i64){boundary,2});
        for(int i=0;i<2;++i) {
            const int64_t *current=visits+4*i,*next_visit=i==0?visits+4:NULL;
            assert(ordinary[i]==reference(current,next_visit,aux,edges,boundary_enabled,false,extend,full,keep));
            assert(boundary[i]==reference(current,next_visit,aux,edges,boundary_enabled,true,extend,full,keep));
            cases+=2;
        }
    }
    printf("%u prefill edge comparisons passed\n",cases);
}
