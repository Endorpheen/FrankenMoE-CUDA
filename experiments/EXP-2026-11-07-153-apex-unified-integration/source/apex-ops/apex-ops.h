#include <stdint.h>
int fg_apex_ops_enabled(void);
int64_t fg_apex_ops_entry(int64_t,const int64_t*);
int64_t fg_apex_ops_float_bits(float);
int64_t fg_apex_ops_arg(int64_t,int64_t);
int64_t fg_apex_ops_i32(int64_t,int64_t);
void fg_apex_ops_put(int64_t,int64_t,int64_t,int64_t);
int64_t fg_apex_ops_get(int64_t,int64_t,int64_t);
int64_t fg_apex_ops_shape(int64_t,int64_t,int64_t,int64_t,int64_t);
void fg_apex_ops_name(int64_t,int64_t,int64_t,int64_t);
void fg_apex_ops_fail(void);
