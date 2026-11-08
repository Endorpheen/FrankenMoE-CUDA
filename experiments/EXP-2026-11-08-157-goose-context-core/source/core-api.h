#ifndef FG157_CORE_API_H
#define FG157_CORE_API_H
#include <stdint.h>
#if defined(__linux__) && !defined(GGML_USE_CPU_HBM)
#define FG157_SUPPORTED_HOST 1
#else
#define FG157_SUPPORTED_HOST 0
#endif
struct fg157_args{uint64_t v[12];};
static int fg157_enabled;
static struct ggml_type_traits fg157_goose_traits[GGML_TYPE_COUNT];
static char fg157_type_names[GGML_TYPE_COUNT][128];
static char fg157_label_bytes[5][GGML_OP_COUNT][128];
static const char *fg157_label_pointers[5][GGML_OP_COUNT];
static inline int64_t fg157_run(int64_t mode,struct fg157_args *args);
#define FG157_CALL(mode,...) fg157_run((mode),&(struct fg157_args){.v={__VA_ARGS__}})
#define FG157_ARG(x) ((uint64_t)(uintptr_t)(x))
static inline double fg157_decode_f64(int64_t v){double f;memcpy(&f,&v,sizeof(f));return f;}
#endif
