from pathlib import Path
import subprocess,json,hashlib,re
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';B=F/'runtime/exp173/build';O=B/'codegen-audit';O.mkdir(exist_ok=True)
objects={'goose_after_static_index_fix':B/'cuda-runtime.o','goose_before_static_index_fix':F/'runtime/exp173/run/libggml-cuda-fa-1687c6fe0c20.so.0.27.0'}
rows=[]
for label,obj in objects.items():
 q=subprocess.run(['/usr/local/cuda/bin/cuobjdump','--gpu-architecture','sm_89','--dump-resource-usage',str(obj)],capture_output=True,text=True,check=True);(O/(label+'-resources.txt')).write_text(q.stdout)
 for m in re.finditer(r'Function\s+(\S+):\s*\n([^\n]+)',q.stdout):
  name=m[1]
  if not any(z in name for z in ['fg175v_kernel','fg175m_kernel','fg175v_native_flash_attn_ext_vec','flash_attn_ext_f16ILi256ELi256']):continue
  dem=subprocess.run(['c++filt',name],capture_output=True,text=True,check=True).stdout.strip()
  if not any(x in dem for x in ['fg175v_kernel<','fg175m_kernel<','fg175v_native_flash_attn_ext_vec<256','flash_attn_ext_f16<256, 256']):continue
  item=dict(label=label,object=str(obj),object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),mangled=name,demangled=dem,resources=m[2].strip());rows.append(item)
  print(label,dem[:180],item['resources'],flush=True)
(O/'selected-resources.json').write_text(json.dumps(rows,indent=2)+'\n')
