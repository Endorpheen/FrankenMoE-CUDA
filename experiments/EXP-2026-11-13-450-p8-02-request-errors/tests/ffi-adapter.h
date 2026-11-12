#pragma once
#include "errors-gen.h"

static int64_t fg450_classify(int64_t a,uint64_t b){ return classify_exception(a,b); }
static int64_t fg450_code(int64_t a,uint64_t b){ return error_code(a,b); }
static int64_t fg450_status(int64_t a,int64_t c,uint64_t b){ return status_from_payload(a,c,b); }
