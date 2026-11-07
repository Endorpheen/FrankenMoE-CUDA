from pathlib import Path
import json,subprocess,shutil,sys
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');d=A/'source/expert-cache';p=d/'llama-moe-cache.cpp';s=p.read_text()
s=s.replace('q.ids=(const int32_t*)fills.data();q.n_ids=fills.size();','q.fills=&fills;q.n_ids=fills.size();')
s=s.replace('if(field==18){GGML_ASSERT((size_t)index<(r.extra[7]==1?r.n_ids*2:r.n_ids));return r.ids[index];}', '''if(field==18){
  if(r.extra[7]==1){auto&fills=*(const std::vector<moe_cache_slru::fill>*)r.fills;GGML_ASSERT((size_t)(index/2)<fills.size());const auto&fill=fills.at(index/2);return index%2?fill.slot:fill.expert;}
  GGML_ASSERT((size_t)index<r.n_ids);return r.ids[index];}''')
p.write_text(s);(d/'services.cpp').write_text(s[s.index('extern "C" int64_t fg150_cache_get'):])
p=Path('/tmp/fg150_cache_fixture.py');s=p.read_text().replace('q.ids=(const int32_t*)fills.data();q.n_ids=fills.size();','q.fills=&fills;q.n_ids=fills.size();');p.write_text(s)
# Recompile/relink only the changed cache ABI object,not the unaffected CPU/backend.
for mode in ['cpu-backend','sanitized']:
 b=A/'variants'/mode/'build';run=A/'variants'/mode/'run';rows=json.loads((b/'commands.json').read_text())
 for label in ['cache-cpp','link-llama']:
  row=next(x for x in rows if x['label']==label);r=subprocess.run(row['argv'],cwd=row['cwd'],capture_output=True,text=True,timeout=180);(b/(label+'-fill-abi.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr[-1000:];print(mode,label,r.returncode,flush=True)
 for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:shutil.copy2(run/'libllama-control.so.0.7.0',run/name)
print('Typed fill access replaces flat int32 pointer arithmetic across struct subobjects;Goose and CPU sources unchanged')
