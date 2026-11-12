void fg141_micro(int,void*,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg141_gather(int,const void*,int64_t,const void*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,void*);
int64_t fg141_execute(const struct fg141_plan*,const void*,int64_t,const void*,int64_t,const void*,int64_t,void*,float*,int64_t);
int64_t fg141_workspace_offset(const struct fg141_plan*);
uint8_t fg141_general_enabled(void);
#ifndef FG141_KERNEL_ONLY
void fg141_quantize(int64_t,int64_t,int64_t,int64_t);
void fg141_reset(int64_t,int32_t);
void fg141_barrier(int64_t);
int32_t fg141_claim(int64_t);
#endif
