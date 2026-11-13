#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int64_t fg145_backend_api(int64_t,int64_t);
struct mock {int64_t meta[64];int fail;int ctx,backend,plan;int work[32];int next;int operations[20];};
int64_t fg145_meta(int64_t c,int64_t f){struct mock*m=(struct mock*)(uintptr_t)c;assert(f>=0&&f<64);return m->meta[f];}
float fg145_float(int64_t c,int64_t f){(void)c;(void)f;return 0;}
int64_t fg145_action(int64_t c,int64_t op,int64_t a,int64_t b){struct mock*m=(struct mock*)(uintptr_t)c;assert(op>=0&&op<20);m->operations[op]++;if(op==m->fail)return 0;
 switch(op){
 case 0:assert(!m->ctx);m->ctx=1;return 11;
 case 1:assert(a>=0&&a<=6);m->meta[2+a]=b;return 0;
 case 2:assert(m->ctx&&!m->backend);m->backend=1;return 22;
 case 3:assert(m->ctx);m->ctx=0;return 0;
 case 4:assert(m->backend);m->backend=0;return 0;
 case 5:{assert(a>0&&m->next<31);int id=++m->next;assert(!m->work[id]);m->work[id]=1;return 100+id;}
 case 6:if(a){int id=(int)(a-100);assert(id>0&&id<32&&m->work[id]);m->work[id]=0;}return 0;
 case 8:assert(!m->plan);m->plan=1;m->meta[12]=33;return 33;
 case 9:assert(m->plan);m->plan=0;m->meta[12]=0;return 0;
 case 10:m->meta[14]=0;return 0;
 case 11:assert(m->plan);return 0;
 case 12:assert(a>=0&&a<=3);if(a==0)m->meta[14]=b;return 0;
 case 13:assert(m->meta[14]!=0||m->meta[13]==0);return 0;
 default:assert(0);return 0;}}
static void init(struct mock*m,int failure,int64_t bytes){memset(m,0,sizeof(*m));m->fail=failure;m->meta[1]=4;m->meta[13]=bytes;m->meta[47]=-2;}
static int live_work(struct mock*m){int n=0;for(int i=1;i<32;++i)n+=m->work[i];return n;}
static void result(const char*name){printf("{\"name\":\"%s\",\"pass\":true}\n",name);}
int main(void){struct mock m;
 init(&m,0,0);assert(fg145_backend_api(0,(int64_t)(uintptr_t)&m)==0);assert(!m.ctx&&!m.backend);result("context-allocation-failure");
 init(&m,2,0);assert(fg145_backend_api(0,(int64_t)(uintptr_t)&m)==0);assert(!m.ctx&&!m.backend&&m.operations[3]==1);result("backend-allocation-failure-cleans-context");
 init(&m,8,128);assert(fg145_backend_api(2,(int64_t)(uintptr_t)&m)==0);assert(!m.plan&&!live_work(&m));result("plan-allocation-failure");
 init(&m,5,128);assert(fg145_backend_api(2,(int64_t)(uintptr_t)&m)==0);assert(!m.plan&&!live_work(&m)&&m.operations[9]==1);result("planned-workspace-failure-cleans-plan");
 init(&m,5,128);m.work[1]=1;m.next=1;m.meta[4]=101;m.meta[5]=32;assert(fg145_backend_api(5,(int64_t)(uintptr_t)&m)==-2);assert(!live_work(&m)&&m.meta[4]==0&&m.meta[5]==0);result("growth-failure-clears-released-pointer");
 m.fail=-1;assert(fg145_backend_api(5,(int64_t)(uintptr_t)&m)==0);assert(live_work(&m)==1&&m.meta[5]==128&&m.meta[14]==m.meta[4]);fg145_action((int64_t)(uintptr_t)&m,6,m.meta[4],0);result("workspace-recovers-after-failure");
 init(&m,-1,128);m.work[1]=1;m.next=1;m.meta[4]=101;m.meta[5]=256;assert(fg145_backend_api(5,(int64_t)(uintptr_t)&m)==0);assert(m.operations[5]==0&&m.operations[6]==0&&m.meta[14]==101&&m.meta[4]==101);fg145_action((int64_t)(uintptr_t)&m,6,101,0);result("sufficient-workspace-reused-without-allocation");
 init(&m,-1,0);assert(fg145_backend_api(0,(int64_t)(uintptr_t)&m)==22);fg145_backend_api(1,(int64_t)(uintptr_t)&m);assert(!m.ctx&&!m.backend&&!live_work(&m));result("backend-lifecycle-no-resources-retained");
 init(&m,-1,0);assert(fg145_backend_api(2,(int64_t)(uintptr_t)&m)==33);fg145_backend_api(3,(int64_t)(uintptr_t)&m);assert(!m.plan&&!live_work(&m)&&m.operations[5]==0);result("zero-workspace-plan-lifecycle");return 0;}
