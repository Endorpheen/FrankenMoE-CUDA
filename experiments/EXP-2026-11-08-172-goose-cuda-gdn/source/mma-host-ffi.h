#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t fg174m_query(int64_t,int64_t);
int64_t fg174m_cc(int64_t);
int64_t fg174m_memo_get(int64_t,int64_t);
void fg174m_memo_set(int64_t,int64_t,int64_t);
void fg174m_lock(int64_t,int64_t);
void fg174m_unlock(int64_t,int64_t);
int64_t fg174m_init(int64_t,int64_t,uint64_t);
void fg174m_clear_error(void);
int64_t fg174m_launch_raw(int64_t,int64_t,uint64_t,uint64_t);
void fg174m_fail(int64_t);
#ifdef __cplusplus
}
#endif
