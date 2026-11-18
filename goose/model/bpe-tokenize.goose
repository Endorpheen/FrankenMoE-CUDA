// GPT-2 style BPE tokenisation over borrowed GGUF spans, in Goose.
//
// Ported from src/llama-vocab.cpp: llm_tokenizer_bpe_session::tokenize and
// add_new_bigram, the token_to_id / bpe_ranks maps built in load_tokens, and
// the special-token order in the loader (cache_special_tokens).
//
// The vocabularies are stored byte-encoded: every byte of the raw text is
// replaced by the codepoint unicode_byte_to_utf8_map assigns to it, so a space
// is the two bytes of U+0120. The pre-tokenizer must therefore run on raw
// codepoints and the encoding is applied per word afterwards; doing it the
// other way round would split on the encoding's bytes.
//
// Two map behaviours are load-bearing and are reproduced exactly:
//   token_to_id[text] = id   - a later duplicate id replaces the earlier one;
//   bpe_ranks.emplace(pair, rank) - a later duplicate merge is IGNORED.
//
// The merge queue is a binary heap ordered by (rank, left index), which is the
// native comparator. A queued entry carries the two symbol lengths it was
// created with; native re-compares the concatenated text, and for adjacent
// symbols that is equivalent, because a merge can only change a symbol's length.
//
// Merges are parsed the way native parses them: the separator is the first
// space at index >= 1, so a merge whose left side is a single leading space
// still splits correctly, and a merge without a space contributes the empty
// pair (which no lookup can match).

const BT_UNKNOWN: i64 = 1;
const BT_UNUSED: i64 = 2;
const BT_NORMAL: i64 = 4;
const BT_CONTROL: i64 = 8;
const BT_USER_DEFINED: i64 = 16;
const BT_BYTE: i64 = 32;
const BT_SPECIAL: i64 = 256;

fn bt_hash(data: const u8[:], off: i64, n: i64) -> u64 {
    var h: u64 = 14695981039346656037;
    var i: i64 = 0;
    while i < n {
        h = (h ^ (data[off + i])) * 1099511628211;
        i = i + 1;
    }
    return h;
}

fn bt_pair_hash(left: u64, right: u64) -> u64 {
    return (left * 14029467366897019727) ^ (right * 2654435761) ^ (right >> 29);
}

fn bt_eq2(a: const u8[:], ao: i64, an: i64, b: const u8[:], bo: i64, bn: i64) -> bool {
    if an != bn || ao < 0 || bo < 0 || an < 0 {
        return false;
    }
    if ao + an > a.len || bo + bn > b.len {
        return false;
    }
    var i: i64 = 0;
    while i < an {
        if a[ao + i] != b[bo + i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn bt_slot_ok(slots: const i64[:], n: i64) -> bool {
    return slots.len > 0 && (slots.len & (slots.len - 1)) == 0 && n >= 0 && n <= slots.len / 2;
}

// tokenizer.ggml.token_type -> llama_token_attr, then the two loader overrides
// that matter to this tokenizer: an unknown type is UNDEFINED (0) and a
// control-looking EOG text is forced to CONTROL by the EOG pass, which is done
// in bt_special_order and the detokeniser instead of here.
fn bt_attr(tt: i64) -> i64 {
    if tt == 1 {
        return BT_NORMAL;
    }
    if tt == 2 {
        return BT_UNKNOWN;
    }
    if tt == 3 {
        return BT_CONTROL;
    }
    if tt == 4 {
        return BT_USER_DEFINED;
    }
    if tt == 5 {
        return BT_UNUSED;
    }
    if tt == 6 {
        return BT_BYTE;
    }
    return 0;
}

// Builds the token table (last wins) and the merge table (first wins).
// stats: 0 tokens, 1 merges, 2 duplicate tokens replaced, 3 duplicate merges ignored.
export fn bt_build(
    gguf: const u8[:],
    tok_off: const i64[:], tok_len: const i64[:], tok_slots: i64[:],
    mrg_off: const i64[:], mrg_len: const i64[:], mrg_slots: i64[:],
    stats: i64[:]
) -> i64 {
    return bt_build_impl(gguf, tok_off, tok_len, tok_slots, mrg_off, mrg_len, mrg_slots, stats);
}

// Same decision for callers inside the unit: an export keeps one concrete
// signature, so internal callers use this body directly.
fn bt_build_impl(
    gguf: const u8[:],
    tok_off: const i64[:], tok_len: const i64[:], tok_slots: i64[:],
    mrg_off: const i64[:], mrg_len: const i64[:], mrg_slots: i64[:],
    stats: i64[:]
) -> i64 {
    let n_tok: i64 = tok_off.len;
    let n_mrg: i64 = mrg_off.len;
    if tok_len.len != n_tok || mrg_len.len != n_mrg || stats.len < 4 {
        return -1;
    }
    if !bt_slot_ok(tok_slots, n_tok) || !bt_slot_ok(mrg_slots, n_mrg) {
        return -1;
    }
    var i: i64 = 0;
    while i < tok_slots.len {
        tok_slots[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < mrg_slots.len {
        mrg_slots[i] = 0;
        i = i + 1;
    }
    stats[0] = 0;
    stats[1] = 0;
    stats[2] = 0;
    stats[3] = 0;

    i = 0;
    while i < n_tok {
        let off: i64 = tok_off[i];
        let len: i64 = tok_len[i];
        if len < 0 || off < 0 || off + len > gguf.len {
            return -2;
        }
        if len > 0 {
            let h: u64 = bt_hash(gguf, off, len);
            var slot: i64 = (h & ((tok_slots.len - 1) as u64)) as i64;
            var visited: i64 = 0;
            var placed: bool = false;
            while visited < tok_slots.len && !placed {
                let entry: i64 = tok_slots[slot];
                if entry == 0 {
                    tok_slots[slot] = i + 1;
                    placed = true;
                } else if bt_eq2(gguf, tok_off[entry - 1], tok_len[entry - 1], gguf, off, len) {
                    // last duplicate wins, exactly as token_to_id[word] = i
                    tok_slots[slot] = i + 1;
                    stats[2] = stats[2] + 1;
                    placed = true;
                } else {
                    slot = (slot + 1) & (tok_slots.len - 1);
                    visited = visited + 1;
                }
            }
            if !placed {
                return -3;
            }
            stats[0] = stats[0] + 1;
        }
        i = i + 1;
    }

    i = 0;
    while i < n_mrg {
        let off: i64 = mrg_off[i];
        let len: i64 = mrg_len[i];
        if len < 0 || off < 0 || off + len > gguf.len {
            return -2;
        }
        // first space at index >= 1, as strchr(word + 1, ' ')
        var sep: i64 = -1;
        var k: i64 = 1;
        while k < len {
            if gguf[off + k] == 32 {
                sep = k;
                break;
            }
            k = k + 1;
        }
        if sep < 0 {
            // first and second are both empty: a real native key that no lookup reaches
            i = i + 1;
            continue;
        }
        let h: u64 = bt_pair_hash(bt_hash(gguf, off, sep), bt_hash(gguf, off + sep + 1, len - sep - 1));
        var slot: i64 = (h & ((mrg_slots.len - 1) as u64)) as i64;
        var visited: i64 = 0;
        var placed: bool = false;
        while visited < mrg_slots.len && !placed {
            let entry: i64 = mrg_slots[slot];
            if entry == 0 {
                mrg_slots[slot] = i + 1;
                placed = true;
            } else {
                let prev: i64 = entry - 1;
                var psep: i64 = -1;
                var p: i64 = 1;
                while p < mrg_len[prev] {
                    if gguf[mrg_off[prev] + p] == 32 {
                        psep = p;
                        break;
                    }
                    p = p + 1;
                }
                let same: bool = (psep > 0
                    && bt_eq2(gguf, mrg_off[prev], psep, gguf, off, sep)
                    && bt_eq2(gguf, mrg_off[prev] + psep + 1, mrg_len[prev] - psep - 1,
                              gguf, off + sep + 1, len - sep - 1));
                if same {
                    // emplace: the first duplicate keeps its rank
                    stats[3] = stats[3] + 1;
                    placed = true;
                } else {
                    slot = (slot + 1) & (mrg_slots.len - 1);
                    visited = visited + 1;
                }
            }
        }
        if !placed {
            return -3;
        }
        stats[1] = stats[1] + 1;
        i = i + 1;
    }
    return 0;
}

// Token id for a byte span, -1 when the text is not a token.
fn bt_token_id(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
               tok_slots: const i64[:], key: const u8[:], off: i64, len: i64) -> i64 {
    if len <= 0 || tok_slots.len == 0 {
        return -1;
    }
    let h: u64 = bt_hash(key, off, len);
    var slot: i64 = (h & ((tok_slots.len - 1) as u64)) as i64;
    var visited: i64 = 0;
    while visited < tok_slots.len {
        let entry: i64 = tok_slots[slot];
        if entry == 0 {
            return -1;
        }
        let id: i64 = entry - 1;
        if id >= tok_off.len {
            return -1;
        }
        if bt_eq2(gguf, tok_off[id], tok_len[id], key, off, len) {
            return id;
        }
        slot = (slot + 1) & (tok_slots.len - 1);
        visited = visited + 1;
    }
    return -1;
}

// Rank of (left, right), -1 when the pair is not a merge. The stored key lives
// in the GGUF merges array; the query lives in the encoded word buffer.
fn bt_merge_rank(gguf: const u8[:], mrg_off: const i64[:], mrg_len: const i64[:],
                 mrg_slots: const i64[:], enc: const u8[:],
                 lo: i64, ll: i64, ro: i64, rl: i64) -> i64 {
    if ll <= 0 || rl <= 0 || mrg_slots.len == 0 {
        return -1;
    }
    let h: u64 = bt_pair_hash(bt_hash(enc, lo, ll), bt_hash(enc, ro, rl));
    var slot: i64 = (h & ((mrg_slots.len - 1) as u64)) as i64;
    var visited: i64 = 0;
    while visited < mrg_slots.len {
        let entry: i64 = mrg_slots[slot];
        if entry == 0 {
            return -1;
        }
        let rank: i64 = entry - 1;
        if rank >= mrg_off.len {
            return -1;
        }
        let base: i64 = mrg_off[rank];
        let n: i64 = mrg_len[rank];
        var sep: i64 = -1;
        var k: i64 = 1;
        while k < n {
            if gguf[base + k] == 32 {
                sep = k;
                break;
            }
            k = k + 1;
        }
        if sep > 0
            && bt_eq2(gguf, base, sep, enc, lo, ll)
            && bt_eq2(gguf, base + sep + 1, n - sep - 1, enc, ro, rl) {
            return rank;
        }
        slot = (slot + 1) & (mrg_slots.len - 1);
        visited = visited + 1;
    }
    return -1;
}

// Special-token order: the ids whose attribute is control, user-defined or
// unknown, sorted by text length descending (cache_special_tokens). Equal
// lengths keep id order, which cannot change the result: two specials of the
// same length can only both match where they are identical.
export fn bt_special_order(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
                           attr: const i64[:], order: i64[:]) -> i64 {
    return bt_special_order_impl(gguf, tok_off, tok_len, attr, order);
}

fn bt_special_order_impl(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
                         attr: const i64[:], order: i64[:]) -> i64 {
    if attr.len < tok_off.len || order.len < tok_off.len || tok_len.len < tok_off.len {
        return -1;
    }
    var n: i64 = 0;
    var i: i64 = 0;
    while i < tok_off.len {
        let a: i64 = attr[i];
        if (a & (BT_CONTROL | BT_USER_DEFINED | BT_UNKNOWN)) != 0 && tok_len[i] > 0 {
            order[n] = i;
            n = n + 1;
        }
        i = i + 1;
    }
    var a: i64 = 1;
    while a < n {
        let cur: i64 = order[a];
        let len: i64 = tok_len[cur];
        var j: i64 = a;
        while j > 0 && tok_len[order[j - 1]] < len {
            order[j] = order[j - 1];
            j = j - 1;
        }
        order[j] = cur;
        a = a + 1;
    }
    return n;
}

fn bt_lead_len(b: i64) -> i64 {
    let hi: i64 = b / 16;
    if hi <= 11 {
        return 1;
    }
    if hi <= 13 {
        return 2;
    }
    if hi == 14 {
        return 3;
    }
    return 4;
}

fn bt_less(heap: const i64[:], a: i64, b: i64) -> bool {
    if heap[a * 5] != heap[b * 5] {
        return heap[a * 5] < heap[b * 5];
    }
    return heap[a * 5 + 1] < heap[b * 5 + 1];
}

fn bt_swap(heap: i64[:], a: i64, b: i64) {
    var k: i64 = 0;
    while k < 5 {
        let t: i64 = heap[a * 5 + k];
        heap[a * 5 + k] = heap[b * 5 + k];
        heap[b * 5 + k] = t;
        k = k + 1;
    }
}

// hc[0] is the entry count, hc[1] is set to 1 if the heap overflowed.
fn bt_push(heap: i64[:], hc: i64[:], rank: i64, left: i64, right: i64, ln: i64, rn: i64) {
    var i: i64 = hc[0];
    if (i + 1) * 5 > heap.len {
        hc[1] = 1;
        return;
    }
    heap[i * 5] = rank;
    heap[i * 5 + 1] = left;
    heap[i * 5 + 2] = right;
    heap[i * 5 + 3] = ln;
    heap[i * 5 + 4] = rn;
    hc[0] = i + 1;
    while i > 0 {
        let p: i64 = (i - 1) / 2;
        if bt_less(heap, i, p) {
            bt_swap(heap, i, p);
            i = p;
        } else {
            break;
        }
    }
}

fn bt_pop(heap: i64[:], hc: i64[:]) {
    let n: i64 = hc[0] - 1;
    if n < 0 {
        hc[0] = 0;
        return;
    }
    bt_swap(heap, 0, n);
    hc[0] = n;
    var i: i64 = 0;
    while true {
        let l: i64 = i * 2 + 1;
        let r: i64 = l + 1;
        if l >= hc[0] {
            break;
        }
        var m: i64 = l;
        if r < hc[0] && bt_less(heap, r, l) {
            m = r;
        }
        if bt_less(heap, m, i) {
            bt_swap(heap, i, m);
            i = m;
        } else {
            break;
        }
    }
}

// One word: build the symbol chain, run the merge queue, emit ids.
// Returns the number of ids appended, or -1 on output/heap capacity failure.
fn bt_word(
    enc: const u8[:], off: i64, n: i64,
    gguf: const u8[:],
    tok_off: const i64[:], tok_len: const i64[:], tok_slots: const i64[:],
    mrg_off: const i64[:], mrg_len: const i64[:], mrg_slots: const i64[:],
    ids: i64[:], nout: i64, sym: i64[:], heap: i64[:], hc: i64[:]
) -> i64 {
    if n <= 0 {
        return 0;
    }
    if sym.len < n * 4 {
        return -1;
    }
    var count: i64 = 0;
    var at: i64 = off;
    let end: i64 = off + n;
    while at < end {
        var step: i64 = bt_lead_len(enc[at]);
        if step > end - at {
            step = end - at;
        }
        sym[count * 4] = at;
        sym[count * 4 + 1] = step;
        sym[count * 4 + 2] = count - 1;
        sym[count * 4 + 3] = if at + step == end { 0 - 1 } else { count + 1 };
        at = at + step;
        count = count + 1;
    }

    hc[0] = 0;
    hc[1] = 0;
    var i: i64 = 1;
    while i < count {
        let l: i64 = i - 1;
        let rank: i64 = bt_merge_rank(gguf, mrg_off, mrg_len, mrg_slots, enc,
                                      sym[l * 4], sym[l * 4 + 1], sym[i * 4], sym[i * 4 + 1]);
        if rank >= 0 {
            bt_push(heap, hc, rank, l, i, sym[l * 4 + 1], sym[i * 4 + 1]);
        }
        i = i + 1;
    }

    while hc[0] > 0 {
        let l: i64 = heap[1];
        let r: i64 = heap[2];
        let ln: i64 = heap[3];
        let rn: i64 = heap[4];
        bt_pop(heap, hc);
        if sym[l * 4 + 1] == 0 || sym[r * 4 + 1] == 0 {
            continue;
        }
        if sym[l * 4 + 1] != ln || sym[r * 4 + 1] != rn {
            continue;
        }
        // merge the right symbol into the left one
        sym[l * 4 + 1] = ln + rn;
        sym[r * 4 + 1] = 0;
        let rnext: i64 = sym[r * 4 + 3];
        sym[l * 4 + 3] = rnext;
        if rnext >= 0 {
            sym[rnext * 4 + 2] = l;
        }
        // left side of the current symbol
        let lp: i64 = sym[l * 4 + 2];
        if lp >= 0 {
            let lrank: i64 = bt_merge_rank(gguf, mrg_off, mrg_len, mrg_slots, enc,
                                           sym[lp * 4], sym[lp * 4 + 1], sym[l * 4], sym[l * 4 + 1]);
            if lrank >= 0 {
                bt_push(heap, hc, lrank, lp, l, sym[lp * 4 + 1], sym[l * 4 + 1]);
            }
        }
        let next: i64 = sym[l * 4 + 3];
        if next >= 0 {
            let rrank: i64 = bt_merge_rank(gguf, mrg_off, mrg_len, mrg_slots, enc,
                                           sym[l * 4], sym[l * 4 + 1], sym[next * 4], sym[next * 4 + 1]);
            if rrank >= 0 {
                bt_push(heap, hc, rrank, l, next, sym[l * 4 + 1], sym[next * 4 + 1]);
            }
        }
    }

    var total: i64 = 0;
    var out: i64 = nout;
    var s: i64 = 0;
    while s >= 0 {
        let sl: i64 = sym[s * 4 + 1];
        if sl > 0 {
            let so: i64 = sym[s * 4];
            let id: i64 = bt_token_id(gguf, tok_off, tok_len, tok_slots, enc, so, sl);
            if id >= 0 {
                if out >= ids.len {
                    return -1;
                }
                ids[out] = id;
                out = out + 1;
                total = total + 1;
            } else {
                var k: i64 = 0;
                while k < sl {
                    let bid: i64 = bt_token_id(gguf, tok_off, tok_len, tok_slots, enc, so + k, 1);
                    if bid >= 0 {
                        if out >= ids.len {
                            return -1;
                        }
                        ids[out] = bid;
                        out = out + 1;
                        total = total + 1;
                    }
                    k = k + 1;
                }
            }
        }
        s = sym[s * 4 + 3];
    }
    return total;
}

// Attributes for the partition and the detokeniser, in the native loader order:
// token_type, then the FIM text search (which also promotes a token to CONTROL),
// then the "unused" text marker, then the end-of-generation text list, then the
// gpt-oss user-defined override. Attribution order matters because the
// "unused" pass only looks at tokens that are already CONTROL.
//
// The native FIM search walks token_to_id, an unordered_map, so a vocabulary
// that contains two texts from the same FIM list would pick an arbitrary one.
// This port walks ids in ascending order and keeps the first match per class;
// the Apex vocabulary has exactly one text per class, so both agree.

fn bt_eog_text(gguf: const u8[:], off: i64, len: i64) -> bool {
    if len <= 0 || off < 0 || off + len > gguf.len {
        return false;
    }
    let t: const u8[:] = gguf[off .. off + len];
    return t == "<|eot_id|>" || t == "<|eot|>" || t == "<|im_end|>" || t == "<|end|>"
        || t == "<|return|>" || t == "<|call|>" || t == "<|flush|>" || t == "<|calls|>"
        || t == "<end_of_turn>" || t == "<|endoftext|>" || t == "</s>"
        || t == "<|eom_id|>" || t == "<EOT>" || t == "_<EOT>" || t == "[EOT]"
        || t == "[EOS]" || t == "<|end_of_text|>" || t == "<end_of_utterance>"
        || t == "<eos>" || t == "<turn|>" || t == "<|tool_response>"
        || t == "＜end▁of▁sentence｜>"
        || t == "[e~[" || t == "<|ifm|im_end|>";
}

// 1 prefix, 2 suffix, 3 middle, 4 pad, 5 repo, 6 separator, 0 none.
fn bt_fim_class(gguf: const u8[:], off: i64, len: i64) -> i64 {
    if len <= 0 || off < 0 || off + len > gguf.len {
        return 0;
    }
    let t: const u8[:] = gguf[off .. off + len];
    if (t == "<|fim_prefix|>" || t == "<fim-prefix>" || t == "<fim_prefix>" || t == "<PRE>"
        || t == "▁<PRE>" || t == "<|code_prefix|>" || t == "<|prefix|>"
        || t == "<|plamo:fim_prefix|>") {
        return 1;
    }
    if (t == "<|fim_suffix|>" || t == "<fim-suffix>" || t == "<fim_suffix>"
        || t == "＜fim▁hole｜" || t == "<SUF>" || t == "▁<SUF>"
        || t == "<|code_suffix|>" || t == "<|suffix|>" || t == "<|plamo:fim_suffix|>") {
        return 2;
    }
    if (t == "<|fim_middle|>" || t == "<fim-middle>" || t == "<fim_middle>"
        || t == "＜fim▁end｜" || t == "<MID>" || t == "▁<MID>"
        || t == "<|code_middle|>" || t == "<|middle|>" || t == "<|plamo:fim_middle|>") {
        return 3;
    }
    if (t == "<|fim_pad|>" || t == "<fim-pad>" || t == "<fim_pad>" || t == "<PAD>" || t == "[PAD]") {
        return 4;
    }
    if (t == "<|fim_repo|>" || t == "<|repo_name|>" || t == "<fim-repo>" || t == "<REPO>"
        || t == "<reponame>") {
        return 5;
    }
    if (t == "<|file_sep|>" || t == "<|plamo:file_separator|>") {
        return 6;
    }
    return 0;
}

fn bt_has_unused(gguf: const u8[:], off: i64, len: i64) -> bool {
    if len < 6 {
        return false;
    }
    var i: i64 = 0;
    while i + 6 <= len {
        if gguf[off + i] == 117 && gguf[off + i + 1] == 110 && gguf[off + i + 2] == 117
            && gguf[off + i + 3] == 115 && gguf[off + i + 4] == 101 && gguf[off + i + 5] == 100 {
            return true;
        }
        i = i + 1;
    }
    return false;
}

export fn bt_attrs(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
                   tok_type: const i64[:], attr: i64[:]) -> i64 {
    return bt_attrs_impl(gguf, tok_off, tok_len, tok_type, attr);
}

fn bt_attrs_impl(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
                 tok_type: const i64[:], attr: i64[:]) -> i64 {
    if attr.len < tok_off.len || tok_len.len < tok_off.len || tok_type.len < tok_off.len {
        return -1;
    }
    var i: i64 = 0;
    while i < tok_off.len {
        attr[i] = bt_attr(tok_type[i]);
        i = i + 1;
    }
    // FIM: the first id per class is promoted to CONTROL
    var cls: i64 = 1;
    while cls <= 6 {
        i = 0;
        while i < tok_off.len {
            if bt_fim_class(gguf, tok_off[i], tok_len[i]) == cls {
                attr[i] = attr[i] | BT_CONTROL;
                break;
            }
            i = i + 1;
        }
        cls = cls + 1;
    }
    // control tokens whose text says "unused"
    i = 0;
    while i < tok_off.len {
        if (attr[i] & BT_CONTROL) != 0 && (attr[i] & BT_UNUSED) == 0 {
            if bt_has_unused(gguf, tok_off[i], tok_len[i]) {
                attr[i] = attr[i] | BT_UNUSED;
            }
        }
        i = i + 1;
    }
    // end-of-generation texts are CONTROL
    i = 0;
    while i < tok_off.len {
        if bt_eog_text(gguf, tok_off[i], tok_len[i]) {
            attr[i] = attr[i] | BT_CONTROL;
        }
        i = i + 1;
    }
    // gpt-oss: these are rendered, never suppressed
    i = 0;
    while i < tok_off.len {
        let t: const u8[:] = gguf[tok_off[i] .. tok_off[i] + tok_len[i]];
        if t == "<|channel|>" || t == "<|message|>" || t == "<|start|>" || t == "<|constrain|>" {
            attr[i] = BT_USER_DEFINED;
        }
        i = i + 1;
    }
    return 0;
}

// End-of-generation ids, ascending: every text on the native EOG list, the FIM
// pad/repo/separator ids, and the configured eos/eot/eom ids.
export fn bt_eog_ids(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
                     eos_id: i64, eot_id: i64, eom_id: i64, out: i64[:]) -> i64 {
    return bt_eog_ids_impl(gguf, tok_off, tok_len, eos_id, eot_id, eom_id, out);
}

fn bt_eog_ids_impl(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
                   eos_id: i64, eot_id: i64, eom_id: i64, out: i64[:]) -> i64 {
    var n: i64 = 0;
    var i: i64 = 0;
    while i < tok_off.len {
        var hit: bool = bt_eog_text(gguf, tok_off[i], tok_len[i]);
        if !hit {
            let c: i64 = bt_fim_class(gguf, tok_off[i], tok_len[i]);
            hit = c == 4 || c == 5 || c == 6;
        }
        if i == eos_id || i == eot_id || i == eom_id {
            hit = true;
        }
        if hit {
            if n >= out.len {
                return 0 - (n + 1);
            }
            out[n] = i;
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

// Tokenises one raw text fragment: QWEN35 pre-tokenizer, then BPE per word.
// ids[nout ..] receives the ids; the return value is the number of ids added,
// or -1 when an output buffer is too small (info[5] names the stage).
// info: 0 codepoints, 1 encoded bytes, 2 words, 3 ids, 4 stage.
fn bt_bpe_body(
    text: const u8[:],
    gguf: const u8[:],
    tok_off: const i64[:], tok_len: const i64[:], tok_slots: const i64[:],
    mrg_off: const i64[:], mrg_len: const i64[:], mrg_slots: const i64[:],
    ids: i64[:], nout_in: i64,
    cpts: i64[:], seg: i64[:], enc: u8[:],
    sym: i64[:], heap: i64[:], hc: i64[:],
    info: i64[:]
) -> i64 {
    if info.len < 5 || hc.len < 2 || nout_in < 0 || nout_in > ids.len {
        return -1;
    }
    info[0] = 0;
    info[1] = 0;
    info[2] = 0;
    info[3] = 0;
    info[4] = 0;
    if text.len == 0 {
        return 0;
    }
    let n_word: i64 = pt_qwen35_body(text, cpts, seg, enc, info);
    if n_word < 0 {
        info[4] = 2;
        return -1;
    }
    info[2] = n_word;
    var nout: i64 = nout_in;
    var w: i64 = 0;
    while w < n_word {
        let off: i64 = seg[w * 4 + 2];
        let len: i64 = seg[w * 4 + 3];
        let added: i64 = bt_word(enc, off, len, gguf, tok_off, tok_len, tok_slots,
                                 mrg_off, mrg_len, mrg_slots, ids, nout, sym, heap, hc);
        if added < 0 || hc[1] != 0 {
            info[4] = 3;
            return -1;
        }
        nout = nout + added;
        w = w + 1;
    }
    info[3] = nout - nout_in;
    return nout - nout_in;
}

// Export wrapper: the body stays non-exported so internal callers and the C ABI
// do not compete for one concrete signature.
export fn bt_bpe(
    text: const u8[:], gguf: const u8[:],
    tok_off: const i64[:], tok_len: const i64[:], tok_slots: const i64[:],
    mrg_off: const i64[:], mrg_len: const i64[:], mrg_slots: const i64[:],
    ids: i64[:], nout_in: i64,
    cpts: i64[:], seg: i64[:], enc: u8[:],
    sym: i64[:], heap: i64[:], hc: i64[:], info: i64[:]
) -> i64 {
    return bt_bpe_body(text, gguf, tok_off, tok_len, tok_slots, mrg_off, mrg_len, mrg_slots,
                       ids, nout_in, cpts, seg, enc, sym, heap, hc, info);
}
