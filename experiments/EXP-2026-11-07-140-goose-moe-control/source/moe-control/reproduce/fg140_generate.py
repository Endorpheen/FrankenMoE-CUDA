from pathlib import Path
import json,hashlib,subprocess,shutil,re
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-control';shutil.copy2('/tmp/fg140_control.goose',s/'kernel.goose');p=r/'compiler/stage3-7ab1a1ca1cdb';assert hashlib.sha256(p.read_bytes()).hexdigest()=='7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846';cmd=[str(p),'-o',str(r/'generated/moe-control.c'),str(s/'kernel.goose')];a=subprocess.run(cmd,cwd=r,capture_output=True,text=True);(s/'emit.log').write_text(a.stdout+a.stderr);(s/'emit-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('emit',a.returncode,a.stdout[-1000:],a.stderr[-2000:],flush=True);a.check_returncode();code=(s/'kernel.goose').read_text();body=code[code.index('struct Plan {')+len('struct Plan {'):code.index('}\nextern')];fields=re.findall(r'(\w+):i64',body);(s/'fields.json').write_text(json.dumps(fields,indent=2)+'\n');(s/'bridge.h').write_text('#pragma once\n#include <stdint.h>\nstruct fg140_plan {'+','.join(['int64_t '+fields[0]]+fields[1:])+';};\nstruct fg140_row {int32_t slot,token;};\nvoid fg140_entry(struct fg140_plan*,int64_t,int64_t*,int64_t,struct fg140_row*,int64_t,const int32_t*,int64_t,const int32_t*,int64_t,const int32_t*,int64_t,uint8_t*,int64_t);\n');print('plan fields',len(fields))

p=s/'bridge.h'
p.open('a').write("""void fg140_quantize(int64_t,int64_t,int64_t,int64_t);
void fg140_reset(int64_t,int64_t,int32_t);
void fg140_barrier(int64_t);
void fg140_ready(int64_t,int64_t);
uint8_t fg140_tiled(int64_t,int64_t,int64_t);
void fg140_chunk(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg140_matrix(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
int32_t fg140_claim(int64_t,int64_t);
void fg140_iq_stats(int64_t,int64_t);
void fg140_witness(int64_t,int64_t,int64_t);
void fg140_set_callbacks(const void*);
""")
