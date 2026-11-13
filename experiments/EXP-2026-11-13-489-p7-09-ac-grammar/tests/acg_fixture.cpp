#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include "acg-gen.h"
#include "peg-parser.h"
#include "json-schema-to-grammar.h"
std::string string_join(const std::vector<std::string>&v,const std::string&s){std::string r;for(size_t i=0;i<v.size();++i){if(i)r+=s;r+=v[i];}return r;}
std::vector<std::string> string_split(const std::string&s,const std::string&d){std::vector<std::string>o;size_t p=0;while(true){size_t q=s.find(d,p);if(q==std::string::npos){o.push_back(s.substr(p));break;}o.push_back(s.substr(p,q-p));p=q+d.size();}return o;}
std::string string_repeat(const std::string&s,size_t n){std::string r;while(n--)r+=s;return r;}
void common_log_add(struct common_log*,enum ggml_log_level,const char*,...){}
struct common_log* common_log_main(){return nullptr;}
int common_log_get_verbosity_thold(){return 0;}
static int cases=0,values=0,fails=0;
static void bad(const char*w,const std::string&d){++fails;if(fails<=10)printf("FAIL %s: %s\n",w,d.c_str());}
static void run(int kind, common_peg_parser_builder & b, const common_peg_parser & root, const std::vector<std::string> & extra, const std::string & label){
 auto arena=b.build();
 const size_t pid=root.id();
 for (const auto & d : extra) {
  if (kind==0) std::get<common_peg_until_parser>(arena.get(pid)).delimiters.push_back(d);
 }
 std::map<std::string,std::string> nm;
 common_grammar_builder gb;
 gb.add_rule=[&](const std::string&n,const std::string&r){nm[n]=r;return n;};
 gb.add_schema=[&](const std::string&n,const common_chat_schema&){nm[n]="<schema>";return n;};
 arena.build_grammar(gb,false);
 std::vector<std::string> delims;
 if (kind==0) delims=std::get<common_peg_until_parser>(arena.get(pid)).delimiters;
 else delims=std::get<common_peg_ac_parser>(arena.get(pid)).delimiters;
 std::vector<uint8_t> strs(4096,0); std::vector<int64_t> sdir(2*64,0);
 int64_t off=0; std::vector<int64_t> ds(delims.size(),0);
 for(size_t i=0;i<delims.size();i++){ ds[i]=i; sdir[2*i+0]=off; sdir[2*i+1]=(int64_t)delims[i].size(); for(unsigned char c:delims[i]) strs[off++]=c; }
 std::vector<uint8_t> out(1<<16,0);
 std::vector<int64_t> roff(4096,0),rlen(4096,0),toff(4096,0),tlen(4096,0);
 std::vector<int64_t> head(4096,-1),ecpt(8192,0),eto(8192,0),enext(8192,0),pat(8192,-1),fail(8192,0),mt(8192,-1),queue(8192,0),order(8192,0),alpha(8192,0),st(16,0);
 ++cases;
 const int64_t n=ac_grammar(kind,(int64_t)pid,sl_u8{strs.data(),off},sl_i64{sdir.data(),(int64_t)sdir.size()},
  sl_i64{ds.data(),(int64_t)ds.size()},sl_i64{ds.data(),(int64_t)ds.size()},sl_i64{ds.data(),(int64_t)ds.size()},(int64_t)delims.size(),
  sl_u8{out.data(),(int64_t)out.size()},sl_i64{roff.data(),4096},sl_i64{rlen.data(),4096},sl_i64{toff.data(),4096},sl_i64{tlen.data(),4096},
  sl_i64{head.data(),4096},sl_i64{ecpt.data(),8192},sl_i64{eto.data(),8192},sl_i64{enext.data(),8192},sl_i64{pat.data(),8192},
  sl_i64{fail.data(),8192},sl_i64{mt.data(),8192},sl_i64{queue.data(),8192},sl_i64{order.data(),8192},sl_i64{alpha.data(),8192},sl_i64{st.data(),16});
 std::map<std::string,std::string> got;
 for(int64_t i=0;i<n;i++) got[std::string((const char*)out.data()+roff[i],(size_t)rlen[i])]=std::string((const char*)out.data()+toff[i],(size_t)tlen[i]);
 const std::string pref=(kind==0?"until-":"ac-")+std::to_string(pid);
 std::map<std::string,std::string> want;
 for(auto&kv:nm) if(kv.first.rfind(pref,0)==0) want[kv.first]=kv.second;
 ++values;
 if(got!=want){ std::string diff; for(auto&kv:want){auto it=got.find(kv.first); if(it==got.end())diff+=" missing "+kv.first; else if(it->second!=kv.second)diff+=" text "+kv.first+" got=["+it->second+"] want=["+kv.second+"]";} for(auto&kv:got) if(!want.count(kv.first))diff+=" extra "+kv.first; bad("rules",label+":"+diff); }
 // root references the start state name
 ++values;
 auto rt=nm.find("root"); std::string start=pref;
 if(rt!=nm.end() && rt->second!=start && rt->second!=("")) bad("root ref",label+" root=["+rt->second+"] start=["+start+"]");
}
int main(){ goose_init(0,0);
 { common_peg_parser_builder b; auto p=b.until("END"); b.set_root(p); run(0,b,p,{}, "until END"); }
 { common_peg_parser_builder b; auto p=b.until("a"); b.set_root(p); run(0,b,p,{}, "until a"); }
 { common_peg_parser_builder b; auto p=b.until(""); b.set_root(p); run(0,b,p,{}, "until empty"); }
 { common_peg_parser_builder b; auto p=b.until("ab"); b.set_root(p); run(0,b,p,{"bc"}, "until ab/bc"); }
 { common_peg_parser_builder b; auto p=b.until("A-B"); b.set_root(p); run(0,b,p,{}, "until A-B"); }
 { common_peg_parser_builder b; auto p=b.ac(b.any(), std::vector<std::string>{"</a>"}); b.set_root(p); run(1,b,p,{}, "ac one"); }
 { common_peg_parser_builder b; auto p=b.ac(b.any(), std::vector<std::string>{"a","ab"}); b.set_root(p); run(1,b,p,{}, "ac a/ab"); }
 { common_peg_parser_builder b; auto p=b.ac(b.any(), std::vector<std::string>{"ab","bc"}); b.set_root(p); run(1,b,p,{}, "ac ab/bc"); }
 { common_peg_parser_builder b; auto p=b.ac(b.any(), std::vector<std::string>{"", "x"}); b.set_root(p); run(1,b,p,{}, "ac empty/x"); }
 printf("FG489_ACG cases=%d values=%d failures=%d\n",cases,values,fails); return fails?1:0; }
