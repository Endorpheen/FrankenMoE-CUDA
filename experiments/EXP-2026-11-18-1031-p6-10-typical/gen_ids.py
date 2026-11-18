import re, sys
src = open('build_chain.c').read()
pat = re.compile(r'static int64_t (gg_function_\d+)\(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, float p5, int64_t p6, float p7, float p8, int64_t base\);')
m = pat.search(src)
if not m:
    sys.exit('dispatch prototype not found')
open('src/fn_ids.h','w').write('#define FG_DISPATCH %s\n' % m.group(1))
print('FG_DISPATCH =', m.group(1))
