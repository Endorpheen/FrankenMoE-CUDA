#pragma once
#include "lifecycle-gen.h"

static int64_t fg448_on_signal(int64_t a,int64_t b,uint64_t c){ return on_signal(a,b,c); }
static int64_t fg448_shutdown_plan(int64_t a,int64_t b,uint64_t c){ return shutdown_plan(a,b,c); }
static int64_t fg448_exit_code(int64_t a,uint64_t c){ return exit_code(a,c); }
