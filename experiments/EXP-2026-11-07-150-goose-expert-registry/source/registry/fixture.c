#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
struct request {const char**lines;int n,pos;const char*line;char brand[1024];int closes;int64_t index,size,align,pointer;int platform,brand_size;int caps[5],props,allocs,frees,wraps,configured,fail;};
#include "/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107/source/cpu-backend/registry/bridge.h"
int64_t fg150_registry_meta(int64_t c,int64_t f){struct request*r=(void*)(uintptr_t)c;if(f>=10000){assert(f-10000<r->brand_size);return(uint8_t)r->brand[f-10000];}if(f>=100){assert(f-100<(int64_t)strlen(r->line));return(uint8_t)r->line[f-100];}switch(f){case 0:return strlen(r->line);case 1:return r->index;case 2:return 2;case 3:return r->size;case 4:return r->align;case 5:return r->pointer;case 6:return r->platform;default:abort();}}
int64_t fg150_registry_action(int64_t c,int64_t op,int64_t a,int64_t b){struct request*r=(void*)(uintptr_t)c;switch(op){case 0:return r->n>=0?1:0;case 1:if(r->pos>=r->n)return 0;r->line=r->lines[r->pos++];return 1;case 2:assert(a>=0&&b>=a&&b-a<1024);memcpy(r->brand,r->line+a,b-a);r->brand[b-a]=0;return 0;case 3:r->closes++;return 0;case 4:return 41;case 5:return 0;case 6:return 61;case 7:assert(a>=0&&a<4);r->props|=1<<a;return 0;case 8:assert(a>=0&&a<5);r->caps[a]=b;return 0;case 9:return 91;case 10:return 101;case 11:return 111;case 12:return 121;case 13:return 131;case 14:assert(a==r->align&&b==r->size);if(r->fail==1)return 0;r->allocs++;return 141;case 15:assert(a==141&&b==r->size);r->wraps++;return r->fail==2?0:151;case 16:assert(a==141);r->frees++;return 0;case 17:assert(a==151);r->configured++;return 0;
case 18:return r->fail==1?0:strlen(r->line)+1;
case 19:assert(a>=0&&a<1024);r->brand_size=a;r->brand[a]=0;return 0;
case 20:case 23:if(r->fail==2)return 0;assert(strlen(r->line)+1<=(size_t)r->brand_size);memcpy(r->brand,r->line,strlen(r->line)+1);return 1;
case 21:return r->fail==3?0:211;
case 22:assert(a==211);return r->fail==1?0:strlen(r->line)+1;
case 24:assert(a==211);r->closes++;return 0;
default:abort();}}
static int checks;
int main(void){
 const char*lines[]={"processor : 0\n","model name :   AMD Ryzen 9 5950X 16-Core Processor \t\n","model name: ignored"};
 struct request r={0};r.lines=lines;r.n=3;assert(fg150_registry(0,(int64_t)(uintptr_t)&r)==1&&!strcmp(r.brand,"AMD Ryzen 9 5950X 16-Core Processor")&&r.closes==1&&r.pos==2);checks++;
 const char*cases[]={"","model","model name","model name: \t\n","model name:","model name:CPU","model name:\t\v\f\r CPU\t\v\f\r\n"};
 const char*expected[]={"CPU","CPU","CPU","","","CPU","CPU"};
 for(int i=0;i<7;i++){r=(struct request){0};r.lines=&cases[i];r.n=1;strcpy(r.brand,"CPU");fg150_registry(0,(int64_t)(uintptr_t)&r);assert(!strcmp(r.brand,expected[i])&&r.closes==1);checks++;}
 r=(struct request){0};r.n=-1;strcpy(r.brand,"CPU");assert(fg150_registry(0,(int64_t)(uintptr_t)&r)==0&&r.closes==0&&!strcmp(r.brand,"CPU"));checks++;
 for(int mode=1;mode<10;mode++){r=(struct request){0};int64_t value=fg150_registry(mode,(int64_t)(uintptr_t)&r);if(mode==3){assert(r.props==15&&r.caps[0]==0&&r.caps[1]==0&&r.caps[2]==1&&r.caps[3]==0&&r.caps[4]==1);}else{int64_t expected[]={0,41,61,0,91,2,101,111,121,131};assert(value==expected[mode]);}checks++;}
 for(int platform=1;platform<=3;platform++)for(int fail=0;fail<4;fail++){
  r=(struct request){0};r.platform=platform;r.fail=fail;r.line="AMD Test CPU";strcpy(r.brand,"CPU");
  int64_t value=fg150_registry(0,(int64_t)(uintptr_t)&r);
  if(platform<3&&fail==0){assert(value==1&&!strcmp(r.brand,"AMD Test CPU"));if(platform==2)assert(r.brand_size==(int)strlen(r.line));}
  if(platform==3||fail==1||(platform==2&&fail==3))assert(value==0);
  if(platform==2)assert(r.closes==(fail==3?0:1));checks++;
 }
 for(int fail=0;fail<3;fail++)for(int align=8;align<=256;align*=2)for(int size=0;size<=513;size+=57){r=(struct request){0};r.fail=fail;r.align=align;r.size=size;int64_t result=fg150_registry(10,(int64_t)(uintptr_t)&r);
  if(fail==0){assert(result==151&&r.allocs==1&&r.wraps==1&&r.frees==0&&r.configured==1);r.pointer=141;fg150_registry(11,(int64_t)(uintptr_t)&r);assert(r.frees==1);}
  if(fail==1)assert(result==0&&r.allocs==0&&r.wraps==0&&r.frees==0);
  if(fail==2)assert(result==0&&r.allocs==1&&r.wraps==1&&r.frees==1&&r.configured==0);
  checks++;
 }
 printf("{\"status\":\"PASS_REGISTRY_AND_HBM_POLICY\",\"cases\":%d,\"hardware_hbm\":false}\n",checks);
}
