#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
struct state {int32_t children,views,buffer;uint32_t pad0;int32_t chunk;uint32_t pad1;uint64_t offset;bool allocated;uint8_t pad2[7];};
int fg_selfhost_parent_done(void*,bool);
bool fg_selfhost_view_done(void*);
__attribute__((noinline)) static int native_parent(void *p,bool is_view){struct state*s=p;s->children-=1;if(s->children==0&&s->views==0){if(is_view)return 2;if(s->allocated)return 1;}return 0;}
__attribute__((noinline)) static bool native_view(void *p){struct state*s=p;s->views-=1;return s->views==0&&s->children==0&&s->allocated;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){struct state parent={.allocated=true},owner={.allocated=true};for(int pass=0;pass<7;pass++)for(int k=0;k<2;k++){int goose=(pass+k)%2;int(*pfn)(void*,bool)=goose?fg_selfhost_parent_done:native_parent;bool(*vfn)(void*)=goose?fg_selfhost_view_done:native_view;uint64_t checksum=0;double t=now();for(int i=0;i<1000000;i++){parent.children=i%8+1;parent.views=i%3;owner.children=i%4;owner.views=i%5+1;int a=pfn(&parent,i%2);bool b=vfn(&owner);checksum+=(uint64_t)a+b+(uint32_t)parent.children+(uint32_t)owner.views;}double ns=(now()-t)*1e9/2000000;printf("{\"pass\":%d,\"goose\":%d,\"ns_update\":%.6f,\"checksum\":%llu,\"preliminary\":true}\n",pass,goose,ns,(unsigned long long)checksum);}}
