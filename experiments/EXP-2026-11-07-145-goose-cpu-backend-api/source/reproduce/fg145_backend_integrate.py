from pathlib import Path
import shutil
p=Path('/tmp/fg143_integrate.py');s=p.read_text();assert "run_cmd('backend-goose'" not in s;shutil.copy2(p,'/tmp/fg143_integrate_before_backend_api.py')
marker="newlink=[];found=set();renames=[]";assert s.count(marker)==1
addition="""for backend_label in ['backend-goose','backend-cpp']:
 backend_recipe=json.loads((build/(backend_label+'-command.json')).read_text());run_cmd(backend_label,backend_recipe['argv'],backend_recipe['cwd'])
"""
s=s.replace(marker,addition+marker)
marker=" elif arg.endswith('.o'):";assert s.count(marker)==1;s=s.replace(marker," elif arg.endswith('/ggml-cpu/ggml-cpu.cpp.o'):arg=str(build/'backend-cpp.o')\n"+marker)
marker="str(build/'stateful.o'),str(build/'control.o')]";assert s.count(marker)==1;s=s.replace(marker,"str(build/'stateful.o'),str(build/'control.o'),str(build/'backend-goose.o')]")
p.write_text(s)
