// Hangul syllable decomposition: the algorithmic branch of unicode_canonical_decompose.
//
// This part of NFC needs no table - UAX #15 defines it arithmetically:
//   SBase = 0xAC00, LBase = 0x1100, VBase = 0x1161, TBase = 0x11A7
//   LCount = 19, VCount = 21, TCount = 28, NCount = VCount * TCount = 588, SCount = LCount * NCount = 11172
//   SIndex = S - SBase;  L = LBase + SIndex / NCount;  V = VBase + (SIndex % NCount) / TCount;  T = SIndex % TCount
// A syllable with T == 0 decomposes to two codepoints, otherwise to three.
//
// The table-driven half of the same function stays in C++ until the Unicode tables are moved as data; this half is
// proved here by exhaustion over the 11172 syllables, so no boundary can be missed.

extern "hg_st_u32" fn st(buf: u64, byte: i64, v: i64);

const SBASE: i64 = 0xAC00;
const LBASE: i64 = 0x1100;
const VBASE: i64 = 0x1161;
const TBASE: i64 = 0x11A7;
const NCOUNT: i64 = 588;
const TCOUNT: i64 = 28;
const SCOUNT: i64 = 11172;

// Returns how many codepoints were written: 0 when the input is not a Hangul syllable.
export fn hg_decompose(cpt: i64, out: u64) -> i64 {
    if cpt < SBASE || cpt >= SBASE + SCOUNT {
        return 0;
    }
    let index: i64 = cpt - SBASE;
    let l: i64 = LBASE + (index / NCOUNT);
    let v: i64 = VBASE + ((index - ((index / NCOUNT) * NCOUNT)) / TCOUNT);
    let t: i64 = index - ((index / TCOUNT) * TCOUNT);

    st(out, 0 as i64, l);
    st(out, 4 as i64, v);
    if t == (0 as i64) {
        return 2;
    }
    st(out, 8 as i64, TBASE + t);
    return 3;
}
