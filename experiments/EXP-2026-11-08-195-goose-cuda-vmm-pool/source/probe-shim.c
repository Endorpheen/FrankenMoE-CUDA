// Probe: count VMM driver calls so the port is only undertaken if the path executes.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
typedef int CUresult; typedef unsigned long long CUdeviceptr; typedef void* CUmemGenericAllocationHandle;
static _Atomic uint64_t c_create, c_reserve, c_map, c_release, c_unmap, c_free;
static void report(void){
  fprintf(stderr,"FG195_VMM cuMemCreate=%llu cuMemAddressReserve=%llu cuMemMap=%llu cuMemRelease=%llu cuMemUnmap=%llu cuMemAddressFree=%llu\n",
   (unsigned long long)atomic_load(&c_create),(unsigned long long)atomic_load(&c_reserve),
   (unsigned long long)atomic_load(&c_map),(unsigned long long)atomic_load(&c_release),
   (unsigned long long)atomic_load(&c_unmap),(unsigned long long)atomic_load(&c_free));
}
static _Atomic int once;
#define WRAP(NAME,CNT) CUresult NAME(unsigned long long a, unsigned long long b, void*c, unsigned long long d, unsigned long long e){ \
  if(!atomic_exchange(&once,1)) atexit(report); atomic_fetch_add(&CNT,1); \
  static CUresult(*nx)(unsigned long long,unsigned long long,void*,unsigned long long,unsigned long long); \
  if(!nx) nx=(CUresult(*)(unsigned long long,unsigned long long,void*,unsigned long long,unsigned long long))dlsym(RTLD_NEXT,#NAME); \
  return nx(a,b,c,d,e); }
WRAP(cuMemCreate,c_create) WRAP(cuMemAddressReserve,c_reserve) WRAP(cuMemMap,c_map)
WRAP(cuMemRelease,c_release) WRAP(cuMemUnmap,c_unmap) WRAP(cuMemAddressFree,c_free)
