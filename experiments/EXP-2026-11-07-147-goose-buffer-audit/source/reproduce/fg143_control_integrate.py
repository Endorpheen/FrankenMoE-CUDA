from pathlib import Path
import shutil

p = Path('/tmp/fg143_integrate.py')
s = p.read_text()
assert "run_cmd('control'" not in s
shutil.copy2(p, '/tmp/fg143_integrate_before_control.py')
marker = "pattern=r'(void ggml_compute_forward_mul_mat"
at = s.index(marker)
addition = '''core='#include <stdbool.h>\\nstruct ggml_cgraph;struct ggml_cplan;struct ggml_threadpool;struct ggml_tensor;struct ggml_compute_params;\\n#include "'+str(s/'control/declarations.h')+'"\\n'+core
dispatch='static void ggml_compute_forward(struct ggml_compute_params * params, struct ggml_tensor * tensor) {'
assert core.count(dispatch)==1
core=core.replace(dispatch,dispatch+'\\n    if(fg143_control_is_enabled()){fg143_control_dispatch(params,tensor);return;}',1)
tasks='static int ggml_get_n_tasks(struct ggml_tensor * node, int n_threads) {'
assert core.count(tasks)==1
core=core.replace(tasks,tasks+'\\n    if(fg143_control_is_enabled())return fg143_control_tasks(node,n_threads);',1)
core,n=re.subn(r'(struct ggml_cplan ggml_graph_plan\\(.*?\\)\\s*\\{)',lambda m:m.group(1)+'\\n    if(fg143_control_is_enabled())return fg143_control_plan(cgraph,n_threads,threadpool);',core,count=1,flags=re.S)
assert n==1
core=core.replace(tasks,(s/'control/services.c').read_text()+'\\n'+tasks,1)
'''
s = s[:at]+addition+s[at:]
old = "stateful_cmd=json.loads((build/'stateful-command.json').read_text());run_cmd('stateful',stateful_cmd)"
assert s.count(old) == 1
s = s.replace(old, old+"\ncontrol_cmd=json.loads((build/'control-command.json').read_text());run_cmd('control',control_cmd)")
old = "str(build/'executor.o'),str(build/'stateful.o')]"
assert s.count(old) == 1
s = s.replace(old, "str(build/'executor.o'),str(build/'stateful.o'),str(build/'control.o')]")
p.write_text(s)
