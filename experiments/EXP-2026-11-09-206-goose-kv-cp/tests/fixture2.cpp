#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <random>
#include <vector>

extern "C" {
int64_t fg206d_gather(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, int64_t, int64_t);
}
static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) { if (failures < 15) printf("FAIL %s: goose=%lld ref=%lld\n", what, a, b); ++failures; }
}
static void run_trap(const char * what, int expected, void (*fn)(void)) {
    int fd[2]; if (pipe(fd) != 0) { ++failures; return; }
    pid_t pid = fork();
    if (pid == 0) { close(fd[0]); dup2(fd[1], 2); close(fd[1]); fn(); _exit(0); }
    close(fd[1]);
    char buf[256]={0}; ssize_t n=read(fd[0],buf,sizeof buf-1); close(fd[0]);
    int st=0; waitpid(pid,&st,0); ++checks;
    bool died = WIFSIGNALED(st) && WTERMSIG(st)==SIGABRT;
    long long code=(n>0&&strstr(buf,"validation code"))?atoll(strstr(buf,"validation code")+16):-1;
    if(!died||code!=expected){ if(failures<15) printf("FAIL trap %s: died=%d code=%lld exp=%d\n",what,died,code,expected); ++failures; }
}
static int64_t gp[8],gs[8*4],sp[8],ss[8*4],sh[8];
static void trap_shift(void){ sh[0]=4;
  fg206d_gather((uint64_t)(uintptr_t)gp,(uint64_t)(uintptr_t)gs,(uint64_t)(uintptr_t)sp,
                (uint64_t)(uintptr_t)ss,(uint64_t)(uintptr_t)sh,1,4); }

int main(void){
  std::mt19937 rng(206044);
  const int W=4;
  for(int t=0;t<400;t++){
    const int m=1+(int)(rng()%8);
    std::vector<int64_t> DP(m,-1),DS((size_t)m*W,-1);
    std::vector<int64_t> SP(m),SS((size_t)m*W),SH(m,0);
    for(int j=0;j<m;j++){ SP[j]=(rng()%3)==0?-1:(int64_t)(rng()%100);
      for(int k=0;k<W;k++) SS[j*W+k]=(int64_t)(rng()%1000); }
    const int64_t rc=fg206d_gather((uint64_t)(uintptr_t)DP.data(),(uint64_t)(uintptr_t)DS.data(),
                                   (uint64_t)(uintptr_t)SP.data(),(uint64_t)(uintptr_t)SS.data(),
                                   (uint64_t)(uintptr_t)SH.data(),m,W);
    ck(rc==m,"rc",rc,m);
    for(int j=0;j<m;j++){ ck(DP[j]==SP[j],"pos",DP[j],SP[j]);
      for(int k=0;k<W;k++) ck(DS[j*W+k]==SS[j*W+k],"seq",DS[j*W+k],SS[j*W+k]); }
  }
  run_trap("shifted source",601,trap_shift);
  printf("FG206D_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){ printf("EXP206_LOCAL_GATES_FAIL\n"); return 1; }
  printf("EXP206_LOCAL_GATES_PASS\n"); return 0;
}
