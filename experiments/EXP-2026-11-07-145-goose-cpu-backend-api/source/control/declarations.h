extern bool fg143_control_is_enabled(void);
extern void fg143_control_dispatch(struct ggml_compute_params*,struct ggml_tensor*);
extern int fg143_control_tasks(struct ggml_tensor*,int);
extern struct ggml_cplan fg143_control_plan(const struct ggml_cgraph*,int,struct ggml_threadpool*);
