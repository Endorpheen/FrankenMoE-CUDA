#include <stdint.h>
static uint64_t counts[2][32];
void fg153_graph_trace(int family,int64_t mode) {if(family>=0&&family<2&&mode>=0&&mode<32)__atomic_fetch_add(&counts[family][mode],1,__ATOMIC_RELAXED);}
uint64_t fg153_graph_witness(int family,int mode) {return family>=0&&family<2&&mode>=0&&mode<32?__atomic_load_n(&counts[family][mode],__ATOMIC_RELAXED):0;}
