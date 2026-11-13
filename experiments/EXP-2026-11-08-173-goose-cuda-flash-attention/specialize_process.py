#!/usr/bin/env python3
# EXP168 Apex: specialize process_tile for ncols1=4/8, np=2/1; unroll static loops.
import re
APEX = ((4, 2), (8, 1)); MAX_ITERS = 4096; EXPECTED_STATIC_LOOPS = 14
ASN = ('=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>='); CMP = ('==', '!=', '<=', '>=', '<', '>')
TOK = re.compile(r'//[^\n]*|/\*.*?\*/|"[^"]*"|[A-Za-z_][A-Za-z0-9_]*|[0-9]+(?:\.[0-9]*)?|>>=|<<=|->|::|==|!=|<=|>=|&&|\|\||\+=|-=|\*=|/=|%=|>>|<<|\s+|.', re.S)
class TransformError(RuntimeError): pass
def ident(x): return bool(x) and (x[0].isalpha() or x[0] == '_') and all(c.isalnum() or c == '_' for c in x)
def tdiv(a, b):
    if b == 0: raise ValueError
    q = abs(a) // abs(b); return -q if (a < 0) != (b < 0) else q
def toks(src):
    t, last = [], 0
    for m in TOK.finditer(src):
        if m.start() != last: raise TransformError('tokenizer gap')
        x = m.group(0)
        k = 'ws' if x.isspace() else 'comment' if x.startswith('//') or x.startswith('/*') else 'string' if x[0] == '"' else 'ident' if ident(x) else 'num' if x[0].isdigit() else 'punct'
        t.append((k, x, m.start(), m.end())); last = m.end()
    if last != len(src): raise TransformError('tokenizer gap')
    return t
class Spec:
    def __init__(self, src): self.t = toks(src); self.s = [i for i, x in enumerate(self.t) if x[0] not in ('ws', 'comment')]; self.seen = set()
    def tx(self, i): return self.t[self.s[i]][1]
    def render(self, lo, hi, sub=None): return ''.join(x for _, x, _, _ in self.t[lo:hi]) if sub is None else ''.join(sub.get(x, x) if k == 'ident' else x for k, x, _, _ in self.t[lo:hi])
    def match(self, pos, o, c):
        d = 0
        while pos < len(self.s):
            x = self.tx(pos)
            if x == o: d += 1
            elif x == c:
                d -= 1
                if d == 0: return pos
            pos += 1
        raise TransformError('unbalanced ' + o)
    def block(self, b, env):
        it, br, out, i = b['items'], b['close'], ([self.render(b['open'] + 1, b['items'][0]['lo'])] if b['items'] else []), 0
        while i < len(it):
            nd = it[i]; nxt = it[i + 1]['lo'] if i + 1 < len(it) else br
            if i + 1 < len(it) and it[i + 1]['kind'] == 'while':
                r = self.unroll(nd, it[i + 1], env)
                if r is not None:
                    out.append(r); out.append(self.render(it[i + 1]['end'], it[i + 2]['lo'] if i + 2 < len(it) else br)); i += 2; continue
            out.append(self.node(nd, env)); out.append(self.render(nd['end'], nxt)); self.stepenv(nd, env); i += 1
        return ''.join(out)
    def node(self, nd, env):
        if nd['kind'] == 'simple': return self.render(nd['lo'], nd['hi'])
        out = self.render(nd['kw'], nd['brace'] + 1) + self.block(nd['body'], dict(env)) + self.render(nd['close'], nd['close'] + 1)
        if nd['kind'] == 'if' and nd['else_kw'] is not None:
            out += self.render(nd['close'] + 1, nd['open2'] + 1) + self.block(nd['body2'], dict(env)) + self.render(nd['close2'], nd['close2'] + 1)
        return out
    def pblock(self, pos):
        start, items = pos - 1, []
        while True:
            if pos >= len(self.s): raise TransformError('unterminated block')
            if self.tx(pos) == '}': return {'items': items, 'close': self.s[pos], 'open': self.s[start], 'start_sig': start, 'end_sig': pos}, pos
            nd, pos = self.pstmt(pos); items.append(nd)
    def pstmt(self, pos):
        x = self.tx(pos)
        if x in ('while', 'if'):
            q = pos + 1
            while q < len(self.s) and self.tx(q) != '{': q += 1
            if q >= len(self.s): raise TransformError('expected {')
            b, e = self.pblock(q + 1)
            nd = {'kind': x, 'lo': self.s[pos], 'kw': self.s[pos], 'brace': self.s[q], 'cond': (pos + 1, q), 'body': b, 'close': self.s[e], 'else_kw': None, 'end': self.s[e] + 1}
            if x == 'while': return nd, e + 1
            n = e + 1
            if n < len(self.s) and self.tx(n) == 'else':
                if n + 1 >= len(self.s) or self.tx(n + 1) != '{': raise TransformError('unsupported else')
                nd['else_kw'] = self.s[n]; nd['open2'] = self.s[n + 1]
                b2, e2 = self.pblock(n + 2); nd['body2'] = b2; nd['close2'] = self.s[e2]; nd['end'] = self.s[e2] + 1
                return nd, e2 + 1
            return nd, n
        d = 0
        for q in range(pos, len(self.s)):
            y = self.tx(q)
            if y in ('(', '[', '{'): d += 1
            elif y in (')', ']', '}'): d -= 1
            elif y == ';' and d == 0: return {'kind': 'simple', 'lo': self.s[pos], 'hi': self.s[q] + 1, 'sig': (pos, q + 1), 'end': self.s[q] + 1}, q + 1
        raise TransformError('unterminated statement')
    def decl(self, nd):
        if nd['kind'] != 'simple': return None
        ts = [self.tx(i) for i in range(*nd['sig'])]
        if len(ts) < 4 or ts[0] not in ('let', 'var') or not ident(ts[1]): return None
        eq = self.at(ts, '=')
        return None if eq is None else (ts[1], ts[3] if ts[2] == ':' else None, ts[eq + 1:-1] if ts[-1] == ';' else ts[eq + 1:])
    def at(self, ts, op):
        d = 0
        for i, x in enumerate(ts):
            if x in ('(', '['): d += 1
            elif x in (')', ']'): d -= 1
            elif x == op and d == 0: return i
        return None
    def stepenv(self, nd, env):
        if nd['kind'] != 'simple': return
        ts = [self.tx(i) for i in range(*nd['sig'])]
        if len(ts) > 1 and ts[1] in ASN: env.pop(ts[0], None); return
        d = self.decl(nd)
        if d and d[1] in (None, 'i32'):
            v = self.ev(d[2], env)
            if v is not None: env[d[0]] = v
    def ev(self, ts, env):
        ts = [x for i, x in enumerate(ts) if x != 'as' and not (i and ts[i - 1] == 'as')]
        p = [0]
        def cur(): return ts[p[0]] if p[0] < len(ts) else None
        def atom():
            x = cur()
            if x == '-': p[0] += 1; return -atom()
            if x == '(':
                p[0] += 1; v = shift()
                if cur() != ')': raise ValueError
                p[0] += 1; return v
            if ident(x):
                v = env.get(x)
                if v is None: raise ValueError
                p[0] += 1; return v
            v = int(x, 0); p[0] += 1; return v
        def mul():
            v = atom()
            while cur() in ('*', '/', '%'):
                op = cur(); p[0] += 1; w = atom()
                v = v * w if op == '*' else (tdiv(v, w) if op == '/' else v - tdiv(v, w) * w)
            return v
        def add():
            v = mul()
            while cur() in ('+', '-'):
                op = cur(); p[0] += 1; w = mul(); v = v + w if op == '+' else v - w
            return v
        def shift():
            v = add()
            while cur() in ('>>', '<<'):
                op = cur(); p[0] += 1; w = add()
                if w < 0 or w > 63: raise ValueError
                v = v >> w if op == '>>' else v << w
            return v
        try:
            v = shift()
            return v if p[0] == len(ts) else None
        except (ValueError, TypeError, ZeroDivisionError): return None
    def cond(self, ts, var, val, env):
        d = 0
        for i, x in enumerate(ts):
            if x == '(': d += 1
            elif x == ')': d -= 1
            elif x in CMP and d == 0:
                l, r = ts[:i], ts[i + 1:]
                if l == [var]: b = self.ev(r, env); return None if b is None else (x, b, False)
                if r == [var]: b = self.ev(l, env); return None if b is None else (x, b, True)
                return None
        return None
    def holds(self, op, val, b, rev):
        a, c = (b, val) if rev else (val, b)
        return {'<': a < c, '>': a > c, '<=': a <= c, '>=': a >= c, '==': a == c, '!=': a != c}[op]
    def step(self, nd, var, val, env):
        if nd['kind'] != 'simple': return None
        ts = [self.tx(i) for i in range(*nd['sig'])]
        if len(ts) < 3 or ts[0] != var: return None
        e = ts[2:-1] if ts[-1] == ';' else ts[2:]
        if ts[1] in ('+=', '-=', '*=', '/=', '%='):
            v = self.ev(e, env)
            if v is None: return None
            return {'+=': lambda a, b: a + b, '-=': lambda a, b: a - b, '*=': lambda a, b: a * b,
                    '/=': lambda a, b: tdiv(a, b) if b else None,
                    '%=': lambda a, b: a - tdiv(a, b) * b if b else None}[ts[1]](val, v)
        if ts[1] == '=':
            env2 = dict(env); env2[var] = val
            return self.ev(e, env2)
        return None
    def check(self, b, var, st):
        for i in range(b['start_sig'] + 1, b['end_sig']):
            x = self.tx(i)
            if x in ('break', 'continue', 'return'): raise TransformError('control flow in static loop')
            if x in ('let', 'var') and i + 1 < b['end_sig'] and self.tx(i + 1) == var: raise TransformError('induction var redeclared')
            if x == var and i + 1 < b['end_sig'] and self.tx(i + 1) in ASN and i != st['sig'][0]: raise TransformError('unexpected write to ' + var)
    def unroll(self, init, wh, env):
        if init['kind'] != 'simple': return None
        d = self.decl(init)
        if d:
            if self.tx(init['sig'][0]) != 'var': return None
            var, typ, expr = d
            if typ != 'i32': return None
            v0 = self.ev(expr, env)
        else:
            ts = [self.tx(i) for i in range(*init['sig'])]
            if len(ts) < 4 or not ident(ts[0]) or ts[1] != '=': return None
            var = ts[0]; v0 = self.ev(ts[2:-1] if ts[-1] == ';' else ts[2:], env)
        if v0 is None: return None
        ct = [self.tx(i) for i in range(*wh['cond'])]
        if self.cond(ct, var, v0, env) is None: return None
        it = wh['body']['items']
        if not it: raise TransformError('empty static loop')
        st = it[-1]
        if self.step(st, var, v0, env) is None: raise TransformError('unrecognised loop step ' + var)
        self.check(wh['body'], var, st)
        vals, val = [], v0
        while True:
            c = self.cond(ct, var, val, env)
            if c is None: raise TransformError('dynamic loop bound ' + var)
            if not self.holds(c[0], val, c[1], c[2]): break
            if len(vals) >= MAX_ITERS: raise TransformError('unroll limit ' + var)
            vals.append(val); nv = self.step(st, var, val, env)
            if nv is None: raise TransformError('loop step eval ' + var)
            val = nv
        self.seen.add(wh['kw'])
        out = [self.render(init['lo'], init['hi']), self.render(init['end'], wh['lo'])]
        reduced = {'items': it[:-1], 'close': st['lo'], 'open': wh['body']['open']}
        for v in vals:
            e2 = dict(env); e2[var] = v
            out.append('if true{\nlet %s:i32=(%d as i32);\n' % (var, v)); out.append(self.block(reduced, e2)); out.append('}\n')
        out.append(self.render(st['end'], wh['body']['close'])); out.append('%s=(%d as i32);\n' % (var, val)); env[var] = val
        return ''.join(out)
def transform(source):
    p = Spec(source); found = None; depth = 0
    for i in range(len(p.s)):
        x = p.tx(i)
        if x == '{': depth += 1
        elif x == '}': depth -= 1
        elif depth == 0 and x == 'fn' and i + 1 < len(p.s) and p.tx(i + 1) == 'process_tile': found = i; break
    if found is None: raise TransformError('fn process_tile not found')
    i = found + 2
    if i >= len(p.s) or p.tx(i) != '(': raise TransformError('bad signature')
    rp = p.match(i, '(', ')')
    i = rp + 1
    while i < len(p.s) and p.tx(i) != '{': i += 1
    if i >= len(p.s): raise TransformError('body missing')
    ob = i; e = p.match(ob, '{', '}')
    body, end = p.pblock(ob + 1)
    if end != e: raise TransformError('body parse mismatch')
    ft, nt, bt = p.s[found], p.s[found + 1], p.s[ob]; pre, post = p.render(ft, nt), p.render(nt + 1, bt + 1); params = p.render(p.s[found + 2] + 1, p.s[rp])
    names = [z.split(':')[0].strip() for z in params.split(',')]
    if not names or any(not ident(z) for z in names) or 'ncols1' not in names or 'np' not in names: raise TransformError('bad process_tile parameters')
    args = ','.join(names); fallback = p.render(body['open'] + 1, body['close']); out = []
    for nc, npv in APEX:
        p.seen = set(); specpost = p.render(nt + 1, bt + 1, {'ncols1': 'ncols1_abi', 'np': 'np_abi'})
        out.append(pre + 'process_tile_apex_nc%d_np%d' % (nc, npv) + specpost + 'let ncols1:i32=(%d as i32);\nlet np:i32=(%d as i32);\n' % (nc, npv) + p.block(body, {'ncols1': nc, 'np': npv}) + '}')
        if len(p.seen) != EXPECTED_STATIC_LOOPS: raise TransformError('unexpected static loop count: %d' % len(p.seen))
    c1, c2 = '((ncols1==(4 as i32))&&(np==(2 as i32)))', '((ncols1==(8 as i32))&&(np==(1 as i32)))'
    out.append(pre + 'process_tile' + post + 'if (' + c1 + '){process_tile_apex_nc4_np2(' + args + ');}else{if (' + c2 + '){process_tile_apex_nc8_np1(' + args + ');}else{' + fallback + '}}}')
    return source[:p.t[ft][2]] + ''.join(out) + source[p.t[p.s[e]][3]:]
