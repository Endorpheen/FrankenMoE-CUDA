#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
int64_t fg146_backend_api(int64_t,int64_t);
static int64_t values[128];
static int calls,fail_alloc,allocs,frees;
static size_t length;
int64_t fg146_meta(int64_t c,int64_t field){(void)c;if(field<0||field>=128)abort();return values[field];}
float fg146_float(int64_t c,int64_t field){(void)c;(void)field;return 0;}
int64_t fg146_action(int64_t c,int64_t op,int64_t a,int64_t b){(void)c;++calls;
 if(op==20){if(fail_alloc)return 0;void*p=NULL;if(posix_memalign(&p,64,(size_t)a))return 0;++allocs;return(int64_t)(uintptr_t)p;}
 if(op==22){free((void*)(uintptr_t)a);++frees;return 0;}
 if(op==24){memset((void*)(uintptr_t)a,(int)b,length);return 0;}
 if(op==25){memmove((void*)(uintptr_t)a,(const void*)(uintptr_t)b,length);return 0;}
 abort();
}
static void check(int x){if(!x)abort();}
static void reset(void){memset(values,0,sizeof(values));values[71]=32;calls=0;length=0;}
static void reject(int mode,int field,int64_t value){pid_t p=fork();check(p>=0);if(!p){FILE*f=freopen("/dev/null","w",stderr);(void)f;values[field]=value;fg146_backend_api(mode,0);_exit(0);}int s;check(waitpid(p,&s,0)==p);check(WIFEXITED(s)&&WEXITSTATUS(s)==1);}
int main(void){int cases=0;reset();values[61]=128;int64_t ptr=fg146_backend_api(20,0);check(ptr>UINT32_MAX&&allocs==1);++cases;
 values[60]=ptr;check(fg146_backend_api(19,0)==ptr);++cases;
 values[60]=ptr+1;check(fg146_backend_api(19,0)==ptr+32);++cases;
 values[60]=ptr;values[62]=1;fg146_backend_api(21,0);check(frees==0);++cases;
 values[62]=0;fg146_backend_api(21,0);check(frees==1);++cases;
 reset();values[61]=64;fail_alloc=1;check(fg146_backend_api(20,0)==0);fail_alloc=0;++cases;
 values[61]=0;check(fg146_backend_api(20,0)==0);++cases;
 values[61]=-1;check(fg146_backend_api(20,0)==0);++cases;
 unsigned char a[512] __attribute__((aligned(64))),b[512] __attribute__((aligned(64)));memset(a,0xcc,sizeof(a));memset(b,0x37,sizeof(b));
 reset();values[60]=(int64_t)(uintptr_t)a;values[61]=sizeof(a);values[63]=values[60]+32;values[69]=256;values[66]=0x41;
 for(int i=0;i<128;++i){values[64]=(i*37)%257;length=(size_t)(256-values[64]);values[65]=(int64_t)length;fg146_backend_api(22,0);check(a[31]==0xcc&&a[288]==0xcc);++cases;}
 values[64]=17;values[65]=length=111;values[67]=(int64_t)(uintptr_t)b;fg146_backend_api(23,0);check(!memcmp(a+49,b,111));++cases;
 values[68]=(int64_t)(uintptr_t)(b+200);fg146_backend_api(24,0);check(!memcmp(b+200,a+49,111));++cases;
 values[64]=256;values[65]=length=0;int before=calls;values[67]=0;fg146_backend_api(23,0);check(calls==before);++cases;
 values[64]=0;values[65]=length=256;reject(23,64,-1);++cases;reject(23,65,257);++cases;reject(23,64,INT64_MAX);++cases;reject(23,61,31);++cases;reject(23,67,0);++cases;
 values[65]=length=0;values[64]=0;reject(27,60,values[60]+1);++cases;reject(19,60,INT64_MAX);++cases;
 values[60]=(int64_t)(uintptr_t)a;values[61]=512;values[65]=length=512;values[66]=0x5a;fg146_backend_api(26,0);check(a[0]==0x5a&&a[511]==0x5a);++cases;
 values[60]=0;values[61]=values[65]=length=0;before=calls;fg146_backend_api(26,0);check(calls==before);++cases;
 printf("{\"status\":\"PASS_REAL_GOOSE_BUFFER_POLICY\",\"cases\":%d,\"allocs\":%d,\"frees\":%d,\"scope\":\"Actual generated Goose with mock system primitives and real high-address buffers; not OS OOM\"}\n",cases,allocs,frees);return 0;
}
