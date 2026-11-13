#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include "ac-gen.h"
#include "trie.h"
static int cases=0, values=0, fails=0;
static void bad(const char*w,const std::string&d){++fails;if(fails<=10)printf("FAIL %s: %s\n",w,d.c_str());}
int main(){
 goose_init(0,0);
 const std::vector<std::vector<std::string>> sets={
  {"END"},{"a"},{"a","ab","abc"},{"ab","b"},{"","x"},{"aXb","aYb"},{"AB","ABC","B"},
  {"\xc3\xa9","\xc3\xa8"},{"xy","xyz","yz"},{"q"},{"bc","abcx"},{"a","a"},
 };
 for(auto& words: sets){
  common_aho_corasick nac(words);
  std::vector<uint8_t> sbuf(4096,0); std::vector<int64_t> sdir(2*64,0);
  int64_t off=0;
  for(size_t i=0;i<words.size();i++){ sdir[2*i+0]=off; sdir[2*i+1]=(int64_t)words[i].size(); for(unsigned char c:words[i]) sbuf[off++]=c; }
  std::vector<int64_t> head(1024,-1),ecpt(4096,0),eto(4096,0),enext(4096,0),pat(4096,-1),fail(4096,0),match(4096,-1),queue(4096,0),order(4096,0),st(8,0);
  const int64_t ns=ac_build(sl_u8{sbuf.data(),off}, sl_i64{sdir.data(),2*(int64_t)words.size()}, (int64_t)words.size(),
    sl_i64{head.data(),1024}, sl_i64{ecpt.data(),4096}, sl_i64{eto.data(),4096}, sl_i64{enext.data(),4096},
    sl_i64{pat.data(),4096}, sl_i64{fail.data(),4096}, sl_i64{match.data(),4096}, sl_i64{queue.data(),4096}, sl_i64{order.data(),4096}, sl_i64{st.data(),8});
  ++cases;
  if(ns!=(int64_t)nac.num_states()){ bad("states",std::to_string(ns)+" vs "+std::to_string(nac.num_states())); continue; }
  bool ok=true;
  for(size_t s=0;s<nac.num_states()&&ok;s++){
   ++values;
   if((int64_t)nac.match_pattern(s)!=ac_terminal(sl_i64{match.data(),4096},(int64_t)s)){ bad("terminal","state "+std::to_string(s)); ok=false; }
   for(uint32_t cp: std::set<uint32_t>{'a','b','c','d','e','x','y','z','A','B','C','D','E','N','X','Y','Z','q',0x00e9,0x00e8}){
    ++values;
    int64_t want=(int64_t)nac.next(s,cp);
    int64_t got=ac_next((int64_t)s,(int64_t)cp,sl_i64{head.data(),1024},sl_i64{ecpt.data(),4096},sl_i64{eto.data(),4096},sl_i64{enext.data(),4096},sl_i64{fail.data(),4096});
    if(want!=got){ bad("next","state "+std::to_string(s)+" cp "+std::to_string(cp)+" want "+std::to_string(want)+" got "+std::to_string(got)); ok=false; break; }
   }
  }
 }
 printf("FG488_AC cases=%d values=%d failures=%d\n",cases,values,fails);
 return fails?1:0;
}
