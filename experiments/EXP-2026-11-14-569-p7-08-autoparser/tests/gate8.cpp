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
struct A{std::vector<int64_t> kind,cfirst,ccount,children,sidx,dstart,dcount,mn,mx,st;std::vector<uint8_t> pool;
 A():kind(4096,0),cfirst(4096,0),ccount(4096,0),children(32768,0),sidx(4096,0),dstart(4096,0),dcount(4096,0),mn(4096,0),mx(4096,0),st(128,0),pool(1<<16,0){}};
static std::string H(const std::string&gp,const std::string&rs,const std::string&fnp,const std::string&nsfx,const std::string&fcl,
 const std::string&anp,const std::string&ans,const std::string&avp,const std::string&avs,const std::string&ast,const std::string&aen,
 const std::string&ss,const std::string&se,const std::string&pcs,const std::string&pce,int64_t tol){
 A a;std::vector<int64_t> noff{0,4},nlen{4,8},soff{0,1},slen{0,0},poff{0,4},plen{4,5},ptype{1,1},preq{1,0},pbase{0,1},pcount{1,1},pso{0,0},psl{0,0};
 auto g=vb(gp),r1=vb(rs),r2=vb(""),f1=vb(fnp),f2=vb(nsfx),f3=vb(fcl),a1=vb(anp),a2=vb(ans),a3=vb(avp),a4=vb(avs),as=vb(ast),ae=vb(aen),s1=vb(ss),s2=vb(se),pc=vb(pcs),pe=vb(pce),e=vb("");
 auto nm=vb("echoset_mode"),sc=vb(""),pn=vb("textlevel"),psc=vb("");
 int64_t root=autoparser_build_tag_tagged(bu(g),bu(r1),bu(r2),0,bu(f1),bu(f2),bu(f3),bu(a1),bu(a2),bu(a3),bu(a4),bu(as),bu(ae),bu(s1),bu(s2),bu(pc),bu(pe),bu(e),bu(e),0,tol,
  bu(nm),li(noff),li(nlen),bu(sc),li(soff),li(slen),bu(pn),li(poff),li(plen),li(ptype),li(preq),li(pbase),li(pcount),bu(psc),li(pso),li(psl),2,1,1,
  li(a.kind),li(a.cfirst),li(a.ccount),li(a.children),li(a.sidx),li(a.dstart),li(a.dcount),li(a.mn),li(a.mx),bu(a.pool),li(a.st));
 std::vector<int64_t> meta(8,0),vis(4096,0),rids(4096,0),hids(4096,0),roff(4096,0),rlen(4096,0),boff(4096,0),blen(4096,0);std::vector<uint8_t> scratch(1<<20,0),out(1<<20,0);
 int64_t len=autoparser_arena_gbnf(root,0,a.st[60],li(a.kind),li(a.cfirst),li(a.ccount),li(a.children),li(a.sidx),li(a.dstart),li(a.dcount),li(a.mn),li(a.mx),bu(a.pool),li(meta),li(vis),li(rids),li(hids),li(roff),li(rlen),li(boff),li(blen),bu(scratch),bu(out));
 if(meta[3])return "<unsupported>";return std::string((const char*)out.data(),(size_t)len);}
static const std::string N="\n";
static int gc=0,gf=0;
static void gcheck(const char*n,const std::string&got,const std::string&want){++gc;if(got!=want){++gf;size_t i=0;while(i<got.size()&&i<want.size()&&got[i]==want[i])++i;printf("FAIL %s diff %zu len %zu/%zu\n",n,i,got.size(),want.size());}else printf("ok %s\n",n);}
static std::string H1(){return H("<assistant>","","<function=",">","</args></function>","<param=",">","","</param>","<args>","</args>","","","<tool_call>","</tool_call>",0);}
static std::string H2(){return H("<|assistant|>"+N+"<think>","<think>","",N,"","<arg_key>","</arg_key>"+N,"<arg_value>","</arg_value>"+N,"","","","","<tool_call>","</tool_call>",0);}
static std::string H3(){return H("]~b]ai"+N+"<think>","<think>","<invoke name=\"", "\">"+N,"</invoke>"+N,"<parameter name=\"","","\">","</parameter>"+N,"","","<minimax:tool_call>"+N,"</minimax:tool_call>","","",0);}
static std::string H4(){return H("<assistant><think>","<think>","","","","<arg_key>","</arg_key>","<arg_value>","</arg_value>","","","","","<tool_call>","</tool_call>",1);}
int main(){goose_init(0,0);
    gcheck("h1", H1(), R"XH(ac-133 ::= [<] ac-133-01 | [^<] ac-133
ac-133-01 ::= [<] ac-133-01 | [/] ac-133-02 | [^/<] ac-133
ac-133-02 ::= [<] ac-133-01 | [p] ac-133-03 | [^<p] ac-133
ac-133-03 ::= [<] ac-133-01 | [a] ac-133-04 | [^<a] ac-133
ac-133-04 ::= [<] ac-133-01 | [r] ac-133-05 | [^<r] ac-133
ac-133-05 ::= [<] ac-133-01 | [a] ac-133-06 | [^<a] ac-133
ac-133-06 ::= [<] ac-133-01 | [m] ac-133-07 | [^<m] ac-133
ac-133-07 ::= [>] | [<] ac-133-01 | [^<>] ac-133
ac-86 ::= [<] ac-86-01 | [^<] ac-86
ac-86-01 ::= [<] ac-86-01 | [/] ac-86-02 | [^/<] ac-86
ac-86-02 ::= [<] ac-86-01 | [p] ac-86-03 | [^<p] ac-86
ac-86-03 ::= [<] ac-86-01 | [a] ac-86-04 | [^<a] ac-86
ac-86-04 ::= [<] ac-86-01 | [r] ac-86-05 | [^<r] ac-86
ac-86-05 ::= [<] ac-86-01 | [a] ac-86-06 | [^<a] ac-86
ac-86-06 ::= [<] ac-86-01 | [m] ac-86-07 | [^<m] ac-86
ac-86-07 ::= [>] | [<] ac-86-01 | [^<>] ac-86
root ::= "<assistant>" space until-181? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "<tool_call>" space (tool-echo | tool-set-mode) space "</tool_call>" (space "<tool_call>" space (tool-echo | tool-set-mode) space "</tool_call>")* space
tool-echo ::= ("<function=" "echo" ">") space "<args>" tool-echo-arg-text "</args>" space "</args></function>"
tool-echo-arg-text ::= ("<param=" "text" ">") "" ac-86
tool-set-mode ::= ("<function=" "set_mode" ">") space "<args>" (space (tool-set-mode-arg-level))* "</args>" space "</args></function>"
tool-set-mode-arg-level ::= ("<param=" "level" ">") "" ac-133
until-181 ::= | [<] until-181-01 | [^<] until-181
until-181-01 ::= | [<] until-181-01 | [t] until-181-02 | [^<t] until-181
until-181-02 ::= | [<] until-181-01 | [o] until-181-03 | [^<o] until-181
until-181-03 ::= | [<] until-181-01 | [o] until-181-04 | [^<o] until-181
until-181-04 ::= | [<] until-181-01 | [l] until-181-05 | [^<l] until-181
until-181-05 ::= | [<] until-181-01 | [_] until-181-06 | [^<_] until-181
until-181-06 ::= | [<] until-181-01 | [c] until-181-07 | [^<c] until-181
until-181-07 ::= | [<] until-181-01 | [a] until-181-08 | [^<a] until-181
until-181-08 ::= | [<] until-181-01 | [l] until-181-09 | [^<l] until-181
until-181-09 ::= | [<] until-181-01 | [l] until-181-10 | [^<l] until-181
until-181-10 ::= | [<] until-181-01 | [^<>] until-181
)XH");
    gcheck("h2", H2(), R"XH(ac-129 ::= [<] ac-129-01 | [^<] ac-129
ac-129-01 ::= [<] ac-129-01 | [/] ac-129-02 | [^/<] ac-129
ac-129-02 ::= [<] ac-129-01 | [a] ac-129-03 | [^<a] ac-129
ac-129-03 ::= [<] ac-129-01 | [r] ac-129-04 | [^<r] ac-129
ac-129-04 ::= [<] ac-129-01 | [g] ac-129-05 | [^<g] ac-129
ac-129-05 ::= [<] ac-129-01 | [_] ac-129-06 | [^<_] ac-129
ac-129-06 ::= [<] ac-129-01 | [v] ac-129-07 | [^<v] ac-129
ac-129-07 ::= [<] ac-129-01 | [a] ac-129-08 | [^<a] ac-129
ac-129-08 ::= [<] ac-129-01 | [l] ac-129-09 | [^<l] ac-129
ac-129-09 ::= [<] ac-129-01 | [u] ac-129-10 | [^<u] ac-129
ac-129-10 ::= [<] ac-129-01 | [e] ac-129-11 | [^<e] ac-129
ac-129-11 ::= [<] ac-129-01 | [>] ac-129-12 | [^<>] ac-129
ac-129-12 ::= [\n] | [<] ac-129-01 | [^\n<] ac-129
ac-86 ::= [<] ac-86-01 | [^<] ac-86
ac-86-01 ::= [<] ac-86-01 | [/] ac-86-02 | [^/<] ac-86
ac-86-02 ::= [<] ac-86-01 | [a] ac-86-03 | [^<a] ac-86
ac-86-03 ::= [<] ac-86-01 | [r] ac-86-04 | [^<r] ac-86
ac-86-04 ::= [<] ac-86-01 | [g] ac-86-05 | [^<g] ac-86
ac-86-05 ::= [<] ac-86-01 | [_] ac-86-06 | [^<_] ac-86
ac-86-06 ::= [<] ac-86-01 | [v] ac-86-07 | [^<v] ac-86
ac-86-07 ::= [<] ac-86-01 | [a] ac-86-08 | [^<a] ac-86
ac-86-08 ::= [<] ac-86-01 | [l] ac-86-09 | [^<l] ac-86
ac-86-09 ::= [<] ac-86-01 | [u] ac-86-10 | [^<u] ac-86
ac-86-10 ::= [<] ac-86-01 | [e] ac-86-11 | [^<e] ac-86
ac-86-11 ::= [<] ac-86-01 | [>] ac-86-12 | [^<>] ac-86
ac-86-12 ::= [\n] | [<] ac-86-01 | [^\n<] ac-86
root ::= "<|assistant|>\n" space until-173? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "<tool_call>" space (tool-echo | tool-set-mode) space "</tool_call>" (space "<tool_call>" space (tool-echo | tool-set-mode) space "</tool_call>")* space
tool-echo ::= ("" "echo" "\n") space tool-echo-arg-text 
tool-echo-arg-text ::= ("<arg_key>" "text" "</arg_key>\n") "<arg_value>" ac-86
tool-set-mode ::= ("" "set_mode" "\n") space (space (tool-set-mode-arg-level))* 
tool-set-mode-arg-level ::= ("<arg_key>" "level" "</arg_key>\n") "<arg_value>" ac-129
until-173 ::= | [<] until-173-01 | [^<] until-173
until-173-01 ::= | [<] until-173-01 | [t] until-173-02 | [^<t] until-173
until-173-02 ::= | [<] until-173-01 | [o] until-173-03 | [^<o] until-173
until-173-03 ::= | [<] until-173-01 | [o] until-173-04 | [^<o] until-173
until-173-04 ::= | [<] until-173-01 | [l] until-173-05 | [^<l] until-173
until-173-05 ::= | [<] until-173-01 | [_] until-173-06 | [^<_] until-173
until-173-06 ::= | [<] until-173-01 | [c] until-173-07 | [^<c] until-173
until-173-07 ::= | [<] until-173-01 | [a] until-173-08 | [^<a] until-173
until-173-08 ::= | [<] until-173-01 | [l] until-173-09 | [^<l] until-173
until-173-09 ::= | [<] until-173-01 | [l] until-173-10 | [^<l] until-173
until-173-10 ::= | [<] until-173-01 | [^<>] until-173
)XH");
    gcheck("h3", H3(), R"XH(ac-129 ::= [<] ac-129-01 | [^<] ac-129
ac-129-01 ::= [<] ac-129-01 | [/] ac-129-02 | [^/<] ac-129
ac-129-02 ::= [<] ac-129-01 | [p] ac-129-03 | [^<p] ac-129
ac-129-03 ::= [<] ac-129-01 | [a] ac-129-04 | [^<a] ac-129
ac-129-04 ::= [<] ac-129-01 | [r] ac-129-05 | [^<r] ac-129
ac-129-05 ::= [<] ac-129-01 | [a] ac-129-06 | [^<a] ac-129
ac-129-06 ::= [<] ac-129-01 | [m] ac-129-07 | [^<m] ac-129
ac-129-07 ::= [<] ac-129-01 | [e] ac-129-08 | [^<e] ac-129
ac-129-08 ::= [<] ac-129-01 | [t] ac-129-09 | [^<t] ac-129
ac-129-09 ::= [<] ac-129-01 | [e] ac-129-10 | [^<e] ac-129
ac-129-10 ::= [<] ac-129-01 | [r] ac-129-11 | [^<r] ac-129
ac-129-11 ::= [<] ac-129-01 | [>] ac-129-12 | [^<>] ac-129
ac-129-12 ::= [\n] | [<] ac-129-01 | [^\n<] ac-129
ac-86 ::= [<] ac-86-01 | [^<] ac-86
ac-86-01 ::= [<] ac-86-01 | [/] ac-86-02 | [^/<] ac-86
ac-86-02 ::= [<] ac-86-01 | [p] ac-86-03 | [^<p] ac-86
ac-86-03 ::= [<] ac-86-01 | [a] ac-86-04 | [^<a] ac-86
ac-86-04 ::= [<] ac-86-01 | [r] ac-86-05 | [^<r] ac-86
ac-86-05 ::= [<] ac-86-01 | [a] ac-86-06 | [^<a] ac-86
ac-86-06 ::= [<] ac-86-01 | [m] ac-86-07 | [^<m] ac-86
ac-86-07 ::= [<] ac-86-01 | [e] ac-86-08 | [^<e] ac-86
ac-86-08 ::= [<] ac-86-01 | [t] ac-86-09 | [^<t] ac-86
ac-86-09 ::= [<] ac-86-01 | [e] ac-86-10 | [^<e] ac-86
ac-86-10 ::= [<] ac-86-01 | [r] ac-86-11 | [^<r] ac-86
ac-86-11 ::= [<] ac-86-01 | [>] ac-86-12 | [^<>] ac-86
ac-86-12 ::= [\n] | [<] ac-86-01 | [^\n<] ac-86
root ::= "]~b]ai\n" space until-171? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "<minimax:tool_call>\n" space (tool-echo | tool-set-mode) (", " (tool-echo | tool-set-mode))* space "</minimax:tool_call>"
tool-echo ::= ("<invoke name=\"" "echo" "\">\n") space tool-echo-arg-text space "</invoke>\n"
tool-echo-arg-text ::= ("<parameter name=\"" "text" "") "\">" ac-86
tool-set-mode ::= ("<invoke name=\"" "set_mode" "\">\n") space (space (tool-set-mode-arg-level))* space "</invoke>\n"
tool-set-mode-arg-level ::= ("<parameter name=\"" "level" "") "\">" ac-129
until-171 ::= | [<] until-171-01 | [^<] until-171
until-171-01 ::= | [<] until-171-01 | [m] until-171-02 | [^<m] until-171
until-171-02 ::= | [<] until-171-01 | [i] until-171-03 | [^<i] until-171
until-171-03 ::= | [<] until-171-01 | [n] until-171-04 | [^<n] until-171
until-171-04 ::= | [<] until-171-01 | [i] until-171-05 | [^<i] until-171
until-171-05 ::= | [<] until-171-01 | [m] until-171-06 | [^<m] until-171
until-171-06 ::= | [<] until-171-01 | [a] until-171-07 | [^<a] until-171
until-171-07 ::= | [<] until-171-01 | [x] until-171-08 | [^<x] until-171
until-171-08 ::= | [<] until-171-01 | [:] until-171-09 | [^:<] until-171
until-171-09 ::= | [<] until-171-01 | [t] until-171-10 | [^<t] until-171
until-171-10 ::= | [<] until-171-01 | [o] until-171-11 | [^<o] until-171
until-171-11 ::= | [<] until-171-01 | [o] until-171-12 | [^<o] until-171
until-171-12 ::= | [<] until-171-01 | [l] until-171-13 | [^<l] until-171
until-171-13 ::= | [<] until-171-01 | [_] until-171-14 | [^<_] until-171
until-171-14 ::= | [<] until-171-01 | [c] until-171-15 | [^<c] until-171
until-171-15 ::= | [<] until-171-01 | [a] until-171-16 | [^<a] until-171
until-171-16 ::= | [<] until-171-01 | [l] until-171-17 | [^<l] until-171
until-171-17 ::= | [<] until-171-01 | [l] until-171-18 | [^<l] until-171
until-171-18 ::= | [<] until-171-01 | [>] until-171-19 | [^<>] until-171
until-171-19 ::= | [<] until-171-01 | [^\n<] until-171
)XH");
    gcheck("h4", H4(), R"XH(ac-135 ::= [<] ac-135-01 | [^<] ac-135
ac-135-01 ::= [<] ac-135-01 | [/] ac-135-02 | [^/<] ac-135
ac-135-02 ::= [<] ac-135-01 | [a] ac-135-03 | [^<a] ac-135
ac-135-03 ::= [<] ac-135-01 | [r] ac-135-04 | [^<r] ac-135
ac-135-04 ::= [<] ac-135-01 | [g] ac-135-05 | [^<g] ac-135
ac-135-05 ::= [<] ac-135-01 | [_] ac-135-06 | [^<_] ac-135
ac-135-06 ::= [<] ac-135-01 | [v] ac-135-07 | [^<v] ac-135
ac-135-07 ::= [<] ac-135-01 | [a] ac-135-08 | [^<a] ac-135
ac-135-08 ::= [<] ac-135-01 | [l] ac-135-09 | [^<l] ac-135
ac-135-09 ::= [<] ac-135-01 | [u] ac-135-10 | [^<u] ac-135
ac-135-10 ::= [<] ac-135-01 | [e] ac-135-11 | [^<e] ac-135
ac-135-11 ::= [>] | [<] ac-135-01 | [^<>] ac-135
ac-86 ::= [<] ac-86-01 | [^<] ac-86
ac-86-01 ::= [<] ac-86-01 | [/] ac-86-02 | [^/<] ac-86
ac-86-02 ::= [<] ac-86-01 | [a] ac-86-03 | [^<a] ac-86
ac-86-03 ::= [<] ac-86-01 | [r] ac-86-04 | [^<r] ac-86
ac-86-04 ::= [<] ac-86-01 | [g] ac-86-05 | [^<g] ac-86
ac-86-05 ::= [<] ac-86-01 | [_] ac-86-06 | [^<_] ac-86
ac-86-06 ::= [<] ac-86-01 | [v] ac-86-07 | [^<v] ac-86
ac-86-07 ::= [<] ac-86-01 | [a] ac-86-08 | [^<a] ac-86
ac-86-08 ::= [<] ac-86-01 | [l] ac-86-09 | [^<l] ac-86
ac-86-09 ::= [<] ac-86-01 | [u] ac-86-10 | [^<u] ac-86
ac-86-10 ::= [<] ac-86-01 | [e] ac-86-11 | [^<e] ac-86
ac-86-11 ::= [>] | [<] ac-86-01 | [^<>] ac-86
root ::= "<assistant>" space until-184? tool-call
space ::= | " " | "\n"{1,2} [ \t]{0,20}
tool-call ::= "<tool_call>" space (tool-echo | tool-set-mode) space "</tool_call>" (space "<tool_call>" space (tool-echo | tool-set-mode) space "</tool_call>")* space
tool-echo ::= (("" "echo" "") space) tool-echo-arg-text 
tool-echo-arg-text ::= ("<arg_key>" "text" "</arg_key>") "<arg_value>" ac-86
tool-set-mode ::= ("" "set_mode" "") space (space (tool-set-mode-arg-level))* 
tool-set-mode-arg-level ::= ("<arg_key>" "level" "</arg_key>") "<arg_value>" ac-135
until-184 ::= | [<] until-184-01 | [^<] until-184
until-184-01 ::= | [<] until-184-01 | [t] until-184-02 | [^<t] until-184
until-184-02 ::= | [<] until-184-01 | [o] until-184-03 | [^<o] until-184
until-184-03 ::= | [<] until-184-01 | [o] until-184-04 | [^<o] until-184
until-184-04 ::= | [<] until-184-01 | [l] until-184-05 | [^<l] until-184
until-184-05 ::= | [<] until-184-01 | [_] until-184-06 | [^<_] until-184
until-184-06 ::= | [<] until-184-01 | [c] until-184-07 | [^<c] until-184
until-184-07 ::= | [<] until-184-01 | [a] until-184-08 | [^<a] until-184
until-184-08 ::= | [<] until-184-01 | [l] until-184-09 | [^<l] until-184
until-184-09 ::= | [<] until-184-01 | [l] until-184-10 | [^<l] until-184
until-184-10 ::= | [<] until-184-01 | [^<>] until-184
)XH");
 printf("FG_HOIST cases=%d failures=%d\n",gc,gf);return gf==0?0:1;}
