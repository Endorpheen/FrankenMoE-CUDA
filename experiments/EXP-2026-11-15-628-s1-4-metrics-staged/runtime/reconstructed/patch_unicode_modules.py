#!/usr/bin/env python3
"""Route the three unicode experiments (EXP215 cpt decode, EXP216 encode, EXP217 whole-string decode)
into the staged unicode.cpp, keeping the EXP218 lowercase route that is already there."""
from pathlib import Path

U = Path(__file__).resolve().parent / 'src/unicode.cpp'
s = U.read_text()

DECLS = '''// Goose unicode routes from EXP215, EXP216 and EXP217; each has its own switch.
extern "C" int fg215u_enabled(void);
extern "C" int64_t fg215u_cpt_from_utf8(const char * buf, int64_t size, int64_t off, int64_t * adv);
extern "C" int fg216e_enabled(void);
extern "C" int64_t fg216e_cpt_to_utf8(int64_t cpt, char * out, int64_t cap);
extern "C" int fg217c_enabled(void);
extern "C" int64_t fg217c_cpts_from_utf8(const char * utf8, int64_t size, int32_t * out);

'''
anchor = '// Goose Unicode lowercase mapping (EXP218).'
assert anchor in s and 'fg215u_enabled' not in s
s = s.replace(anchor, DECLS + anchor, 1)

# EXP215: codepoint decode at a byte offset
old = 'uint32_t unicode_cpt_from_utf8(const std::string & utf8, size_t & offset) {'
idx = s.index(old)
brace = s.index('{', idx)
body_start = brace + 1
route215 = '''
    if (fg215u_enabled()) {
        // Goose decodes and validates; the caller advances by the length it returns, as native does.
        int64_t adv = 0;
        const int64_t cp = fg215u_cpt_from_utf8(utf8.data(), (int64_t) utf8.size(), (int64_t) offset, &adv);
        offset += (size_t) adv;
        return (uint32_t) cp;
    }
'''
s = s[:body_start] + route215 + s[body_start:]

# EXP216: codepoint encode
old16 = 'std::string unicode_cpt_to_utf8(uint32_t cpt) {'
i16 = s.index(old16)
b16 = s.index('{', i16)
route216 = '''
    if (fg216e_enabled()) {
        // Goose writes the bytes; the host owns the string. Zero length is native's empty result for a
        // codepoint above U+10FFFF.
        char buf[4];
        const int64_t n = fg216e_cpt_to_utf8((int64_t) cpt, buf, 4);
        return std::string(buf, (size_t) n);
    }
'''
s = s[:b16 + 1] + route216 + s[b16 + 1:]

# EXP217: whole-string decode
old17 = 'std::vector<uint32_t> unicode_cpts_from_utf8(const std::string & utf8) {'
i17 = s.index(old17)
b17 = s.index('{', i17)
route217 = '''
    if (fg217c_enabled()) {
        // Goose decodes and recovers inline: one U+FFFD per invalid byte, advancing one byte at a time,
        // so the result never exceeds utf8.size(). No exception escapes, matching native.
        if (utf8.empty()) return {};
        std::vector<int32_t> tmp(utf8.size());
        const int64_t n = fg217c_cpts_from_utf8(utf8.data(), (int64_t) utf8.size(), tmp.data());
        return std::vector<uint32_t>(tmp.begin(), tmp.begin() + (size_t) n);
    }
'''
s = s[:b17 + 1] + route217 + s[b17 + 1:]

U.write_text(s)
print('unicode routes applied')
