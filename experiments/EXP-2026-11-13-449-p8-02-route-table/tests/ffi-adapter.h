#pragma once
#include "routes-gen.h"

static int64_t fg449_route_id(int64_t m,const char *p,int64_t n){
    sl_u8 s; s.data=(uint8_t*)(uintptr_t)p; s.len=n;
    return route_id(m,s);
}
static int64_t fg449_requires_key(int64_t a){ return requires_key(a); }
