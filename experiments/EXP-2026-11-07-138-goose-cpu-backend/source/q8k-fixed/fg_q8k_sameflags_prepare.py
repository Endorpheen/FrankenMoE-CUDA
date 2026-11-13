from pathlib import Path
import subprocess,json
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/q8k-fixed';text=(r/'source/ggml/src/ggml-quants.c').read_text()
def extract(start):
 a=text.index(start);b=text.index('{',a)+1;i=b;depth=1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[a:i]
code='''#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#define QK_K 256
#define GGML_RESTRICT restrict
#define MIN(a,b) ((a)<(b)?(a):(b))
typedef struct {float d;int8_t qs[256];int16_t bsums[16];} block_q8_K;
_Static_assert(sizeof(block_q8_K)==292,"Q8_K layout");
'''+extract('static inline int nearest_int(')+'\n'+extract('void quantize_row_q8_K_ref(').replace('quantize_row_q8_K_ref','fg_q8k_native_same_flags',1)+'\n';(s/'native-same-flags.c').write_text(code)
bench=(r/'source/q8k-quant/benchmark.c').read_text().replace('extern void fg_selfhost_quant_q8k','extern void fg_q8k_native_same_flags(const float *,void *,int64_t);\nextern void fg_selfhost_quant_q8k',1).replace('if(goose)fg_selfhost_quant_q8k','if(goose==2)fg_q8k_native_same_flags(x,y,n);else if(goose)fg_selfhost_quant_q8k')
bench=bench.replace('double native,goose;','double native,goose;double sameflags=timed(2,n,x,y);').replace('goose_ns\\\":%.9g,','goose_ns\\\":%.9g,\\\"native_same_flags_ns\\\":%.9g,').replace('n,r,native,goose);','n,r,native,goose,sameflags);');(s/'benchmark-same-flags.c').write_text(bench)
(s/'same-flags-commands.json').write_text(json.dumps({'note':'Build and execute only after current model requests complete. Frozen runtime native reference remains unchanged.','flags':['-O3','-march=native','-ffp-contract=off','-fPIC']},indent=2))
