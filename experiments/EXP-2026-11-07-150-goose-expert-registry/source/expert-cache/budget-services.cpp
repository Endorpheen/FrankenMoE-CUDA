#include "budget.h"
#include <cstdlib>
#define FG_CHECK(x) do{if(!(x))std::abort();}while(0)
extern "C" int64_t fg150_budget_get(int64_t context,int64_t field,int64_t i){auto&q=*(fg150_budget_request*)(uintptr_t)context;FG_CHECK(i>=0);switch(field){
case 0:return q.bytes;case 1:return q.used;case 2:return q.devices.size();case 3:return q.groups.size();case 4:return q.split.size();
case 10:return q.devices.at(i).host;case 11:return q.devices.at(i).free_bytes;case 12:return q.devices.at(i).alignment;case 13:return q.devices.at(i).split_index;
case 20:return q.groups.at(i).device;case 21:return q.groups.at(i).host;case 22:return q.groups.at(i).layers;case 23:return q.groups.at(i).experts;case 24:return q.groups.at(i).first;case 25:return q.groups.at(i).banks;case 26:return q.groups.at(i).slots;case 30:return q.strides.at(i);case 40:return q.tensors.at(i);case 50:return q.host_tensors;default:abort();}}
extern "C" double fg150_budget_float(int64_t context,int64_t field,int64_t i){auto&q=*(fg150_budget_request*)(uintptr_t)context;FG_CHECK(i>=0);if(field==0)return q.split.at(i);FG_CHECK(field==1);return q.devices.at(i).split;}
extern "C" void fg150_budget_put(int64_t context,int64_t field,int64_t i,int64_t value){auto&q=*(fg150_budget_request*)(uintptr_t)context;FG_CHECK(i>=0&&value>=0);if(field==0)q.groups.at(i).slots=value;else if(field==1)q.tensors.at(i)=value;else {FG_CHECK(field==2);q.host_tensors=value;}}
extern "C" void fg150_budget_put_float(int64_t context,int64_t i,double value){auto&q=*(fg150_budget_request*)(uintptr_t)context;FG_CHECK(i>=0);q.devices.at(i).split=value;}

#include <cstring>
extern "C" uint64_t fg150_budget_double_bits(double value){uint64_t bits;static_assert(sizeof(bits)==sizeof(value),"IEEE binary64 ABI");std::memcpy(&bits,&value,sizeof(bits));return bits;}
