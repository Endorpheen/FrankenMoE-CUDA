static void budget_cases(){
 for(int test=0;test<2000;test++){
  fg150_budget_request goose={};goose.bytes=random_word()%(8*1024*1024);goose.used=1+random_word()%8;
  int devices=1+random_word()%4;goose.devices.resize(devices);goose.tensors.resize(devices);
  for(int i=0;i<devices;i++)goose.devices[i]={0,(int64_t)(random_word()%(16*1024*1024)),int64_t(32)<<(random_word()%4),i,0};
  for(int i=0;i<1+(test%6);i++){
   int device=random_word()%devices,layers=1+random_word()%8,experts=1+random_word()%32,banks=1+random_word()%4;int64_t host=0,first=goose.strides.size();
   for(int j=0;j<banks;j++){int64_t stride=64+(random_word()%1024);goose.strides.push_back(stride);host+=stride*experts*layers;}
   goose.groups.push_back({device,host,layers,experts,first,banks,0});goose.devices[device].host+=host;
  }
  if(test%3)for(int i=0;i<devices;i++)goose.split.push_back(test%3==1?0.f:float(random_word()%100));
  if(test%7==0)for(auto&d:goose.devices)d.free_bytes=0;
  auto native=goose;bool by_free=native.split.empty()||std::all_of(native.split.begin(),native.split.end(),[](float x){return x==0.f;});double sum=0;
  for(auto&d:native.devices){if(d.host==0)continue;d.split=by_free?double(d.free_bytes):double(native.split[d.split_index]);sum+=d.split;}
  if(sum==0)for(auto&d:native.devices){d.split=d.host>0?1.:0.;sum+=d.split;}
  for(auto&g:native.groups){auto&d=native.devices[g.device];size_t budget=(size_t)(double(native.bytes)*d.split/sum*g.host/d.host);
   auto bytes=[&](int slots){size_t n=0;for(int b=0;b<g.banks;b++){size_t raw=native.strides[g.first+b]*(slots+1);n+=(raw+d.alignment-1)/d.alignment*d.alignment;}return n;};
   while(g.slots<g.layers*g.experts&&bytes(g.slots+1)<=budget)g.slots++;
   if(g.slots<native.used){g.slots=0;continue;}native.tensors[g.device]+=g.banks*(1+g.layers);native.host_tensors+=g.layers;
  }
  CHECK(fg150_cache(8,(int64_t)(uintptr_t)&goose)==(native.host_tensors>0));
  CHECK(goose.tensors==native.tensors&&goose.host_tensors==native.host_tensors);
  for(int i=0;i<devices;i++)CHECK(goose.devices[i].split==native.devices[i].split);
  for(size_t i=0;i<goose.groups.size();i++)CHECK(goose.groups[i].slots==native.groups[i].slots);
  checks++;
 }
}
