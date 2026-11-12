extern "C" __attribute__((visibility("default"))) void fg163_probe_optimize(ggml_backend_t b,ggml_cgraph*g,ggml_backend_graph_optimize_params*p){ggml_backend_graph_optimize(b,g,p);}
extern "C" __attribute__((visibility("default"))) size_t fg164_probe_snapshot(ggml_backend_sched_t s,char*out,size_t cap){
 size_t used=0;auto put=[&](const char*fmt,auto...args){if(used>=cap)return;int n=snprintf(out+used,cap-used,fmt,args...);if(n>0)used+=(size_t)n;};
 put("state %d %d %d %d %d %d %d %d %d\n",s->is_reset,s->is_alloc,s->n_backends,s->n_splits,s->n_copies,s->cur_copy,s->next_copy,s->n_graph_inputs,s->graph_size);
 auto tensor=[&](const char*kind,ggml_tensor*t,int bid){put("%s %s op%d b%d flags%d view%s",kind,t->name,(int)t->op,bid,t->flags,t->view_src?t->view_src->name:"-");for(int k=0;k<GGML_MAX_SRC;k++)if(t->src[k])put(" src%d=%s",k,t->src[k]->name);put("\n");};
 for(int i=0;i<s->n_splits;i++){auto&sp=s->splits[i];put("split %d %d %d %d %d\n",i,sp.backend_id,sp.i_start,sp.i_end,sp.n_inputs);for(int j=0;j<sp.n_inputs;j++)put("input %s\n",sp.inputs[j]->name);}
 for(int i=0;i<s->n_graph_inputs;i++)put("graph_input %s\n",s->graph_inputs[i]->name);
 for(int i=0;i<s->graph.n_nodes;i++)tensor("node",s->graph.nodes[i],s->node_backend_ids[i]);
 for(int i=0;i<s->graph.n_leafs;i++)tensor("leaf",s->graph.leafs[i],s->leaf_backend_ids[i]);
 return used;
}
