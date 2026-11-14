// Tools grammar gate: eight tagged arenas, REQUIRED, no params, markers cleared.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "runtime-gen.h"
extern "C" double gs_json_strtod(sl_u8 s){std::string t((const char*)s.data,(size_t)s.len);return std::strtod(t.c_str(),nullptr);}
extern "C" int64_t gs_json_fmt_e(sl_u8 o,int64_t p,double x){return snprintf((char*)o.data,(size_t)o.len,"%.*e",(int)p,x);}
extern "C" int64_t gs_json_fmt_g(sl_u8 o,double x){return snprintf((char*)o.data,(size_t)o.len,"%.17g",x);}
extern "C" double gs_frexp_mant(double x){int e=0;return std::frexp(x,&e);}
extern "C" int64_t gs_to_string_f64(sl_u8 o,double x){auto t=std::to_string(x);memcpy(o.data,t.data(),t.size());return (int64_t)t.size();}
extern "C" int64_t gs_strftime_epoch(sl_u8 o,int64_t e,sl_u8 f){std::string s((const char*)f.data,(size_t)f.len);std::time_t t=e<0?std::time(nullptr):(std::time_t)e;auto tm=std::localtime(&t);if(!tm)return -1;return (int64_t)std::strftime((char*)o.data,(size_t)o.len,s.c_str(),tm);}
static sl_i64 li(std::vector<int64_t>&v){return sl_i64{v.data(),(int64_t)v.size()};}
static sl_u8 bu(std::vector<uint8_t>&v){return sl_u8{v.data(),(int64_t)v.size()};}
static std::vector<uint8_t> vb(const std::string&s){return std::vector<uint8_t>(s.begin(),s.end());}
struct Arena{std::vector<int64_t> kind,cfirst,ccount,children,sidx,dstart,dcount,mn,mx,st;std::vector<uint8_t> pool;
 Arena():kind(4096,0),cfirst(4096,0),ccount(4096,0),children(32768,0),sidx(4096,0),dstart(4096,0),dcount(4096,0),mn(4096,0),mx(4096,0),st(128,0),pool(1<<16,0){}};
static std::string tools_grammar(const std::string&gp,const std::string&rs,const std::string&fnp,const std::string&nsfx,const std::string&fcl,
 const std::string&anp,const std::string&ans,const std::string&avp,const std::string&avs,int64_t parallel,int64_t tol){
 Arena a;std::vector<int64_t> noff{0},nlen{8},soff{0},slen{0},poff{0},plen{0},ptype{0},preq{0},pbase{0},pcount{0},pso{0},psl{0};
 auto g=vb(gp),r1=vb(rs),r2=vb(""),f1=vb(fnp),f2=vb(nsfx),f3=vb(fcl),a1=vb(anp),a2=vb(ans),a3=vb(avp),a4=vb(avs),e=vb("");
 auto nm=vb("get_time"),sc=vb(""),pn=vb(""),psc=vb("");
 int64_t root=autoparser_build_tag_tagged(bu(g),bu(r1),bu(r2),0,bu(f1),bu(f2),bu(f3),bu(a1),bu(a2),bu(a3),bu(a4),
  bu(e),bu(e),bu(e),bu(e),bu(e),bu(e),bu(e),bu(e),0,tol,bu(nm),li(noff),li(nlen),bu(sc),li(soff),li(slen),bu(pn),
  li(poff),li(plen),li(ptype),li(preq),li(pbase),li(pcount),bu(psc),li(pso),li(psl),1,parallel,1,
  li(a.kind),li(a.cfirst),li(a.ccount),li(a.children),li(a.sidx),li(a.dstart),li(a.dcount),li(a.mn),li(a.mx),bu(a.pool),li(a.st));
 std::vector<int64_t> meta(8,0),vis(4096,0),rids(4096,0),hids(4096,0),roff(4096,0),rlen(4096,0),boff(4096,0),blen(4096,0);
 std::vector<uint8_t> scratch(1<<20,0),out(1<<20,0);
 int64_t len=autoparser_arena_gbnf(root,0,a.st[60],li(a.kind),li(a.cfirst),li(a.ccount),li(a.children),li(a.sidx),
  li(a.dstart),li(a.dcount),li(a.mn),li(a.mx),bu(a.pool),li(meta),li(vis),li(rids),li(hids),li(roff),li(rlen),li(boff),li(blen),bu(scratch),bu(out));
 if(meta[3])return "<unsupported>";return std::string((const char*)out.data(),(size_t)len);}
static int gc=0,gf=0;
static void gcheck(const char*n,const std::string&got,const std::string&want){++gc;if(got!=want){++gf;size_t i=0;while(i<got.size()&&i<want.size()&&got[i]==want[i])++i;
 printf("FAIL %s diff %zu\n got =[%s]\n want=[%s]\n",n,i,got.substr(i<60?0:i-60,240).c_str(),want.substr(i<60?0:i-60,240).c_str());}else printf("ok %s\n",n);}
int main(){goose_init(0,0);
    gcheck("glm_par", tools_grammar(R"XT(<|assistant|>)XT",R"XT(<think>)XT",R"XT()XT",R"XT(
)XT",R"XT()XT",R"XT(<arg_key>)XT",R"XT(</arg_key>
)XT",R"XT(<arg_value>)XT",R"XT(</arg_value>
)XT",1,0), R"XT(root ::= "<|assistant|>" space ? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "" space (tool-get-time) (", " (tool-get-time))* space ""
tool-get-time ::= ("" "get_time" "\n") space space
)XT");
    gcheck("glm_ser", tools_grammar(R"XT(<|assistant|>)XT",R"XT(<think>)XT",R"XT()XT",R"XT(
)XT",R"XT()XT",R"XT(<arg_key>)XT",R"XT(</arg_key>
)XT",R"XT(<arg_value>)XT",R"XT(</arg_value>
)XT",0,0), R"XT(root ::= "<|assistant|>" space ? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "" space (tool-get-time) space ""
tool-get-time ::= ("" "get_time" "\n") space space
)XT");
    gcheck("t12_par", tools_grammar(R"XT(<assistant>)XT",R"XT()XT",R"XT(<function=)XT",R"XT(>)XT",R"XT(</args></function>)XT",R"XT(<param=)XT",R"XT(>)XT",R"XT()XT",R"XT(</param>)XT",1,0), R"XT(root ::= "<assistant>" space ? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "" space (tool-get-time) (", " (tool-get-time))* space ""
tool-get-time ::= ("<function=" "get_time" ">") space space "</args></function>"
)XT");
    gcheck("t12_ser", tools_grammar(R"XT(<assistant>)XT",R"XT()XT",R"XT(<function=)XT",R"XT(>)XT",R"XT(</args></function>)XT",R"XT(<param=)XT",R"XT(>)XT",R"XT()XT",R"XT(</param>)XT",0,0), R"XT(root ::= "<assistant>" space ? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "" space (tool-get-time) space ""
tool-get-time ::= ("<function=" "get_time" ">") space space "</args></function>"
)XT");
    gcheck("mm_par", tools_grammar(R"XT(]~b]ai
<think>
)XT",R"XT(<think>)XT",R"XT(<invoke name=")XT",R"XT(">
)XT",R"XT(</invoke>
)XT",R"XT(<parameter name=")XT",R"XT()XT",R"XT(">)XT",R"XT(</parameter>
)XT",1,0), R"XT(root ::= "]~b]ai\n" space ? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "" space (tool-get-time) (", " (tool-get-time))* space ""
tool-get-time ::= ("<invoke name=\"" "get_time" "\">\n") space space "</invoke>\n"
)XT");
    gcheck("mm_ser", tools_grammar(R"XT(]~b]ai
<think>
)XT",R"XT(<think>)XT",R"XT(<invoke name=")XT",R"XT(">
)XT",R"XT(</invoke>
)XT",R"XT(<parameter name=")XT",R"XT()XT",R"XT(">)XT",R"XT(</parameter>
)XT",0,0), R"XT(root ::= "]~b]ai\n" space ? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "" space (tool-get-time) space ""
tool-get-time ::= ("<invoke name=\"" "get_time" "\">\n") space space "</invoke>\n"
)XT");
    gcheck("lag_par", tools_grammar(R"XT(<assistant><think>)XT",R"XT(<think>)XT",R"XT()XT",R"XT()XT",R"XT()XT",R"XT(<arg_key>)XT",R"XT(</arg_key>)XT",R"XT(<arg_value>)XT",R"XT(</arg_value>)XT",1,1), R"XT(root ::= "<assistant>" space ? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "" space (tool-get-time) (", " (tool-get-time))* space ""
tool-get-time ::= ("" "get_time" "") space space
)XT");
    gcheck("lag_ser", tools_grammar(R"XT(<assistant><think>)XT",R"XT(<think>)XT",R"XT()XT",R"XT()XT",R"XT()XT",R"XT(<arg_key>)XT",R"XT(</arg_key>)XT",R"XT(<arg_value>)XT",R"XT(</arg_value>)XT",0,1), R"XT(root ::= "<assistant>" space ? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "" space (tool-get-time) space ""
tool-get-time ::= ("" "get_time" "") space space
)XT");
 printf("FG_TOOLS cases=%d failures=%d\n",gc,gf);return gf==0?0:1;}
