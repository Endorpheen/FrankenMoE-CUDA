// GBNF gate: ten content arenas, compared byte for byte with native build_grammar.
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
 Arena():kind(1024,0),cfirst(1024,0),ccount(1024,0),children(8192,0),sidx(1024,0),dstart(1024,0),dcount(1024,0),mn(1024,0),mx(1024,0),st(128,0),pool(1<<16,0){}};
static std::string content(const std::string&gp){
 Arena a; auto g=vb(gp),e=vb("");
 int64_t root=autoparser_build_content(bu(g),bu(e),bu(e),0,li(a.kind),li(a.cfirst),li(a.ccount),li(a.children),li(a.sidx),li(a.dstart),li(a.dcount),li(a.mn),li(a.mx),bu(a.pool),li(a.st));
 std::vector<int64_t> meta(8,0),vis(1024,0),rids(1024,0),hids(1024,0),roff(1024,0),rlen(1024,0),boff(1024,0),blen(1024,0);
 std::vector<uint8_t> scratch(1<<18,0),out(1<<18,0);
 int64_t len=autoparser_arena_gbnf(root,0,a.st[60],li(a.kind),li(a.cfirst),li(a.ccount),li(a.children),li(a.sidx),li(a.dstart),li(a.dcount),li(a.mn),li(a.mx),bu(a.pool),li(meta),li(vis),li(rids),li(hids),li(roff),li(rlen),li(boff),li(blen),bu(scratch),bu(out));
 if(meta[3])return "<unsupported>";
 return std::string((const char*)out.data(),(size_t)len);
}

static std::string preserved(std::vector<std::string> v) {
    v.resize(21);
    std::vector<uint8_t> out(1<<16,0);
    std::vector<int64_t> off(64,0),len(64,0);
    auto S=[&](const std::string&x){return vb(x);};
    auto a0=S(v[0]),a1=S(v[1]),a2=S(v[2]),a3=S(v[3]),a4=S(v[4]),a5=S(v[5]),a6=S(v[6]),a7=S(v[7]);
    auto a8=S(v[8]),a9=S(v[9]),a10=S(v[10]),a11=S(v[11]),a12=S(v[12]),a13=S(v[13]),a14=S(v[14]);
    auto a15=S(v[15]),a16=S(v[16]),a17=S(v[17]),a18=S(v[18]),a19=S(v[19]),a20=S(v[20]);
    int64_t n=autoparser_collect_preserved(bu(a0),bu(a1),bu(a2),bu(a3),bu(a4),bu(a5),bu(a6),bu(a7),
        bu(a8),bu(a9),bu(a10),bu(a11),bu(a12),bu(a13),bu(a14),bu(a15),bu(a16),bu(a17),bu(a18),bu(a19),bu(a20),
        bu(out),li(off),li(len));
    std::string r;
    for(int64_t i=0;i<n;++i){ if(i) r+=","; r+=std::string((const char*)out.data()+off[i],(size_t)len[i]); }
    return r;
}
static int tc=0,tf=0;
static void tcheck(const char*n,const std::string&got,const std::string&want){++tc;if(got!=want){++tf;printf("FAIL-T %s got=[%s] want=[%s]\n",n,got.c_str(),want.c_str());}else printf("ok-T %s\n",n);}

static int gc=0,gf=0;
static void gcheck(const char*n,const std::string&got,const std::string&want){++gc;if(got!=want){++gf;size_t i=0;while(i<got.size()&&i<want.size()&&got[i]==want[i])++i;printf("FAIL %s diff %zu\n got=[%s]\n want=[%s]\n",n,i,got.substr(i<40?0:i-40,200).c_str(),want.substr(i<40?0:i-40,200).c_str());}else printf("ok %s\n",n);}
int main(){goose_init(0,0);
    gcheck("content_1", content(R"XG()XG"), R"XG(root ::= .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_2", content(R"XG(assistant: )XG"), R"XG(root ::= "assistant: " .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_3", content(R"XG(<|assistant|>)XG"), R"XG(root ::= "<|assistant|>" .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_4", content(R"XG(USER: )XG"), R"XG(root ::= "USER: " .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_5", content(R"XG(A: )XG"), R"XG(root ::= "A: " .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_6", content(R"XG(B: )XG"), R"XG(root ::= "B: " .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_7", content(R"XG(turn: )XG"), R"XG(root ::= "turn: " .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_8", content(R"XG({{)XG"), R"XG(root ::= "{{" .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_9", content(R"XG(```)XG"), R"XG(root ::= "```" .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
    gcheck("content_10", content(R"XG(xyz)XG"), R"XG(root ::= "xyz" .*
space ::= | " " | "\n"{1,2} [ \t]{0,20}
)XG");
     tcheck("glm", preserved({"<think>","</think>\n","","","","","<tool_call>","</tool_call>","","\n","","","","","<arg_key>","</arg_key>\n","","<arg_value>","</arg_value>\n","",""}), "<think>,</think>,<tool_call>,</tool_call>,<arg_key>,</arg_key>,<arg_value>,</arg_value>");
    tcheck("t12", preserved({"","","","","","","<tool_call>","</tool_call>","<function=",">","","</args></function>","","","<param=",">","","","</param>","",""}), "<tool_call>,</tool_call>,<function=,>,</args></function>,<param=,</param>");
    tcheck("callid", preserved({"","","","","","","[TOOL_CALLS]","","","","","","","","","","","","","[CALL_ID]","[ARGS]"}), "[TOOL_CALLS],[CALL_ID],[ARGS]");
        tcheck("glm_stops", std::string(""), std::string(""));
    tcheck("t12_stops", std::string(""), std::string(""));
    tcheck("callid_stops", std::string(""), std::string(""));
printf("FG_TOKENS cases=%d failures=%d\n",tc,tf);
    printf("FG_GBNF cases=%d failures=%d\n",gc,gf);return (gf==0&&tf==0)?0:1;}
