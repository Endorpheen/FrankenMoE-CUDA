#!/usr/bin/env python3
"""Staged-only refactor: put the two tokenizer decisions behind a flag.

Not part of the repository: this script edits the staged copy of llama.cpp.
It is kept next to the other stage scripts so the edit is reproducible and
reviewable instead of living only in a diff.

  src/llama-vocab.cpp
    * the "determine vocab type" block moves verbatim into
      llama_vocab::impl::determine_vocab_type_native(ml) and returns true when
      the model has no vocab;
    * a dispatcher determine_vocab_type(ml) picks native or Goose
      (FG_GOOSE_TOKENIZER) and load() calls it;
    * tokenizer_st_partition() becomes a dispatcher, the native body keeps its
      text under tokenizer_st_partition_native().
  common/common.cpp
    * string_strip() becomes a dispatcher, the native body is
      string_strip_native().

The native text is moved, never rewritten. Two mechanical substitutions are
recorded and re-checked by tests/check-native-parity.py:

    __func__   -> "load"        (9 times, keeps the log prefix)
    return;    -> return true;  (1 time, the "no vocab" early return)
"""
import sys
from pathlib import Path

VOCAB = Path('llama.cpp/src/llama-vocab.cpp')
COMMON = Path('llama.cpp/common/common.cpp')

DISPATCH_START = '    // determine vocab type\n    {\n'
DISPATCH_END = ('        ml.get_key(LLM_KV_TOKENIZER_REMOVE_EXTRA_WS, '
                'remove_extra_whitespaces, false);\n    }\n')

PARTITION_SIG = 'void llama_vocab::impl::tokenizer_st_partition('
LOAD_SIG = 'void llama_vocab::impl::load('

DECLS_OLD = ('    void tokenizer_st_partition(std::forward_list<fragment_buffer_variant> & buffer, '
             'bool parse_special) const;\n')

DECLS_NEW = '''    // staged: the two tokenizer decisions behind FG_GOOSE_TOKENIZER
    bool determine_vocab_type(llama_model_loader & ml, const LLM_KV & kv);
    bool determine_vocab_type_native(llama_model_loader & ml, const LLM_KV & kv);
#ifdef FG_GOOSE_TOKENIZER
    bool determine_vocab_type_goose(llama_model_loader & ml, const LLM_KV & kv);
#endif

    void tokenizer_st_partition(std::forward_list<fragment_buffer_variant> & buffer, bool parse_special) const;
    void tokenizer_st_partition_native(std::forward_list<fragment_buffer_variant> & buffer, bool parse_special) const;
'''

CALL_OLD = '    // determine vocab type\n    {\n'

CALL_NEW = '''    // determine vocab type
    // staged: the decision lives in determine_vocab_type(); a true return means
    // the model carries no vocabulary and the rest of load() is skipped.
    if (determine_vocab_type(ml, kv)) {
        return;
    }
'''

GOOSE_DISPATCH = r'''bool llama_vocab::impl::determine_vocab_type(llama_model_loader & ml, const LLM_KV & kv) {
#ifdef FG_GOOSE_TOKENIZER
    return determine_vocab_type_goose(ml, kv);
#else
    return determine_vocab_type_native(ml, kv);
#endif
}

// Native body of the "determine vocab type" block of load(), moved here
// unchanged apart from __func__ -> "load" (log prefix) and the early return.
bool llama_vocab::impl::determine_vocab_type_native(llama_model_loader & ml, const LLM_KV & kv) {
    struct gguf_context * ctx = ml.metadata;

@@BODY@@    return false;
}

#ifdef FG_GOOSE_TOKENIZER
// The Goose path. The module decides everything the native block decides; the
// host still reads the gguf keys, hands the answers over, and owns the arrays
// (bpe ranks, charsmap bytes, dummy tokens for a model without vocab).
bool llama_vocab::impl::determine_vocab_type_goose(llama_model_loader & ml, const LLM_KV & kv) {
    struct gguf_context * ctx = ml.metadata;

    ml.get_key(LLM_KV_TOKENIZER_MODEL, tokenizer_model);
    ml.get_key(LLM_KV_TOKENIZER_PRE,   tokenizer_pre, false);

    ml.get_key(LLM_KV_TOKENIZER_TOKEN_TYPE_COUNT, n_token_types, false);

    fg_goose_vt_answers answers;

    // tokenizer.ggml.merges: presence and the two gguf types, never the values
    const int merges_keyidx = gguf_find_key(ctx, kv(LLM_KV_TOKENIZER_MERGES).c_str());
    if (merges_keyidx != -1) {
        answers.has_merges         = 1;
        answers.merges_kv_is_array = gguf_get_kv_type(ctx, merges_keyidx) == GGUF_TYPE_ARRAY;
        answers.merges_arr_is_string =
            answers.merges_kv_is_array && gguf_get_arr_type(ctx, merges_keyidx) == GGUF_TYPE_STRING;
        answers.n_merges = (int64_t) gguf_get_arr_n(ctx, merges_keyidx);
    }

    // tokenizer.ggml.precompiled_charsmap: presence, array type, element type,
    // length and the xcda_blob_size header value. The blob is only read when
    // the element type is a byte type and the blob holds the 4-byte header; the
    // module classifies the other cases itself.
    const int charsmap_keyidx = gguf_find_key(ctx, kv(LLM_KV_TOKENIZER_PRECOMPILED_CHARSMAP).c_str());
    if (charsmap_keyidx != -1) {
        answers.charsmap_present    = 1;
        answers.charsmap_kv_is_array = gguf_get_kv_type(ctx, charsmap_keyidx) == GGUF_TYPE_ARRAY;
        if (answers.charsmap_kv_is_array) {
            const gguf_type pc_type = gguf_get_arr_type(ctx, charsmap_keyidx);
            answers.charsmap_is_i8_or_u8 = (pc_type == GGUF_TYPE_INT8 || pc_type == GGUF_TYPE_UINT8);
            answers.charsmap_len = (int64_t) gguf_get_arr_n(ctx, charsmap_keyidx);
            if (answers.charsmap_is_i8_or_u8 && answers.charsmap_len >= (int64_t) sizeof(uint32_t)) {
                memcpy(&answers.charsmap_blob, gguf_get_arr_data(ctx, charsmap_keyidx), sizeof(uint32_t));
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                answers.charsmap_blob = __builtin_bswap32(answers.charsmap_blob);
#endif
            }
        }
    }

    fg_goose_vt_decision decision;
    if (fg_goose_vocab_type_decide(tokenizer_model, tokenizer_pre, answers, decision) != 0) {
        // The refusal text is the native one, byte for byte.
        throw std::runtime_error(decision.error);
    }

    type     = (enum llama_vocab_type)     decision.type;
    pre_type = (enum llama_vocab_pre_type) decision.pre_type;

    special_bos_id  = decision.bos;
    special_eos_id  = decision.eos;
    special_unk_id  = decision.unk;
    special_sep_id  = decision.sep;
    special_pad_id  = decision.pad;
    special_mask_id = decision.mask;

    add_space_prefix   = decision.add_space_prefix   != 0;
    escape_whitespaces = decision.escape_whitespaces != 0;
    clean_spaces       = decision.clean_spaces       != 0;
    ignore_merges      = decision.ignore_merges      != 0;
    add_bos            = decision.add_bos            != 0;
    add_eos            = decision.add_eos            != 0;
    add_sep            = decision.add_sep            != 0;

    normalizer_opts.lowercase = decision.lower_case != 0;

    if (!decision.pre_out.empty()) {
        tokenizer_pre = decision.pre_out;
    }

    if (type == LLAMA_VOCAB_TYPE_NONE) {
        linefeed_id = LLAMA_TOKEN_NULL;

        // read vocab size from metadata
        uint32_t n_tokens = 0;
        if (ml.get_key(LLM_KV_VOCAB_SIZE, n_tokens, false)) {
            LLAMA_LOG_WARN("%s: adding %u dummy tokens\n", "load", n_tokens);
            id_to_token.resize(n_tokens);
        }

        return true;
    }

    // Arrays stay host-owned. The module counted the merges and validated the
    // charsmap header; the values are read here, exactly as native reads them.
    if (type == LLAMA_VOCAB_TYPE_BPE && merges_keyidx != -1) {
        const int n_merges = gguf_get_arr_n(ctx, merges_keyidx);
        for (int i = 0; i < n_merges; i++) {
            const std::string word = gguf_get_arr_str(ctx, merges_keyidx, i);

            std::string first;
            std::string second;

            const size_t pos = word.find(' ', 1);

            if (pos != std::string::npos) {
                first  = word.substr(0, pos);
                second = word.substr(pos + 1);
            }

            bpe_ranks.emplace(std::make_pair(first, second), i);
        }
    }

    if (type == LLAMA_VOCAB_TYPE_UGM && charsmap_keyidx != -1 &&
        answers.charsmap_kv_is_array && answers.charsmap_is_i8_or_u8) {
        const size_t n_precompiled_charsmap = gguf_get_arr_n(ctx, charsmap_keyidx);
        const char * pc = (const char *) gguf_get_arr_data(ctx, charsmap_keyidx);
        precompiled_charsmap.assign(pc, pc + n_precompiled_charsmap);
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        // correct endianness of data in precompiled_charsmap binary blob
        if (precompiled_charsmap.size() >= sizeof(uint32_t)) {
            uint32_t * xcda_blob_size = (uint32_t *) &precompiled_charsmap[0];
            *xcda_blob_size = __builtin_bswap32(*xcda_blob_size);
            size_t xcda_array_size = *xcda_blob_size / sizeof(uint32_t);
            uint32_t * xcda_array = (uint32_t *) &precompiled_charsmap[sizeof(uint32_t)];
            for (size_t i = 0; i < xcda_array_size; ++i) {
                xcda_array[i] = __builtin_bswap32(xcda_array[i]);
            }
        }
#endif
    }

    ml.get_key(LLM_KV_TOKENIZER_ADD_PREFIX,      add_space_prefix,         false);
    ml.get_key(LLM_KV_TOKENIZER_REMOVE_EXTRA_WS, remove_extra_whitespaces, false);

    return false;
}
#endif

'''

PARTITION_DISPATCH = r'''void llama_vocab::impl::tokenizer_st_partition(std::forward_list<fragment_buffer_variant> & buffer, bool parse_special) const {
#ifdef FG_GOOSE_TOKENIZER
    // No special tokens: the native function would walk the buffer and change
    // nothing, so take the same shortcut without building the module arrays.
    if (cache_special_tokens.empty()) {
        tokenizer_st_partition_native(buffer, parse_special);
        return;
    }

    const int64_t n_tok   = (int64_t) id_to_token.size();
    const int64_t n_order = (int64_t) cache_special_tokens.size();

    int64_t text_bytes = 0;
    for (const auto & t : id_to_token) {
        if (t.text.empty()) {
            // The native loop cannot advance past a zero-length match; the
            // loader refuses such a vocab upstream. Refuse it loudly instead of
            // spinning.
            throw std::runtime_error("tokenizer_st_partition: empty special token text");
        }
        text_bytes += (int64_t) t.text.size();
    }

    std::vector<int64_t> tok_off (n_tok);
    std::vector<int64_t> tok_len (n_tok);
    std::vector<int64_t> tok_attr(n_tok);
    std::vector<char>    tok_text((size_t) text_bytes + 1);

    int64_t at = 0;
    for (int64_t i = 0; i < n_tok; ++i) {
        tok_off[i]  = at;
        tok_len[i]  = (int64_t) id_to_token[i].text.size();
        tok_attr[i] = (int64_t) id_to_token[i].attr;
        memcpy(tok_text.data() + at, id_to_token[i].text.data(), (size_t) tok_len[i]);
        at += tok_len[i];
    }

    std::vector<int64_t> order(n_order);
    for (int64_t i = 0; i < n_order; ++i) {
        order[i] = (int64_t) cache_special_tokens[i];
    }

    std::forward_list<fragment_buffer_variant> out;
    auto it_out = out.before_begin();

    for (const auto & fragment : buffer) {
        if (fragment.type != FRAGMENT_BUFFER_VARIANT_TYPE_RAW_TEXT) {
            it_out = out.emplace_after(it_out, fragment.token);
            continue;
        }

        const int64_t base = (int64_t) fragment.offset;
        const int64_t len  = (int64_t) fragment.length;

        // Each match adds at most two fragments; the bound is the one the gate
        // was measured on (see special-partition.goose).
        const int64_t cap = 4 * (len + 2) + 16 * n_order + 16;

        std::vector<int64_t> out_kind((size_t) cap);
        std::vector<int64_t> out_tok ((size_t) cap);
        std::vector<int64_t> out_off ((size_t) cap);
        std::vector<int64_t> out_len ((size_t) cap);

        const int64_t n = fg_goose_st_partition(fragment.raw_text.data() + base, len,
                                                n_tok,
                                                tok_off.data(), tok_len.data(),
                                                tok_text.data(), tok_attr.data(),
                                                order.data(), n_order,
                                                parse_special ? 1 : 0,
                                                out_kind.data(), out_tok.data(),
                                                out_off.data(), out_len.data(),
                                                cap);
        if (n < 0) {
            throw std::runtime_error("tokenizer_st_partition: fragment budget exhausted");
        }

        for (int64_t i = 0; i < n; ++i) {
            if (out_kind[i] == 0) {
                it_out = out.emplace_after(it_out, (llama_token) out_tok[i]);
            } else {
                it_out = out.emplace_after(it_out, fragment.raw_text,
                                           base + out_off[i], out_len[i]);
            }
        }
    }

    buffer = std::move(out);
#else
    tokenizer_st_partition_native(buffer, parse_special);
#endif
}

void llama_vocab::impl::tokenizer_st_partition_native(
'''

BRIDGE_INCLUDE = '#include "fg-goose-tokenizer-bridge.h"\n'

COMMON_OLD = '''std::string string_strip(const std::string & str) {
    size_t start = 0;
    size_t end = str.size();
    while (start < end && std::isspace(str[start])) {
        start++;
    }
    while (end > start && std::isspace(str[end - 1])) {
        end--;
    }
    return str.substr(start, end - start);
}
'''

COMMON_NEW = '''static std::string string_strip_native(const std::string & str) {
    size_t start = 0;
    size_t end = str.size();
    while (start < end && std::isspace(str[start])) {
        start++;
    }
    while (end > start && std::isspace(str[end - 1])) {
        end--;
    }
    return str.substr(start, end - start);
}

std::string string_strip(const std::string & str) {
#ifdef FG_GOOSE_TOKENIZER
    // The module owns the whitespace decision; the buffer is always large
    // enough, so -1 cannot happen and the native body stays the fallback.
    std::string out(str.size() + 1, '\\0');
    const int64_t n = fg_goose_cs_strip(str.data(), (int64_t) str.size(),
                                        out.data(), (int64_t) out.size());
    if (n < 0) {
        return string_strip_native(str);
    }
    out.resize((size_t) n);
    return out;
#else
    return string_strip_native(str);
#endif
}
'''


def main():
    vocab = VOCAB.read_text()
    common = COMMON.read_text()

    if 'determine_vocab_type_native' in vocab:
        sys.exit('llama-vocab.cpp already refactored')

    start = vocab.index(DISPATCH_START)
    end = vocab.index(DISPATCH_END, start) + len(DISPATCH_END)
    block = vocab[start:end]

    inner = block[len('    // determine vocab type\n') + len('    {\n'):]
    inner = inner[:-len('    }\n')]  # drop the scope braces
    body_lines = []
    for line in inner.split('\n'):
        body_lines.append(line[4:] if line.startswith('    ') else line)
    body = '\n'.join(body_lines)
    if body.count('        return;\n') != 1:
        sys.exit('unexpected early return count in the vocab-type block')
    body = body.replace('        return;\n', '        return true;\n')
    body = body.replace('__func__', '"load"')

    vocab = vocab.replace(DECLS_OLD, DECLS_NEW, 1)

    new_body = GOOSE_DISPATCH.replace("@@BODY@@", body)
    load_at = vocab.index(LOAD_SIG)
    vocab = vocab[:load_at] + new_body + vocab[load_at:]

    vocab = vocab.replace(block, CALL_NEW, 1)

    # partition: the dispatcher is inserted where the native definition starts,
    # and the native definition itself is renamed in the same replacement.
    part_at = vocab.index(PARTITION_SIG)
    vocab = (vocab[:part_at] + PARTITION_DISPATCH +
             vocab[part_at + len(PARTITION_SIG):])

    # includes
    anchor = '#include "llama-vocab.h"\n'
    if BRIDGE_INCLUDE not in vocab:
        vocab = vocab.replace(
            anchor,
            anchor + '\n#ifdef FG_GOOSE_TOKENIZER\n' + BRIDGE_INCLUDE + '#endif\n', 1)

    VOCAB.write_text(vocab)

    common = common.replace(COMMON_OLD, COMMON_NEW, 1)
    anchor = '#include "common.h"\n'
    if BRIDGE_INCLUDE not in common:
        common = common.replace(
            anchor,
            anchor + '\n#ifdef FG_GOOSE_TOKENIZER\n#include "../src/fg-goose-tokenizer-bridge.h"\n#endif\n', 1)
    COMMON.write_text(common)
    print('refactored', VOCAB, 'and', COMMON)


if __name__ == '__main__':
    main()
