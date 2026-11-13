#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int64_t fg149_pool_control(int64_t,int64_t,const uint8_t*,int64_t,uint8_t*,int64_t);
static int64_t m[128];static uint8_t mask_result[512];static int masks,applies,mask_frees,numactl,priority_calls;static int64_t policy,level;static int fail_mask;
int64_t fg149_meta(int64_t c,int64_t field){(void)c;if(field>=500000){int64_t a=field-500000;return(a%512)*2+a/512;}if(field<0||field>=128)abort();return m[field];}
void fg149_set(int64_t c,int64_t field,int64_t value){(void)c;if(field==13)m[15]=value;else if(field==14)m[17]=value;else if(field==7)m[11]=value;else abort();}
int64_t fg149_action(int64_t c,int64_t op,int64_t a,int64_t b){(void)c;
 switch(op){case 9:case 10:case 12:return 0;case 23:m[18]=a;return 0;case 40:if(fail_mask)return 0;memset(mask_result,0,512);++masks;return 1;case 41:if(a<0||a>=512)abort();mask_result[a]=1;return 0;case 42:++applies;return 0;case 43:++mask_frees;return 0;case 44:++numactl;return 0;case 26:++priority_calls;policy=a;level=b;return 1;default:abort();}
}
static void check(int yes){if(!yes)abort();}
static int64_t ref_mask(const uint8_t*x,uint8_t*y,int strict,int64_t at){if(!strict){memcpy(y,x,512);return at;}memset(y,0,512);for(int i=0;i<512;++i){int k=(int)((at+i)%512);if(x[k]){y[k]=1;return k+1;}}return at;}
int main(void){int cases=0;uint8_t x[514],y[514],z[514];int cursor[]={0,1,15,127,511,512};
 for(int pattern=0;pattern<32;++pattern)for(int strict=0;strict<2;++strict)for(int i=0;i<6;++i){memset(x,0,514);memset(y,0x5a,514);memset(z,0x5a,514);for(int k=0;k<512;++k)x[k+1]=pattern==0?0:pattern==1?k==511:pattern==2?k==0:pattern==3?1:k%(pattern+1)==0;m[20]=strict;m[21]=cursor[i];int64_t ref=ref_mask(x+1,z+1,strict,cursor[i]);int64_t got=fg149_pool_control(3,0,x+1,512,y+1,512);check(got==ref&&!memcmp(y,z,514));int expected=0;for(int k=1;k<=512;++k)expected|=x[k];check(fg149_pool_control(2,0,x+1,512,NULL,0)==!!expected);++cases;}
 memset(m,0,sizeof(m));m[70]=2;m[73]=8;m[72]=1;m[100]=m[101]=4;
 for(int strategy=0;strategy<5;++strategy)for(int thread=0;thread<16;++thread){m[71]=strategy;m[20]=thread;masks=applies=mask_frees=numactl=0;fg149_pool_control(16,0,NULL,0,NULL,0);if(strategy==1||strategy==2){check(masks==1&&applies==1&&mask_frees==1&&!numactl);for(int k=0;k<8;++k)check(mask_result[k]==(k%2==(strategy==1?thread%2:1)));}else if(strategy==3)check(numactl==1&&!masks&&!applies&&!mask_frees);else check(!masks&&!applies&&!mask_frees&&!numactl);++cases;}
 masks=applies=mask_frees=0;fg149_pool_control(17,0,NULL,0,NULL,0);check(masks==1&&applies==1&&mask_frees==1);for(int k=0;k<8;++k)check(mask_result[k]==1);++cases;
 masks=applies=mask_frees=0;fail_mask=1;m[71]=1;fg149_pool_control(16,0,NULL,0,NULL,0);check(!applies&&!mask_frees);fail_mask=0;++cases;
 m[80]=0;m[81]=3;m[82]=1;
 for(int value=-2;value<=4;++value){m[20]=value;priority_calls=0;check(fg149_pool_control(18,0,NULL,0,NULL,0)==1);check(priority_calls==(value!=0));if(value){check(policy==(value==-1?3:value>=1&&value<=3?1:0));check(level==(value==1?40:value==2?80:value==3?90:0));}++cases;}
 memset(m,0,sizeof(m));m[10]=16;int32_t counters[]={0,65535,0x7fff0008,(int32_t)0xffff0008u,-1};
 for(int i=0;i<5;++i){m[18]=counters[i];m[20]=8;uint32_t next=((((uint32_t)counters[i]>>16)+1u)&65535u)<<16|8u;fg149_pool_control(13,0,NULL,0,NULL,0);check(m[18]==(int64_t)(int32_t)next);++cases;}
 for(int worker=0;worker<16;++worker){m[18]=0x10008;m[17]=0;m[15]=m[12]=m[11]=0;m[16]=worker;check(fg149_pool_control(9,0,NULL,0,NULL,0)==1);check(m[15]==(worker<8)&&m[17]==0x10008);++cases;}
 printf("{\"status\":\"PASS_GOOSE_MASK_NUMA_PRIORITY_GRAPH_GENERATION_POLICY\",\"cases\":%d,\"scope\":\"Actual Goose policy with mock OS metadata/primitives;not a multi-NUMA hardware benchmark\"}\n",cases);return 0;}
