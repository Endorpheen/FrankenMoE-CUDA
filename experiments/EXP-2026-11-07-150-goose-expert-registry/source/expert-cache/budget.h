#pragma once
#include <cstdint>
#include <vector>
struct fg150_budget_device{int64_t host,free_bytes,alignment,split_index;double split;};
struct fg150_budget_group{int64_t device,host,layers,experts,first,banks,slots;};
struct fg150_budget_request{int64_t bytes,used;std::vector<fg150_budget_device>devices;std::vector<fg150_budget_group>groups;std::vector<int64_t>strides,tensors;std::vector<float>split;int64_t host_tensors=0;};
extern "C" int64_t fg150_budget_get(int64_t,int64_t,int64_t);
extern "C" double fg150_budget_float(int64_t,int64_t,int64_t);
extern "C" void fg150_budget_put(int64_t,int64_t,int64_t,int64_t);
extern "C" void fg150_budget_put_float(int64_t,int64_t,double);
