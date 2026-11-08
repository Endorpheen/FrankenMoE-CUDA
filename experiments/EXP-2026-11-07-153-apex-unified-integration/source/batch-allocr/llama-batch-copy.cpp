#include "llama-batch-copy.h"

#ifdef FG153_BATCH_ALLOCR_GOOSE
#include "bridge.h"
#include "patched_api.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
extern "C" int64_t fg153_goose_entry(int64_t mode, int64_t a, int64_t b, int64_t c,
                                     int64_t d, int64_t e, int64_t f);
namespace {
std::atomic<uint64_t> g_fg153_calls{0};
std::atomic<uint64_t> g_fg153_fallbacks{0};
std::atomic<uint64_t> g_fg153_ubatches{0};
}
#endif

#include "llama-impl.h"
#include "llama-vocab.h"
#include "llama-memory.h"
#include "llama-hparams.h"
#include "llama-model.h"
#include "llama-context.h"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <sstream>

llama_batch_allocr::llama_batch_allocr(uint32_t n_pos_per_embd, bool allow_mixed) : n_pos_per_embd(n_pos_per_embd), allow_mixed(allow_mixed) {
    const char * LLAMA_BATCH_DEBUG = getenv("LLAMA_BATCH_DEBUG");
    debug = LLAMA_BATCH_DEBUG ? atoi(LLAMA_BATCH_DEBUG) : 0;

    seq_pos.resize(LLAMA_MAX_SEQ);
    seq_cpl.resize(LLAMA_MAX_SEQ);
    for (auto & cur : seq_cpl) {
        cur.resize(LLAMA_MAX_SEQ);
    }

    seq_idx.resize(LLAMA_MAX_SEQ, -1);
}

bool llama_batch_allocr::init(
        const llama_batch_ext & batch_inp,
        const llama_vocab & vocab,
        bool output_all) {
#ifdef FG153_BATCH_ALLOCR_GOOSE
    if (fg153_goose_enabled()) return fg153_goose_init(batch_inp, vocab, output_all);
#endif
    clear();

    this->vocab        = &vocab;
    this->n_embd       = batch_inp.n_embd > 0 ? batch_inp.n_embd : batch_inp.n_embd_inp;
    this->n_embd_state = batch_inp.n_embd_state;
    this->n_seq_max    = batch_inp.n_seq_max;

    const int32_t n_tok = (int32_t) batch_inp.tokens.size();

    GGML_ASSERT(n_tok > 0);

    if ((uint32_t) n_seq_max > LLAMA_MAX_SEQ) {
        LLAMA_LOG_ERROR("%s: n_seq_max = %d > %d\n", __func__, n_seq_max, LLAMA_MAX_SEQ);
        return false;
    }

    const llama_memory_i * mem = batch_inp.mem;

    //
    // determine the content types of the batch
    // an entry can carry a token id, a token embedding, or both
    // all entries must carry the same combination, or be a mix of token and embd entries
    // a state embedding (e.g. MTP hook batches) is set on all entries or on none
    //

    int32_t n_tok_only  = 0;
    int32_t n_embd_only = 0;
    int32_t n_both      = 0;

    const bool has_state = batch_inp.tokens[0].has_state;

    for (int32_t i = 0; i < n_tok; ++i) {
        const bool is_tok = batch_inp.tokens[i].id != LLAMA_TOKEN_NULL;
        const bool is_emb = batch_inp.tokens[i].has_embd;

        if (!is_tok && !is_emb) {
            LLAMA_LOG_ERROR("%s: entry %d has neither a token id nor an embedding\n", __func__, i);
            return false;
        }

        if (batch_inp.tokens[i].has_state != has_state) {
            LLAMA_LOG_ERROR("%s: all entries in the batch must have the same state embedding presence\n", __func__);
            return false;
        }

        n_tok_only  += is_tok && !is_emb;
        n_embd_only += is_emb && !is_tok;
        n_both      += is_tok &&  is_emb;
    }

    if (n_both > 0 && n_both != n_tok) {
        LLAMA_LOG_ERROR("%s: entries with both a token id and an embedding cannot be mixed with other entries\n", __func__);
        return false;
    }

    const bool mixed = n_tok_only > 0 && n_embd_only > 0;

    if (mixed && !allow_mixed) {
        LLAMA_LOG_ERROR("%s: this model or context does not support batches mixing token and embedding entries\n", __func__);
        return false;
    }

    const bool has_token = n_tok_only  > 0 || n_both > 0;
    const bool has_embd  = n_embd_only > 0 || n_both > 0;

    if (mixed) {
        is_embd_vec.resize(n_tok);
        for (int32_t i = 0; i < n_tok; ++i) {
            is_embd_vec[i] = batch_inp.tokens[i].has_embd ? 1 : 0;
        }
    }

    //
    // build flat token/embd array
    //

    if (has_token) {
        token_vec.resize(n_tok);
        for (int32_t i = 0; i < n_tok; ++i) {
            if (mixed && is_embd_vec[i]) {
                token_vec[i] = 0; // placeholder
                continue;
            }
            const llama_token id = batch_inp.tokens[i].id;
            if (id < 0 || id >= batch_inp.n_vocab) {
                LLAMA_LOG_ERROR("%s: invalid token[%d] = %d\n", __func__, i, id);
                return false;
            }
            token_vec[i] = id;
        }
    }

    if (mixed) {
        embd_vec.assign((size_t) n_tok*n_embd, 0.0f);
        for (int32_t i = 0; i < n_tok; ++i) {
            if (is_embd_vec[i]) {
                const float * src = batch_inp.embd.data() + batch_inp.tokens[i].embd_off;
                std::copy(src, src + n_embd, embd_vec.data() + (size_t) i*n_embd);
            }
        }
    } else if (has_embd) {
        embd_vec = batch_inp.embd;
    }

    if (has_state) {
        state_vec = batch_inp.state;
    }

    //
    // build flat pos array, section-major: pos[j*n_tok + i] = section j of entry i
    // token entry: [p, p, p, 0] (M-RoPE text position)
    // embd entry:  tokens[i].pos as-is
    //

    pos.resize((size_t) n_tok*n_pos_per_embd);
    for (int32_t i = 0; i < n_tok; ++i) {
        const auto & tok = batch_inp.tokens[i];
        const bool expand = tok.id != LLAMA_TOKEN_NULL;
        for (uint32_t j = 0; j < n_pos_per_embd; ++j) {
            llama_pos p = tok.pos[j];
            if (expand) {
                // expand [p] to [p, p, p, 0] for M-RoPE
                p = j < 3 ? tok.pos[0] : 0;
            }
            pos[(size_t) j*n_tok + i] = p;
        }
    }

    //
    // build n_seq_id / seq_id arrays
    //

    n_seq_id.resize(n_tok);
    seq_id.resize(n_tok + 1);
    seq_id[n_tok] = nullptr;

    {
        size_t total = 0;
        for (int32_t i = 0; i < n_tok; ++i) {
            total += batch_inp.tokens[i].seq_ids.size();
        }
        seq_id_data.reserve(total);

        for (int32_t i = 0; i < n_tok; ++i) {
            for (auto sid : batch_inp.tokens[i].seq_ids) {
                seq_id_data.push_back(sid);
            }
        }

        size_t off = 0;
        for (int32_t i = 0; i < n_tok; ++i) {
            n_seq_id[i] = (int32_t) batch_inp.tokens[i].seq_ids.size();
            seq_id[i]   = seq_id_data.data() + off;
            off += n_seq_id[i];

            for (int32_t s = 0; s < n_seq_id[i]; ++s) {
                if (seq_id[i][s] < 0 || seq_id[i][s] >= (llama_seq_id) n_seq_max) {
                    LLAMA_LOG_ERROR("%s: invalid seq_id[%d][%d] = %d >= %d\n", __func__, i, s, seq_id[i][s], (llama_seq_id) n_seq_max);
                    return false;
                }
            }
        }
    }

    //
    // build output/logits array
    //

    {
        output.resize(n_tok, 0);
        for (int32_t i = 0; i < n_tok; ++i) {
            output[i] = batch_inp.tokens[i].output ? 1 : 0;
        }

        if (output_all) {
            bool warn = false;
            for (int32_t i = 0; i < n_tok; ++i) {
                if (!output[i]) { warn = true; break; }
            }
            if (warn) {
                LLAMA_LOG_WARN("%s: embeddings required but some input tokens were not marked as outputs -> overriding\n", __func__);
                std::fill(output.begin(), output.end(), 1);
            }
        }
    }

    // kept empty if no entry has one
    for (int32_t i = 0; i < n_tok; ++i) {
        if (batch_inp.tokens[i].decision_order != 0) {
            decision_order.resize(n_tok, 0);
            decision_order[i] = batch_inp.tokens[i].decision_order;
        }
    }

    //
    // set up the internal llama_batch to point to our owned arrays
    //

    batch.n_tokens = n_tok;
    batch.token    = has_token ? token_vec.data() : nullptr;
    batch.embd     = has_embd  ? embd_vec.data()  : nullptr;
    batch.pos      = pos.data();
    batch.n_seq_id = n_seq_id.data();
    batch.seq_id   = seq_id.data();
    batch.logits   = output.data();

    //
    // compute stats
    //

    // count the outputs in this batch
    for (int32_t i = 0; i < batch.n_tokens; ++i) {
        n_outputs += batch.logits[i] != 0;
    }

    has_cpl = false;

    // determine coupled sequences
    // these are pairs of sequences that have at least one token in the input batch that is assigned to both of them
    for (int32_t i = 0; i < batch.n_tokens; ++i) {
        const llama_seq_id s0 = batch.seq_id[i][0];

        for (int32_t s = 0; s < batch.n_seq_id[i]; ++s) {
            const llama_seq_id s1 = batch.seq_id[i][s];

            seq_pos[s1].insert(batch.pos[i]);

            if (s > 0) {
                // mark that sequence s1 is coupled to s0
                seq_cpl[s1][s0] = true;

                // note: tracking the other way around is not necessary for now
                //seq_cpl[s0][s1] = true;

                has_cpl = true;
            }
        }
    }

    // precompute the sequence sets for each token and determine the unique sequence ids that participate in the batch
    {
        seq_set_t seq_set_unq;

        for (int32_t i = 0; i < batch.n_tokens; ++i) {
            seq_set_t cur;
            for (int32_t s = 0; s < batch.n_seq_id[i]; ++s) {
                const llama_seq_id seq_id = batch.seq_id[i][s];

                cur        .set(seq_id);
                seq_set_unq.set(seq_id);
            }

            seq_set.push_back(cur);
            seq_set_map[cur].push_back(i);
        }

        for (uint32_t s = 0; s < n_seq_max; ++s) {
            if (seq_set_unq.test(s)) {
                seq_idx[s] = seq_id_unq.size();
                seq_id_unq.push_back(s);
            }
        }
    }

    if (debug > 0) {
        LLAMA_LOG_DEBUG("%s: input batch info:\n", __func__);

        llama_ubatch ubatch {
            /*.b_equal_seqs =*/ false,
            /*.n_tokens     =*/ (uint32_t) batch.n_tokens,
            /*.n_seq_tokens =*/ (uint32_t) 1,
            /*.n_seqs       =*/ (uint32_t) batch.n_tokens,
            /*.n_seqs_unq   =*/ (uint32_t) this->seq_id_unq.size(),
            /*.n_pos        =*/ n_pos_per_embd,
            /*.token        =*/ batch.token,
            /*.embd         =*/ batch.embd,
            /*.embd_state   =*/ state_vec.empty() ? nullptr : state_vec.data(),
            /*.pos          =*/ batch.pos,
            /*.n_seq_id     =*/ batch.n_seq_id,
            /*.seq_id       =*/ batch.seq_id,
            /*.seq_id_unq   =*/ this->seq_id_unq.data(),
            /*.seq_idx      =*/ this->seq_idx.data(),
            /*.output       =*/ batch.logits,
            /*.type         =*/ is_embd_vec.empty() ? nullptr : is_embd_vec.data(),
            /*.decision_order =*/ decision_order.empty() ? nullptr : decision_order.data(),
            /*.data         =*/ {},
        };

        ubatch_print(ubatch, debug);

        LLAMA_LOG_DEBUG("%s:   seq       = [\n", __func__);
        for (int s0 = 0; s0 < (int) seq_pos.size(); ++s0) {
            if (seq_pos[s0].empty()) {
                continue;
            }

            std::stringstream ss;
            for (int s1 = 0; s1 < (int) seq_cpl[s0].size(); ++s1) {
                if (seq_cpl[s0][s1]) {
                    ss << s1 << " ";
                }
            }

            LLAMA_LOG_DEBUG("%s:  %4d: pos = [%4d, %4d], cpl = %s\n",
                    __func__, s0, seq_pos_min(s0), seq_pos_max(s0), ss.str().empty() ? "-" : ss.str().c_str());
        }
        LLAMA_LOG_DEBUG("%s:   ]\n", __func__);
    }

    //
    // consistency checks
    //

    if (n_pos_per_embd > 1) {
        // in a mixed batch, the first entry of each seq picks the rule
        std::vector<int8_t> seq_first_embd(n_seq_max, batch.token ? 0 : 1);
        if (mixed) {
            std::vector<bool> seen(n_seq_max, false);
            for (int32_t i = 0; i < batch.n_tokens; ++i) {
                for (int32_t s = 0; s < batch.n_seq_id[i]; ++s) {
                    const llama_seq_id sid = batch.seq_id[i][s];
                    if (!seen[sid]) {
                        seen[sid] = true;
                        seq_first_embd[sid] = is_embd_vec[i];
                    }
                }
            }
        }

        // M-RoPE case: allow position to "jump" forward only (non-continuous positions are allowed)
        for (uint32_t s = 0; s < n_seq_max; ++s) {
            if (seq_pos[s].empty()) {
                continue;
            }

            const llama_pos p0 = mem ? mem->seq_pos_max(s) : -1;

            if (!seq_first_embd[s]) {
                if (p0 >= 0 && p0 >= seq_pos_min(s)) {
                    LLAMA_LOG_ERROR(
                            "%s: the tokens of sequence %d in the input batch have inconsistent sequence positions:\n"
                            " - the last position stored in the memory module of the context (i.e. the KV cache) for sequence %d is X = %d\n"
                            " - the tokens for sequence %d in the input batch have a starting position of Y = %d\n"
                            " for M-RoPE, it is required that the position satisfies: X < Y\n",
                            __func__, s, s, p0, s, seq_pos_min(s));

                    return false;
                }
            } else {
                // embedding inputs can have overlapping positions
                if (p0 >= 0 && p0 > seq_pos_min(s)) {
                    LLAMA_LOG_ERROR(
                            "%s: the tokens of sequence %d in the input batch have inconsistent sequence positions:\n"
                            " - the last position stored in the memory module of the context (i.e. the KV cache) for sequence %d is X = %d\n"
                            " - the tokens for sequence %d in the input batch have a starting position of Y = %d\n"
                            " for M-RoPE, it is required that the position satisfies: X <= Y\n",
                            __func__, s, s, p0, s, seq_pos_min(s));

                    return false;
                }
            }
        }
    } else {
        for (uint32_t s = 0; s < n_seq_max; ++s) {
            if (seq_pos[s].empty()) {
                continue;
            }

            const llama_pos p0 = mem ? mem->seq_pos_max(s) : -1;

            if (p0 >= 0) {
                bool ok = true;

                if (seq_pos_min(s) != p0 + 1) {
                    ok = false;
                }

                if (!ok) {
                    LLAMA_LOG_ERROR(
                            "%s: the tokens of sequence %d in the input batch have inconsistent sequence positions:\n"
                            " - the last position stored in the memory module of the context (i.e. the KV cache) for sequence %d is X = %d\n"
                            " - the tokens for sequence %d in the input batch have a starting position of Y = %d\n"
                            " it is required that the sequence positions remain consecutive: Y = X + 1\n",
                            __func__, s, s, p0, s, seq_pos_min(s));

                    return false;
                }
            }

            if (seq_pos_max(s) - seq_pos_min(s) + 1 > (int) seq_pos[s].size()) {
                LLAMA_LOG_ERROR("%s: sequence %d positions are not continuous\n", __func__, s);
                return false;
            }
        }
    }

    if (mem) {
        for (uint32_t s0 = 0; s0 < n_seq_max; ++s0) {
            for (uint32_t s1 = 0; s1 < n_seq_max; ++s1) {
                if (seq_cpl[s0][s1]) {
                    if (mem->seq_pos_min(s0) != mem->seq_pos_min(s1) ||
                        mem->seq_pos_max(s0) != mem->seq_pos_max(s1)) {
                        LLAMA_LOG_ERROR("%s: sequence %d is coupled to %d in the input batch, but have divereged\n", __func__, s0, s1);
                        return false;
                    }
                }
            }
        }
    }

    // disallow partial sequence sub-sets:
    //
    // invalid:          x
    //            i: 0 1 2 ...
    // ---------------------------------------
    // seq_id[i][0]: 0 0 1
    // seq_id[i][1]: 1 1 2
    // seq_id[i][2]: 2
    //
    // disallow decreasing sequence positions:
    //
    // invalid:                  x
    //            i: 0 1 2 3 4 5 6 ...
    // ---------------------------------------
    //       pos[i]: 4 5 0 1 6 2 3
    // seq_id[i][0]: 0 0 1 1 0 1 0
    //
    {
        seq_set_t cur_seq_set[LLAMA_MAX_SEQ];
        for (uint32_t s = 0; s < n_seq_max; ++s) {
            cur_seq_set[s].set();
        }

        llama_pos cur_seq_pos[LLAMA_MAX_SEQ];
        for (uint32_t s = 0; s < n_seq_max; ++s) {
            cur_seq_pos[s] = -1;
        }

        for (int32_t i = 0; i < batch.n_tokens; ++i) {
            const llama_pos pos = batch.pos[i];

            for (int32_t s = 0; s < batch.n_seq_id[i]; ++s) {
                const llama_seq_id seq_id = batch.seq_id[i][s];

                cur_seq_set[seq_id] &= seq_set[i];

                if (cur_seq_set[seq_id].none()) {
                    LLAMA_LOG_ERROR("%s: sequence %d belongs to incompatible sequence sets (not allowed)\n", __func__, seq_id);
                    return false;
                }

                if (pos < cur_seq_pos[seq_id]) {
                    LLAMA_LOG_ERROR("%s: sequence %d positions are decreasing (not allowed)\n", __func__, seq_id);
                    return false;
                }

                cur_seq_pos[seq_id] = pos;
            }
        }
    }

    split_reset();

    return true;
}

llama_ubatch llama_batch_allocr::ubatch_reserve(uint32_t n_seq_tokens, uint32_t n_seqs) {
#ifdef FG153_BATCH_ALLOCR_GOOSE
    if (fg153_goose_enabled()) return fg153_goose_ubatch_reserve(n_seq_tokens, n_seqs);
#endif
    const uint32_t n_tokens = n_seq_tokens*n_seqs;

    clear();
    split_reset();

    const int64_t n_pos_all = (int64_t) n_tokens*n_pos_per_embd;

    auto udata = std::make_shared<llama_ubatch::data_t>();

    udata->token     .resize(n_tokens);
    udata->embd      .clear();
    udata->embd_state.clear();
    udata->pos       .resize(n_pos_all);
    udata->n_seq_id  .resize(n_tokens);
    udata->seq_id    .resize(n_tokens);
    udata->seq_id_unq.resize(0);
    udata->seq_idx   .resize(LLAMA_MAX_SEQ, -1);
    udata->output    .resize(n_tokens);

    for (uint32_t s = 0; s < n_seqs; ++s) {
        udata->seq_idx[s] = s;
        udata->seq_id_unq.push_back(s);
    }

    llama_ubatch res {
        /*.b_equal_seqs =*/ true,
        /*.n_tokens     =*/ n_tokens,
        /*.n_seq_tokens =*/ n_seq_tokens,
        /*.n_seqs       =*/ n_seqs,
        /*.n_seqs_unq   =*/ n_seqs,
        /*.n_pos        =*/ n_pos_per_embd,

        /*.token        =*/ udata->token.data(),
        /*.embd         =*/ nullptr,
        /*.embd_state   =*/ nullptr,
        /*.pos          =*/ udata->pos.data(),
        /*.n_seq_id     =*/ udata->n_seq_id.data(),
        /*.seq_id       =*/ udata->seq_id.data(),
        /*.seq_id_unq   =*/ udata->seq_id_unq.data(),
        /*.seq_idx      =*/ udata->seq_idx.data(),
        /*.output       =*/ udata->output.data(),
        /*.type         =*/ nullptr,
        /*.decision_order =*/ nullptr,
        /*.data         =*/ std::move(udata),
    };

    return res;
}

const llama_batch & llama_batch_allocr::get_batch() const {
    return batch;
}

uint32_t llama_batch_allocr::get_n_tokens() const {
    return batch.n_tokens;
}

uint32_t llama_batch_allocr::get_n_outputs() const {
    return n_outputs;
}

uint32_t llama_batch_allocr::get_n_used() const {
    return n_used;
}

std::vector<int32_t> & llama_batch_allocr::get_out_ids() {
    return out_ids;
}

llama_pos llama_batch_allocr::seq_pos_min(llama_seq_id seq_id) const {
    return seq_pos[seq_id].empty() ? -1 : *seq_pos[seq_id].begin();
}

llama_pos llama_batch_allocr::seq_pos_max(llama_seq_id seq_id) const {
    return seq_pos[seq_id].empty() ? -1 : *seq_pos[seq_id].rbegin();
}

void llama_batch_allocr::split_reset() {
#ifdef FG153_BATCH_ALLOCR_GOOSE
    if (fg153_goose_enabled()) { fg153_goose_split_reset(); return; }
#endif
    out_ids.clear();

    n_used = 0;

    used.clear();
    used.resize(get_n_tokens(), false);
}

llama_ubatch llama_batch_allocr::split_simple(uint32_t n_ubatch) {
#ifdef FG153_BATCH_ALLOCR_GOOSE
    if (fg153_goose_enabled()) return fg153_goose_split_simple(n_ubatch);
#endif
    // find the first unused token
    uint32_t cur_idx = 0;
    while (cur_idx < used.size() && used[cur_idx]) {
        ++cur_idx;
    }

    // we are done
    if (cur_idx >= used.size()) {
        return {};
    }

    std::vector<int32_t> idxs;

    while (true) {
        idxs.push_back(cur_idx);

        used[cur_idx] = true;
        ++n_used;

        ++cur_idx;

        if (cur_idx >= used.size()) {
            break;
        }

        if (idxs.size() >= n_ubatch) {
            break;
        }
    }

    return ubatch_add(idxs, idxs.size(), false);
}

llama_ubatch llama_batch_allocr::split_equal(uint32_t n_ubatch, bool sequential, uint32_t n_keep_tail) {
#ifdef FG153_BATCH_ALLOCR_GOOSE
    if (fg153_goose_enabled()) return fg153_goose_split_equal(n_ubatch, sequential, n_keep_tail);
#endif
    if (sequential && has_cpl) {
        LLAMA_LOG_ERROR("%s: sequential split is not supported when there are coupled sequences in the input batch (you may need to use the -kvu flag)\n", __func__);

        return {};
    }

    std::vector<seq_set_t> cur_seq_set;

    llama_seq_id last_seq_id = -1;

    // determine the non-overlapping sequence sets participating in this ubatch
    for (int32_t i = 0; i < batch.n_tokens; ++i) {
        if (used[i]) {
            continue;
        }

        bool add = true;

        for (uint32_t s = 0; s < cur_seq_set.size(); ++s) {
            // no overlap with existing sequence sets:
            if (!(cur_seq_set[s] & seq_set[i]).none()) {
                add = false;
                break;
            }
        }

        // accept only increasing sequence ids
        if (sequential) {
            add = add && (cur_seq_set.empty() || batch.seq_id[i][0] == last_seq_id + 1);
        }

        if (add) {
            cur_seq_set.push_back(seq_set[i]);

            last_seq_id = batch.seq_id[i][0];

            if (cur_seq_set.size() > n_ubatch) {
                break;
            }
        }
    }

    uint32_t n_seqs = cur_seq_set.size();

    // we are done
    if (n_seqs == 0) {
        return {};
    }

    // the current batch index of each sequence set
    std::vector<int32_t> cur_idx(n_seqs, 0);

    for (uint32_t s = 0; s < n_seqs; ++s) {
        while (used[seq_set_map[cur_seq_set[s]][cur_idx[s]]]) {
            ++cur_idx[s];
        }
    }

    // the list of batch indices for each sequence set
    // at the end we will concat these to get the final ubatch
    std::vector<idx_vec_t> idxs_per_seq(n_seqs);

    while (true) {
        // we can only add new n_seq_tokens tokens if all the sequence sets have at least 1 more unused tokens and
        //   if we haven't reached n_ubatch
        bool can_expand = true;

        for (uint32_t s = 0; s < n_seqs; ++s) {
            if (cur_idx[s] >= (int32_t) seq_set_map[cur_seq_set[s]].size()) {
                can_expand = false;
                break;
            }
        }

        if (!can_expand) {
            break;
        }

        for (uint32_t s = 0; s < n_seqs; ++s) {
            const int32_t idx = seq_set_map[cur_seq_set[s]][cur_idx[s]];

            idxs_per_seq[s].push_back(idx);

            used[idx] = true;
            ++n_used;

            ++cur_idx[s];
        }

        if  ((idxs_per_seq[0].size() + 1)*n_seqs > n_ubatch) {
            break;
        }
    }

    // if n_keep_tail > 0, keep only the seqs that either finish in this ubatch or have at least
    //   n_keep_tail tokens remaining for a future ubatch, so that the trailing n_keep_tail tokens
    //   of each seq are never split across ubatches
    if (n_keep_tail > 0) {
        GGML_ASSERT(n_ubatch > n_keep_tail);

        auto n_remaining = [&](uint32_t s) {
            return (uint32_t) (seq_set_map[cur_seq_set[s]].size() - cur_idx[s]);
        };

        // keep the longest prefix of seqs that satisfy the constraint, to preserve sequential seq ids
        uint32_t n_keep = 0;
        while (n_keep < n_seqs) {
            const uint32_t remaining = n_remaining(n_keep);

            if (remaining != 0 && remaining < n_keep_tail) {
                break;
            }

            n_keep++;
        }

        // all seqs violate the constraint - resolve the first one directly and emit it alone
        if (n_keep == 0) {
            auto & idxs = idxs_per_seq[0];

            const auto & seq_idxs = seq_set_map[cur_seq_set[0]];

            if (idxs.size() + n_remaining(0) <= n_ubatch) {
                // extend the seq to completion
                while (n_remaining(0) > 0) {
                    const int32_t idx = seq_idxs[cur_idx[0]];

                    idxs.push_back(idx);

                    used[idx] = true;
                    ++n_used;

                    ++cur_idx[0];
                }
            } else {
                // truncate the seq so that at least n_keep_tail tokens remain
                while (n_remaining(0) < n_keep_tail) {
                    used[idxs.back()] = false;
                    --n_used;

                    idxs.pop_back();

                    --cur_idx[0];
                }
            }

            n_keep = 1;
        }

        // return the tokens of the deferred seqs back to the pool
        for (uint32_t s = n_keep; s < n_seqs; ++s) {
            for (const int32_t idx : idxs_per_seq[s]) {
                used[idx] = false;
                --n_used;
            }
        }

        n_seqs = n_keep;
    }

    // concat the per-sequence-set lists
    std::vector<int32_t> idxs;

    for (uint32_t s = 0; s < n_seqs; ++s) {
        idxs.insert(idxs.end(), idxs_per_seq[s].begin(), idxs_per_seq[s].end());
    }

    return ubatch_add(idxs, n_seqs, true);
}

llama_ubatch llama_batch_allocr::split_seq(uint32_t n_ubatch) {
#ifdef FG153_BATCH_ALLOCR_GOOSE
    if (fg153_goose_enabled()) return fg153_goose_split_seq(n_ubatch);
#endif
    // find the first unused token
    uint32_t cur_idx = 0;
    while (cur_idx < used.size() && used[cur_idx]) {
        ++cur_idx;
    }

    // we are done
    if (cur_idx >= used.size()) {
        return {};
    }

    // this is the starting sequence set
    // we allow adding tokens only if their sequence set is a subset of the current sequence set
    auto cur_seq_set = seq_set[cur_idx];

    std::vector<int32_t> idxs;

    while (true) {
        idxs.push_back(cur_idx);

        used[cur_idx] = true;
        ++n_used;

        if (idxs.size() >= n_ubatch) {
            break;
        }

        do {
            ++cur_idx;
        } while (cur_idx < get_n_tokens() && (used[cur_idx] || ((cur_seq_set & seq_set[cur_idx]) != seq_set[cur_idx])));

        if (cur_idx == get_n_tokens()) {
            break;
        }

        cur_seq_set = seq_set[cur_idx];
    }

    return ubatch_add(idxs, 1, true);
}

void llama_batch_allocr::clear() {
    n_outputs = 0;

    batch = {};

    token_vec   .clear();
    embd_vec    .clear();
    is_embd_vec .clear();
    state_vec   .clear();
    seq_id_data .clear();
    pos         .clear();
    n_seq_id    .clear();
    seq_id      .clear();
    seq_id_unq  .clear();
    output      .clear();
    decision_order.clear();

    for (auto & cur : seq_pos) {
        cur.clear();
    }

    for (auto & cur : seq_cpl) {
        std::fill(cur.begin(), cur.end(), false);
    }

    seq_set.clear();

    seq_set_map.clear();

    std::fill(seq_idx.begin(), seq_idx.end(), -1);
}

llama_ubatch llama_batch_allocr::ubatch_add(const std::vector<int32_t> & idxs, uint32_t n_seqs, bool equal_seqs) {
    const uint32_t n_tokens = idxs.size();

    assert(n_tokens%n_seqs == 0);

    auto udata = std::make_shared<llama_ubatch::data_t>();

    const bool mixed_batch = !is_embd_vec.empty();

    // a ubatch with a single kind of rows is emitted as a plain token or embd ubatch
    uint32_t n_embd_rows = 0;
    if (mixed_batch) {
        for (int32_t idx : idxs) {
            n_embd_rows += is_embd_vec[idx];
        }
    }
    const bool mixed     = mixed_batch && n_embd_rows > 0 && n_embd_rows < n_tokens;
    const bool use_token = batch.token && !(mixed_batch && n_embd_rows == n_tokens);
    const bool use_embd  = batch.embd  && !(mixed_batch && n_embd_rows == 0);
    const bool has_state = !state_vec.empty();

    const int64_t n_embd_all  = use_embd  ? (int64_t) n_tokens*n_embd       : 0;
    const int64_t n_state_all = has_state ? (int64_t) n_tokens*n_embd_state : 0;
    const int64_t n_pos_all   =             (int64_t) n_tokens*n_pos_per_embd;

    udata->token     .resize(n_tokens);
    udata->embd      .resize(n_embd_all);
    udata->embd_state.resize(n_state_all);
    udata->pos       .resize(n_pos_all);
    udata->n_seq_id  .resize(n_tokens);
    udata->seq_id    .resize(n_tokens);
    udata->seq_id_unq.resize(0);
    udata->seq_idx   .resize(LLAMA_MAX_SEQ, -1);
    udata->output    .resize(n_tokens);
    udata->type      .resize(mixed ? n_tokens : 0);
    udata->decision_order.resize(decision_order.empty() ? 0 : n_tokens);

    udata->batch_idxs = idxs;
    udata->seq_id_data.reserve(n_tokens);

    seq_set_t seq_set_unq;

    for (size_t i = 0; i < idxs.size(); ++i) {
        if (use_token) {
            udata->token[i] = batch.token[idxs[i]];
        }

        if (use_embd) {
            memcpy(udata->embd.data() + i*n_embd, batch.embd + (int64_t) idxs[i]*n_embd, n_embd*sizeof(float));
        }

        if (mixed) {
            udata->type[i] = is_embd_vec[idxs[i]];
        }

        if (has_state) {
            memcpy(udata->embd_state.data() + i*n_embd_state, state_vec.data() + (int64_t) idxs[i]*n_embd_state, n_embd_state*sizeof(float));
        }

        for (size_t j = 0; j < (size_t)n_pos_per_embd; ++j) {
            udata->pos[j*n_tokens + i] = batch.pos[j*batch.n_tokens + idxs[i]];
        }

        udata->n_seq_id[i] = batch.n_seq_id[idxs[i]];
        udata->output[i]   = batch.logits[idxs[i]];

        if (!decision_order.empty()) {
            udata->decision_order[i] = decision_order[idxs[i]];
        }

        for (int s = 0; s < udata->n_seq_id[i]; ++s) {
            const llama_seq_id seq_id = batch.seq_id[idxs[i]][s];

            udata->seq_id_data.push_back(seq_id);
            seq_set_unq.set(seq_id);
        }

        if (udata->output[i]) {
            out_ids.push_back(idxs[i]);
        }
    }

    llama_seq_id * seq_id_ptr = udata->seq_id_data.data();
    for (size_t i = 0; i < idxs.size(); ++i) {
        udata->seq_id[i] = seq_id_ptr;
        seq_id_ptr += udata->n_seq_id[i];
    }

    for (uint32_t s = 0; s < n_seq_max; ++s) {
        if (seq_set_unq.test(s)) {
            udata->seq_idx[s] = udata->seq_id_unq.size();
            udata->seq_id_unq.push_back(s);
        }
    }

    llama_ubatch res {
        /*.b_equal_seqs =*/ equal_seqs,
        /*.n_tokens     =*/ n_tokens,
        /*.n_seq_tokens =*/ n_tokens/n_seqs,
        /*.n_seqs       =*/ n_seqs,
        /*.n_seqs_unq   =*/ (uint32_t) udata->seq_id_unq.size(),
        /*.n_pos        =*/ n_pos_per_embd,

        /*.token        =*/ use_token ? udata->token.data() : nullptr,
        /*.embd         =*/ use_embd  ? udata->embd.data()  : nullptr,
        /*.embd_state   =*/ has_state ? udata->embd_state.data() : nullptr,
        /*.pos          =*/ udata->pos.data(),
        /*.n_seq_id     =*/ udata->n_seq_id.data(),
        /*.seq_id       =*/ udata->seq_id.data(),
        /*.seq_id_unq   =*/ udata->seq_id_unq.data(),
        /*.seq_idx      =*/ udata->seq_idx.data(),
        /*.output       =*/ udata->output.data(),
        /*.type         =*/ mixed ? udata->type.data() : nullptr,
        /*.decision_order =*/ udata->decision_order.empty() ? nullptr : udata->decision_order.data(),
        /*.data         =*/ std::move(udata),
    };

    if (debug > 0) {
        LLAMA_LOG_DEBUG("%s: added ubatch to split:\n", __func__);

        ubatch_print(res, debug);
    }

    return res;
}

void llama_batch_allocr::ubatch_print(const llama_ubatch & ubatch, int debug) {
    if (debug > 0) {
        LLAMA_LOG_DEBUG("%s:   equal_seqs   = %d\n", __func__, ubatch.equal_seqs());
        LLAMA_LOG_DEBUG("%s:   n_tokens     = %d\n", __func__, ubatch.n_tokens);
        LLAMA_LOG_DEBUG("%s:   n_seq_tokens = %d\n", __func__, ubatch.n_seq_tokens);
        LLAMA_LOG_DEBUG("%s:   n_seqs       = %d\n", __func__, ubatch.n_seqs);
        LLAMA_LOG_DEBUG("%s:   n_seqs_unq   = %d\n", __func__, ubatch.n_seqs_unq);

        std::stringstream ss_seq_id_unq;
        std::stringstream ss_seq_idx;

        ss_seq_id_unq << "[ ";
        ss_seq_idx << "[";

        for (uint32_t s = 0; s < ubatch.n_seqs_unq; ++s) {
            ss_seq_id_unq << ubatch.seq_id_unq[s] << " ";
        }

        for (uint32_t s = 0; s < LLAMA_MAX_SEQ; ++s) {
            if (ubatch.seq_idx[s] >= 0) {
                ss_seq_idx << ubatch.seq_idx[s]%10;
            } else {
                ss_seq_idx << ".";
            }
        }

        ss_seq_id_unq << "]";
        ss_seq_idx    << "]";

        LLAMA_LOG_DEBUG("%s:   token      = %p\n", __func__, (void *) ubatch.token);
        LLAMA_LOG_DEBUG("%s:   embd       = %p\n", __func__, (void *) ubatch.embd);
        LLAMA_LOG_DEBUG("%s:   embd_state = %p\n", __func__, (void *) ubatch.embd_state);
        LLAMA_LOG_DEBUG("%s:   pos        = %p\n", __func__, (void *) ubatch.pos);
        LLAMA_LOG_DEBUG("%s:   n_seq_id   = %p\n", __func__, (void *) ubatch.n_seq_id);
        LLAMA_LOG_DEBUG("%s:   seq_id     = %p\n", __func__, (void *) ubatch.seq_id);
        LLAMA_LOG_DEBUG("%s:   seq_id_unq = %s\n", __func__, ss_seq_id_unq.str().c_str());
        LLAMA_LOG_DEBUG("%s:   seq_idx    = %s\n", __func__, ss_seq_idx.str().c_str());
        LLAMA_LOG_DEBUG("%s:   output     = %p\n", __func__, (void *) ubatch.output);
        LLAMA_LOG_DEBUG("%s:   type       = %p\n", __func__, (void *) ubatch.type);
        LLAMA_LOG_DEBUG("%s:   n_outputs  = %d\n", __func__, n_outputs);

        if (debug > 0) {
            int seq_id_max = 0;
            for (uint32_t i = 0; i < ubatch.n_tokens; ++i) {
                for (int s = 0; s < ubatch.n_seq_id[i]; ++s) {
                    for (int s = 0; s < ubatch.n_seq_id[i]; ++s) {
                        seq_id_max = std::max(seq_id_max, ubatch.seq_id[i][s]);
                    }
                }
            }
            ++seq_id_max;

            LLAMA_LOG_DEBUG("%s:   token     = [\n", __func__);
            for (uint32_t i = 0; i < ubatch.n_tokens; ++i) {
                std::vector<int8_t> seq_id(seq_id_max);

                for (int s = 0; s < ubatch.n_seq_id[i]; ++s) {
                    seq_id[ubatch.seq_id[i][s]] = 1;
                }

                std::stringstream ss;
                for (int s = 0; s < seq_id_max; ++s) {
                    if (seq_id[s]) {
                        ss << s%10;
                    } else {
                        ss << ".";
                    }
                }

                if (ubatch.token && !(ubatch.is_mixed() && ubatch.type[i])) {
                    LLAMA_LOG_DEBUG("%s:  %4d: id = %6d (%16s), pos = %4d, n_seq_id = %2d, seq_id = [%s], output = %d\n",
                            __func__, i, ubatch.token[i], vocab->token_to_piece(ubatch.token[i]).c_str(),
                            ubatch.pos[i], ubatch.n_seq_id[i], ss.str().c_str(), ubatch.output[i]);
                } else {
                    LLAMA_LOG_DEBUG("%s:  %4d: [embd], pos = %4d, n_seq_id = %2d, seq_id = [%s], output = %d\n",
                            __func__, i, ubatch.pos[i], ubatch.n_seq_id[i], ss.str().c_str(), ubatch.output[i]);
                }
            }
            LLAMA_LOG_DEBUG("%s:   ]\n", __func__);
        }
    }
}

//
// interface implementation
//

struct llama_batch llama_batch_get_one(
             llama_token * tokens,
                 int32_t   n_tokens) {
    return {
        /*n_tokens =*/ n_tokens,
        /*tokens   =*/ tokens,
        /*embd     =*/ nullptr,
        /*pos      =*/ nullptr,
        /*n_seq_id =*/ nullptr,
        /*seq_id   =*/ nullptr,
        /*logits   =*/ nullptr,
    };
}

struct llama_batch llama_batch_init(int32_t n_tokens_alloc, int32_t embd, int32_t n_seq_max) {
    llama_batch batch = {
        /*n_tokens =*/ 0,
        /*tokens   =*/ nullptr,
        /*embd     =*/ nullptr,
        /*pos      =*/ nullptr,
        /*n_seq_id =*/ nullptr,
        /*seq_id   =*/ nullptr,
        /*logits   =*/ nullptr,
    };

    if (embd) {
        batch.embd = (float *) malloc(sizeof(float) * n_tokens_alloc * embd);
    } else {
        batch.token = (llama_token *) malloc(sizeof(llama_token) * n_tokens_alloc);
    }

    batch.pos      = (llama_pos *)     malloc(sizeof(llama_pos)      * n_tokens_alloc);
    batch.n_seq_id = (int32_t *)       malloc(sizeof(int32_t)        * n_tokens_alloc);
    batch.seq_id   = (llama_seq_id **) malloc(sizeof(llama_seq_id *) * (n_tokens_alloc + 1));
    for (int i = 0; i < n_tokens_alloc; ++i) {
        batch.seq_id[i] = (llama_seq_id *) malloc(sizeof(llama_seq_id) * n_seq_max);
    }
    batch.seq_id[n_tokens_alloc] = nullptr;

    batch.logits   = (int8_t *)        malloc(sizeof(int8_t)         * n_tokens_alloc);

    return batch;
}

void llama_batch_free(struct llama_batch batch) {
    if (batch.token)    free(batch.token);
    if (batch.embd)     free(batch.embd);
    if (batch.pos)      free(batch.pos);
    if (batch.n_seq_id) free(batch.n_seq_id);
    if (batch.seq_id) {
        for (int i = 0; batch.seq_id[i] != nullptr; ++i) {
            free(batch.seq_id[i]);
        }
        free(batch.seq_id);
    }
    if (batch.logits)   free(batch.logits);
}


// llama_batch_ext

size_t llama_batch_ext_select_n_embd_inp(llama_context_type ctx_type, llm_arch arch, const llama_hparams & hparams) {
    GGML_UNUSED(ctx_type);
    if (arch == LLM_ARCH_DFLASH) {
        return hparams.n_embd_inp_enc();
    }
    return hparams.n_embd_inp();
}

size_t llama_batch_ext_select_n_embd_state(llama_context_type ctx_type, const llama_hparams & hparams) {
    if (ctx_type == LLAMA_CONTEXT_TYPE_MTP) {
        return hparams.n_embd_out();
    }
    // TODO: this probably should become a function of `llm_arch`
    if (hparams.lilicorr_candidate_topk > 0) {
        return hparams.n_embd_inp();
    }
    return 0;
}

llama_batch_ext::llama_batch_ext(llama_context * ctx) :
        n_tokens_max(llama_n_batch(ctx)),
        n_embd_inp(llama_batch_ext_select_n_embd_inp(ctx->get_cparams().ctx_type, llama_get_model(ctx)->arch, llama_get_model(ctx)->hparams)),
        n_embd_inp_enc(llama_get_model(ctx)->hparams.n_embd_inp_enc()),
        n_embd_state(llama_batch_ext_select_n_embd_state(ctx->get_cparams().ctx_type, llama_get_model(ctx)->hparams)),
        n_seq_max(llama_n_seq_max(ctx)),
        mem(llama_get_memory(ctx)),
        n_vocab(llama_vocab_n_tokens(llama_model_get_vocab(llama_get_model(ctx)))),
        n_pos_per_embd(llama_get_model(ctx)->hparams.n_pos_per_embd()) {
    clear();
}

llama_batch_ext::llama_batch_ext(
        size_t n_tokens_max,
        size_t n_embd_inp,
        size_t n_embd_inp_enc,
        llama_seq_id n_seq_max,
        llama_memory_i * mem,
        llama_token n_vocab,
        size_t n_pos_per_embd) :
        n_tokens_max(n_tokens_max),
        n_embd_inp(n_embd_inp),
        n_embd_inp_enc(n_embd_inp_enc),
        n_embd_state(0),
        n_seq_max(n_seq_max),
        mem(mem),
        n_vocab(n_vocab),
        n_pos_per_embd(n_pos_per_embd) {
    clear();
}

void llama_batch_ext::clear() {
    tokens.clear();
    embd  .clear();
    state .clear();
    n_embd = 0;
}

int32_t llama_batch_ext::add_token(llama_seq_id seq_id) {
    if (tokens.size() >= n_tokens_max) {
        return -1; // size limit reached
    }
    if (seq_id < 0 || seq_id >= n_seq_max) {
        return -3; // invalid sequence id
    }

    // position is left undefined; call set_token_pos() before decoding
    token t;
    t.seq_ids.insert(seq_id);

    tokens.push_back(t);

    return (int32_t)(tokens.size() - 1);
}

bool llama_batch_ext::add_seq(int32_t idx, llama_seq_id seq_id) {
    if (idx < 0 || idx >= (int32_t) tokens.size()) {
        return false;
    }
    if (seq_id < 0 || seq_id >= n_seq_max) {
        return false;
    }

    token & t = tokens[idx];

    t.seq_ids.insert(seq_id);

    return true;
}

bool llama_batch_ext::set_token_id(int32_t idx, llama_token id) {
    if (idx < 0 || idx >= (int32_t) tokens.size()) {
        return false;
    }
    if (id < 0 || id >= n_vocab) {
        return false;
    }
    tokens[idx].id = id;
    return true;
}

bool llama_batch_ext::set_token_embd(int32_t idx, llama_embd embd_in) {
    if (idx < 0 || idx >= (int32_t) tokens.size()) {
        return false;
    }
    if (!embd_in.data) {
        return false;
    }

    const size_t n_total = embd_in.n_rows * embd_in.n_embd;
    if (n_embd == 0) {
        if (n_total != n_embd_inp && n_total != n_embd_inp_enc) {
            LLAMA_LOG_ERROR("%s: embedding size mismatch, got %zu rows x %zu = %zu, expected %zu or %zu\n",
                    __func__, embd_in.n_rows, embd_in.n_embd, n_total, n_embd_inp, n_embd_inp_enc);
            return false;
        }
        n_embd = n_total;
    } else if (n_total != n_embd) {
        LLAMA_LOG_ERROR("%s: embedding size mismatch, got %zu rows x %zu = %zu, expected %zu\n",
                __func__, embd_in.n_rows, embd_in.n_embd, n_total, n_embd);
        return false;
    }

    token & t = tokens[idx];

    if (t.has_embd) {
        LLAMA_LOG_ERROR("%s: embedding for token %d is already set\n", __func__, idx);
        return false;
    }

    t.has_embd = true;
    t.embd_off = embd.size();
    embd.insert(embd.end(), embd_in.data, embd_in.data + n_total);

    return true;
}

bool llama_batch_ext::set_token_state(int32_t idx, llama_embd state_in) {
    if (idx < 0 || idx >= (int32_t) tokens.size()) {
        return false;
    }
    if (!state_in.data) {
        return false;
    }
    if (n_embd_state == 0) {
        return false; // this context does not take state embeddings
    }

    const size_t n_total = state_in.n_rows * state_in.n_embd;
    if (n_total != n_embd_state) {
        LLAMA_LOG_ERROR("%s: state size mismatch, got %zu rows x %zu = %zu, expected %zu\n",
                __func__, state_in.n_rows, state_in.n_embd, n_total, n_embd_state);
        return false;
    }

    token & t = tokens[idx];

    if (t.has_state) {
        LLAMA_LOG_ERROR("%s: state for token %d is already set\n", __func__, idx);
        return false;
    }

    t.has_state = true;
    t.state_off = state.size();
    state.insert(state.end(), state_in.data, state_in.data + n_total);

    return true;
}

bool llama_batch_ext::set_token_pos(int32_t idx, const llama_pos * pos_in) {
    if (idx < 0 || idx >= (int32_t) tokens.size()) {
        return false;
    }
    if (!pos_in) {
        return false;
    }

    token & t = tokens[idx];

    size_t n_pos = t.id != LLAMA_TOKEN_NULL ? 1 : n_pos_per_embd;
    for (size_t i = 0; i < n_pos; ++i) {
        t.pos[i] = pos_in[i];
    }

    return true;
}

bool llama_batch_ext::set_output(int32_t idx, bool output_last) {
    if (idx < 0 || idx >= (int32_t) tokens.size()) {
        return false;
    }
    tokens[idx].output = output_last;
    return true;
}

bool llama_batch_ext::set_decision_order(int32_t idx, int32_t order) {
    if (idx < 0 || idx >= (int32_t) tokens.size()) {
        return false;
    }
    tokens[idx].decision_order = order;
    return true;
}

// llama_batch_ext C API

llama_batch_ext * llama_batch_ext_init(llama_context * ctx) {
    return new llama_batch_ext(ctx);
}

void llama_batch_ext_free(llama_batch_ext * batch) {
    delete batch;
}

void llama_batch_ext_clear(llama_batch_ext * batch) {
    batch->clear();
}

int32_t llama_batch_ext_add(llama_batch_ext * batch, llama_seq_id seq_id) {
    return batch->add_token(seq_id);
}

int32_t llama_batch_ext_add_token(llama_batch_ext * batch, llama_seq_id seq_id, llama_token id) {
    int32_t idx = batch->add_token(seq_id);
    if (idx < 0) {
        return idx;
    }
    if (!batch->set_token_id(idx, id)) {
        return -2;
    }
    return idx;
}

int32_t llama_batch_ext_add_embd(llama_batch_ext * batch, llama_seq_id seq_id, llama_embd embd) {
    int32_t idx = batch->add_token(seq_id);
    if (idx < 0) {
        return idx;
    }
    if (!batch->set_token_embd(idx, embd)) {
        return -2;
    }
    return idx;
}

bool llama_batch_ext_add_seq(llama_batch_ext * batch, int32_t idx, llama_seq_id seq_id) {
    return batch->add_seq(idx, seq_id);
}

bool llama_batch_ext_set_pos(llama_batch_ext * batch, int32_t idx, const llama_pos * pos) {
    return batch->set_token_pos(idx, pos);
}

bool llama_batch_ext_set_embd_token(llama_batch_ext * batch, int32_t idx, llama_embd embd) {
    return batch->set_token_embd(idx, embd);
}

bool llama_batch_ext_set_embd_state(llama_batch_ext * batch, int32_t idx, llama_embd embd) {
    return batch->set_token_state(idx, embd);
}

bool llama_batch_ext_set_output_embd(llama_batch_ext * batch, int32_t idx, bool value) {
    return batch->set_output(idx, value);
}

bool llama_batch_ext_set_output_logits(llama_batch_ext * batch, int32_t idx, bool value) {
    return batch->set_output(idx, value);
}

bool llama_batch_ext_set_decision_order(llama_batch_ext * batch, int32_t idx, llama_decision_order order) {
    return batch->set_decision_order(idx, order);
}

// llama_batch_compat

void llama_batch_compat::init(llama_batch_ext & dst, const llama_batch & batch_inp, size_t n_embd_row) {
    llama_batch_ext * batch_ext = &dst;

    if (n_embd_row == 0) {
        n_embd_row = batch_ext->n_embd_inp;
    }

    // a batch can carry both, for example the MTP hook batches
    const bool has_token = batch_inp.token != nullptr;
    const bool has_embd  = batch_inp.embd  != nullptr;

    static const llama_seq_id default_seq_id    = 0;
    static const int32_t      default_n_seq_id  = 1;

    // auto-generates positions locally when batch_inp.pos is null, continuing from memory
    std::vector<llama_pos> pos_next(batch_ext->n_seq_max);
    for (llama_seq_id s = 0; s < (llama_seq_id) batch_ext->n_seq_max; ++s) {
        pos_next[s] = llama_memory_seq_pos_max(batch_ext->mem, s) + 1; // assume next pos
    }

    for (int32_t i = 0; i < batch_inp.n_tokens; ++i) {
        const int32_t      n_sid = batch_inp.n_seq_id ? batch_inp.n_seq_id[i]    : default_n_seq_id;
        const llama_seq_id * sids = batch_inp.seq_id  ? batch_inp.seq_id[i]      : &default_seq_id;

        llama_batch_ext::token t;

        // seq_ids
        for (int32_t s = 0; s < n_sid; ++s) {
            t.seq_ids.insert(sids[s]);
        }

        // position(s)
        if (batch_inp.pos) {
            if (has_token) {
                // token batch: one position per token
                t.pos[0] = batch_inp.pos[i];
            } else {
                // embedding batch (M-RoPE): section-major layout pos[j*n_tokens + i]
                for (uint32_t j = 0; j < batch_ext->n_pos_per_embd; ++j) {
                    t.pos[j] = batch_inp.pos[(int32_t) j * batch_inp.n_tokens + i];
                }
            }
        } else {
            // auto-generate position from the first seq_id
            t.pos[0] = pos_next[sids[0]]++;
        }

        // token id and/or embeddings
        if (has_token) {
            t.id = batch_inp.token[i];
        }

        // legacy MTP hook batches carry the hidden state next to the token ids
        if (has_embd && has_token && batch_ext->n_embd_state > 0) {
            t.has_state = true;
            t.state_off = batch_ext->state.size();
            const float * src = batch_inp.embd + (size_t) i * batch_ext->n_embd_state;
            batch_ext->state.insert(batch_ext->state.end(), src, src + batch_ext->n_embd_state);
        } else if (has_embd) {
            t.has_embd = true;
            t.embd_off = batch_ext->embd.size();
            const float * src = batch_inp.embd + (size_t) i * n_embd_row;
            batch_ext->embd.insert(batch_ext->embd.end(), src, src + n_embd_row);
            batch_ext->n_embd = n_embd_row;
        }

        // output flag
        // if no logits array is given, default to only the last token being an output
        t.output = batch_inp.logits
            ? (batch_inp.logits[i] != 0)
            : (i == batch_inp.n_tokens - 1);

        batch_ext->tokens.push_back(t);
    }
}

llama_batch_compat::llama_batch_compat(llama_context * ctx, const llama_batch & batch_inp, size_t n_embd_row) {
    batch_ext = new llama_batch_ext(ctx);
    init(*batch_ext, batch_inp, n_embd_row);
}

llama_batch_compat::~llama_batch_compat() {
    delete batch_ext;
}

#ifdef FG153_BATCH_ALLOCR_GOOSE
// ABI-preserving sidecar: no members, no dtor. State is built per call from
// the existing fields, used by Goose, written back, and freed.

bool llama_batch_allocr::fg153_goose_enabled() {
    const char * v = std::getenv("GGML_GOOSE_BATCH_ALLOCR");
    if (v && std::strcmp(v, "0") == 0) { g_fg153_fallbacks++; return false; }
    return true;
}

void llama_batch_allocr::fg153_sync_used(int64_t s) const {
    const int64_t n = batch.n_tokens;
    fg153_ba_call(74, s, 32, n, 0);
    for (int64_t i = 0; i < n; ++i)
        fg153_ba_call(72, s, 32, i, (i < (int64_t) used.size() && used[(size_t) i]) ? 1 : 0);
    fg153_ba_put(s, 6, 0, (int64_t) n_used);
    fg153_ba_call(75, s, 33, 0, 0);
    for (int32_t id : out_ids) fg153_ba_call(73, s, 33, id, 0);
}

void llama_batch_allocr::fg153_import_state(int64_t s) const {
    const int64_t nt = batch.n_tokens;
    fg153_ba_put(s, 0, 0, nt);
    fg153_ba_put(s, 1, 0, (int64_t) n_embd);
    fg153_ba_put(s, 2, 0, (int64_t) n_embd_state);
    fg153_ba_put(s, 3, 0, (int64_t) n_seq_max);
    fg153_ba_put(s, 4, 0, (int64_t) n_pos_per_embd);
    fg153_ba_put(s, 5, 0, (int64_t) n_outputs);
    fg153_ba_put(s, 7, 0, has_cpl ? 1 : 0);
    fg153_ba_put(s, 8, 0, is_embd_vec.empty() ? 0 : 1);
    fg153_ba_put(s, 9, 0, batch.token ? 1 : 0);
    fg153_ba_put(s, 10, 0, batch.embd ? 1 : 0);
    fg153_ba_put(s, 11, 0, state_vec.empty() ? 0 : 1);

    std::vector<int64_t> tok((size_t) nt, 0), posv, nsqv((size_t) nt, 0), seqflat, outv((size_t) nt, 0);
    std::vector<int64_t> offv((size_t) nt + 1, 0);
    for (int64_t i = 0; i < nt; ++i) {
        if (batch.token) tok[(size_t) i] = (int64_t) batch.token[i];
        if (batch.pos) posv.push_back((int64_t) batch.pos[i]);
        if (batch.n_seq_id) nsqv[(size_t) i] = (int64_t) batch.n_seq_id[i];
        if (batch.logits) outv[(size_t) i] = (int64_t) batch.logits[i];
        if (batch.seq_id)
            for (int32_t k = 0; k < nsqv[(size_t) i]; ++k) seqflat.push_back((int64_t) batch.seq_id[i][k]);
        offv[(size_t) i + 1] = (int64_t) seqflat.size();
    }
    auto pv = [&](int64_t key, const auto & src) {
        fg153_ba_call(74, s, key, (int64_t) src.size(), 0);
        for (size_t i = 0; i < src.size(); ++i)
            fg153_ba_call(72, s, key, (int64_t) i, (int64_t) src[i]);
    };
    pv(20, tok); pv(24, posv); pv(25, nsqv); pv(26, seqflat); pv(27, offv);
    pv(28, outv); pv(23, is_embd_vec); pv(29, decision_order); pv(30, seq_id_unq);
    {
        std::vector<int64_t> sx((size_t) LLAMA_MAX_SEQ, -1);
        for (int i = 0; i < LLAMA_MAX_SEQ && i < (int) seq_idx.size(); ++i) sx[(size_t) i] = seq_idx[(size_t) i];
        pv(31, sx);
    }
    if (batch.embd && n_embd > 0) {
        std::vector<float> copy(batch.embd, batch.embd + (size_t) nt * n_embd);
        fg153_ba_import_embd(s, (int64_t) (uintptr_t) copy.data(), (int64_t) copy.size());
    }
    if (!state_vec.empty())
        fg153_ba_import_state(s, (int64_t) (uintptr_t) state_vec.data(), (int64_t) state_vec.size());

    fg153_ba_call(28, s, 0, 0, 0);
    for (int64_t i = 0; i < nt; ++i) {
        fg153_ba_call(23, s, i, 0, 0);
        if (batch.seq_id && batch.n_seq_id)
            for (int32_t k = 0; k < batch.n_seq_id[i]; ++k)
                fg153_ba_call(24, s, i, (int64_t) batch.seq_id[i][k], 0);
        const int64_t slot = fg153_ba_call(29, s, i, 0, 0);
        fg153_ba_call(33, s, slot, i, 0);
    }
    fg153_ba_call(41, s, 0, 0, 0);
    if (batch.pos && batch.seq_id && batch.n_seq_id) {
        for (int64_t i = 0; i < nt; ++i)
            for (int32_t k = 0; k < batch.n_seq_id[i]; ++k)
                fg153_ba_call(36, s, (int64_t) batch.seq_id[i][k], (int64_t) batch.pos[i], 0);
    }
    fg153_ba_call(42, s, 0, 0, 0);
    for (size_t a0 = 0; a0 < seq_cpl.size(); ++a0)
        for (size_t a1 = 0; a1 < seq_cpl[a0].size(); ++a1)
            if (seq_cpl[a0][a1]) fg153_ba_call(43, s, (int64_t) a0, (int64_t) a1, 0);
    fg153_sync_used(s);
}

void llama_batch_allocr::fg153_export_state(int64_t s) {
    const int64_t n = fg153_ba_get(s, 0, 0);
    n_embd=(uint32_t)fg153_ba_get(s,1,0);
    n_embd_state=(uint32_t)fg153_ba_get(s,2,0);
    n_seq_max=(uint32_t)fg153_ba_get(s,3,0);
    has_cpl=fg153_ba_get(s,7,0)!=0;
    is_embd_vec.resize((size_t)fg153_ba_call(70,s,23,0,0));
    for(size_t i=0;i<is_embd_vec.size();++i)is_embd_vec[i]=(int8_t)fg153_ba_call(71,s,23,(int64_t)i,0);
    decision_order.resize((size_t)fg153_ba_call(70,s,29,0,0));
    for(size_t i=0;i<decision_order.size();++i)decision_order[i]=(int32_t)fg153_ba_call(71,s,29,(int64_t)i,0);
    token_vec.assign((size_t) n, 0);
    pos.assign((size_t) (n * (int64_t) n_pos_per_embd), 0);
    n_seq_id.assign((size_t) n, 0);
    output.assign((size_t) n, 0);
    for (int64_t i = 0; i < n; ++i) token_vec[(size_t) i] = (llama_token) fg153_ba_call(71, s, 20, i, 0);
    for (int64_t i = 0; i < n * (int64_t) n_pos_per_embd; ++i) pos[(size_t) i] = (llama_pos) fg153_ba_call(71, s, 24, i, 0);
    for (int64_t i = 0; i < n; ++i) n_seq_id[(size_t) i] = (int32_t) fg153_ba_call(71, s, 25, i, 0);
    for (int64_t i = 0; i < n; ++i) output[(size_t) i] = (int8_t) fg153_ba_call(71, s, 28, i, 0);
    seq_id_data.clear();
    for (int64_t i = 0; i < n; ++i) {
        const int64_t off = fg153_ba_call(71, s, 27, i, 0);
        for (int64_t k = 0; k < n_seq_id[(size_t) i]; ++k)
            seq_id_data.push_back((llama_seq_id) fg153_ba_call(71, s, 26, off + k, 0));
    }
    seq_id.assign((size_t) n, nullptr);
    size_t off = 0;
    for (int64_t i = 0; i < n; ++i) { seq_id[(size_t) i] = seq_id_data.data() + off; off += n_seq_id[(size_t) i]; }
    seq_id.push_back(nullptr);
    const int64_t nuq = fg153_ba_call(70, s, 30, 0, 0);
    seq_id_unq.assign((size_t) nuq, 0);
    for (int64_t i = 0; i < nuq; ++i) seq_id_unq[(size_t) i] = (llama_seq_id) fg153_ba_call(71, s, 30, i, 0);
    seq_idx.assign(LLAMA_MAX_SEQ, -1);
    for (int64_t i = 0; i < (int64_t) n_seq_max && i < LLAMA_MAX_SEQ; ++i)
        seq_idx[(size_t) i] = (int32_t) fg153_ba_call(71, s, 31, i, 0);

    batch.n_tokens = (int32_t) n;
    bool use_token = fg153_ba_get(s, 9, 0) > 0;
    batch.token = use_token ? token_vec.data() : nullptr;
    if (batch.embd) {
        int64_t nf = 0;
        const float * e = fg153_ba_state_embd(s, &nf);
        embd_vec.assign(e, e + nf);
        batch.embd = embd_vec.data();
    }
    if (!state_vec.empty()) {
        int64_t nf = 0;
        const float * st = fg153_ba_state_state(s, &nf);
        state_vec.assign(st, st + nf);
    }
    batch.pos = pos.data();
    batch.n_seq_id = n_seq_id.data();
    batch.seq_id = seq_id.data();
    batch.logits = output.data();
    n_outputs = (uint32_t) fg153_ba_get(s, 5, 0);

    used.assign((size_t) n, false);
    for (int64_t i = 0; i < n; ++i) used[(size_t) i] = fg153_ba_call(71, s, 32, i, 0) != 0;
    n_used = (uint32_t) fg153_ba_get(s, 6, 0);
    out_ids.clear();
    const int64_t no = fg153_ba_call(70, s, 33, 0, 0);
    for (int64_t i = 0; i < no; ++i) out_ids.push_back((int32_t) fg153_ba_call(71, s, 33, i, 0));
}

static llama_ubatch fg153_to_native(int64_t s, int64_t uh) {
    const int64_t nt = fg153_ba_ub_n(uh, 0);
    const int64_t nseq = fg153_ba_ub_n(uh, 1);
    const int64_t npos = fg153_ba_get(s, 4, 0);
    auto data = std::make_shared<llama_ubatch::data_t>();
    data->token.resize((size_t) nt);
    for (int64_t i = 0; i < nt; ++i) data->token[(size_t) i] = (llama_token) fg153_ba_ub_i(uh, 0, i);
    int64_t nf = 0;
    if (const float * e = fg153_ba_ub_embd(uh, &nf); e && nf > 0) data->embd.assign(e, e + nf);
    if (const float * st = fg153_ba_ub_state(uh, &nf); st && nf > 0) data->embd_state.assign(st, st + nf);
    data->pos.resize((size_t) (nt * npos));
    for (int64_t i = 0; i < nt * npos; ++i) data->pos[(size_t) i] = (llama_pos) fg153_ba_ub_i(uh, 1, i);
    data->n_seq_id.resize((size_t) nt);
    int64_t total = 0;
    for (int64_t i = 0; i < nt; ++i) { data->n_seq_id[(size_t) i] = (int32_t) fg153_ba_ub_i(uh, 2, i); total += data->n_seq_id[(size_t) i]; }
    data->seq_id_data.reserve((size_t) total);
    for (int64_t i = 0; i < total; ++i) data->seq_id_data.push_back((llama_seq_id) fg153_ba_ub_i(uh, 9, i));
    data->seq_id.resize((size_t) nt);
    size_t off = 0;
    for (int64_t i = 0; i < nt; ++i) { data->seq_id[(size_t) i] = data->seq_id_data.data() + off; off += data->n_seq_id[(size_t) i]; }
    const int64_t nuq = fg153_ba_ub_n(uh, 7);
    data->seq_id_unq.resize((size_t) nuq);
    for (int64_t i = 0; i < nuq; ++i) data->seq_id_unq[(size_t) i] = (llama_seq_id) fg153_ba_ub_i(uh, 7, i);
    const int64_t nsqidx = fg153_ba_ub_n(uh, 6);
    data->seq_idx.assign(LLAMA_MAX_SEQ, -1);
    for (int64_t i = 0; i < nsqidx && i < (int64_t) data->seq_idx.size(); ++i)
        data->seq_idx[(size_t) i] = (int32_t) fg153_ba_ub_i(uh, 8, i);
    data->output.resize((size_t) nt);
    for (int64_t i = 0; i < nt; ++i) data->output[(size_t) i] = (int8_t) fg153_ba_ub_i(uh, 3, i);
    for (int64_t i = 0; i < nt; ++i) { const int64_t v = fg153_ba_ub_i(uh, 4, i); if (v < 0) break; data->type.push_back((int8_t) v); }
    for (int64_t i = 0; i < nt; ++i) { const int64_t v = fg153_ba_ub_i(uh, 5, i); if (v < 0) break; data->decision_order.push_back((int32_t) v); }
    data->batch_idxs.resize((size_t) nt);
    for (int64_t i = 0; i < nt; ++i) data->batch_idxs[(size_t) i] = (int32_t) fg153_ba_ub_i(uh, 6, i);

    llama_ubatch res{};
    res.b_equal_seqs = (uint32_t) fg153_ba_ub_n(uh, 3);
    res.n_tokens = (uint32_t) nt;
    res.n_seq_tokens = (uint32_t) (nseq > 0 ? nt / nseq : 0);
    res.n_seqs = (uint32_t) nseq;
    res.n_seqs_unq = (uint32_t) nuq;
    res.n_pos = (uint32_t) npos;
    bool use_token = fg153_ba_get(s, 9, 0) > 0;
    if (!data->type.empty()) {
        bool all_embd = true;
        for (int8_t t : data->type) if (!t) { all_embd = false; break; }
        if (all_embd) use_token = false;
    }
    res.token = use_token ? data->token.data() : nullptr;
    res.embd = data->embd.empty() ? nullptr : data->embd.data();
    res.embd_state = data->embd_state.empty() ? nullptr : data->embd_state.data();
    res.pos = data->pos.data();
    res.n_seq_id = data->n_seq_id.data();
    res.seq_id = data->seq_id.data();
    res.seq_id_unq = data->seq_id_unq.data();
    res.seq_idx = data->seq_idx.data();
    res.output = data->output.data();
    res.type = data->type.empty() ? nullptr : data->type.data();
    res.decision_order = data->decision_order.empty() ? nullptr : data->decision_order.data();
    res.data = std::move(data);
    return res;
}

bool llama_batch_allocr::fg153_goose_init(const llama_batch_ext & batch_inp,
                                          const llama_vocab & vocab, bool output_all) {
    clear();
    this->vocab=&vocab;
    n_embd=batch_inp.n_embd>0?batch_inp.n_embd:batch_inp.n_embd_inp;
    n_embd_state=batch_inp.n_embd_state;
    n_seq_max=batch_inp.n_seq_max;
    std::unique_ptr<fg153_ba_state,decltype(&fg153_ba_state_free)> owned(fg153_ba_state_new(),&fg153_ba_state_free);
    const int64_t s=(int64_t)(uintptr_t)owned.get();
    g_fg153_calls++;
    std::vector<int64_t> id, he, hs, out, dec, eo, so2, posv, sf, soff(1, 0);
    const size_t n = batch_inp.tokens.size();
    for (size_t i = 0; i < n; ++i) {
        const auto & t = batch_inp.tokens[i];
        id.push_back((int64_t) t.id);
        he.push_back(t.has_embd ? 1 : 0);
        hs.push_back(t.has_state ? 1 : 0);
        out.push_back(t.output ? 1 : 0);
        dec.push_back((int64_t) t.decision_order);
        eo.push_back((int64_t) t.embd_off);
        so2.push_back((int64_t) t.state_off);
        for (size_t j = 0; j < batch_inp.n_pos_per_embd; ++j)
            posv.push_back((int64_t) t.pos[j < GGML_MROPE_SECTIONS ? j : 0]);
        for (auto sid : t.seq_ids) sf.push_back((int64_t) sid);
        soff.push_back((int64_t) sf.size());
    }
    std::vector<int64_t> mem_min, mem_max;
    if (batch_inp.mem) {
        mem_min.resize((size_t) batch_inp.n_seq_max);
        mem_max.resize((size_t) batch_inp.n_seq_max);
        for (llama_seq_id q = 0; q < batch_inp.n_seq_max; ++q) {
            mem_min[(size_t) q] = batch_inp.mem->seq_pos_min(q);
            mem_max[(size_t) q] = batch_inp.mem->seq_pos_max(q);
        }
    }
    fg153_ba_input in{};
    in.n_tok = (int64_t) n;
    in.n_embd = (int64_t) (batch_inp.n_embd > 0 ? batch_inp.n_embd : batch_inp.n_embd_inp);
    in.n_embd_inp = (int64_t) batch_inp.n_embd_inp;
    in.n_embd_state = (int64_t) batch_inp.n_embd_state;
    in.n_seq_max = (int64_t) batch_inp.n_seq_max;
    in.n_vocab = (int64_t) batch_inp.n_vocab;
    in.n_pos = (int64_t) batch_inp.n_pos_per_embd;
    in.allow_mixed = allow_mixed ? 1 : 0;
    in.tok_id = id.data(); in.tok_has_embd = he.data(); in.tok_has_state = hs.data();
    in.tok_output = out.data(); in.tok_decision = dec.data();
    in.tok_embd_off = eo.data(); in.tok_state_off = so2.data();
    in.tok_pos = posv.data(); in.tok_seq_off = soff.data(); in.tok_seq_flat = sf.data();
    in.embd = batch_inp.embd.empty() ? nullptr : batch_inp.embd.data();
    in.state = batch_inp.state.empty() ? nullptr : batch_inp.state.data();
    in.has_mem = batch_inp.mem ? 1 : 0;
    in.mem_min = mem_min.empty() ? nullptr : mem_min.data();
    in.mem_max = mem_max.empty() ? nullptr : mem_max.data();

    const int64_t ok = fg153_goose_entry(0, s, (int64_t) (uintptr_t) &in, output_all ? 1 : 0, 0, 0, 0);
    this->vocab = &vocab;
    if (ok) {
        batch.token = (n == 0 || batch_inp.tokens[0].id == LLAMA_TOKEN_NULL) ? nullptr : token_vec.data();
        batch.embd = batch_inp.embd.empty() ? nullptr : embd_vec.data();
        fg153_export_state(s);
    }
    return ok != 0;
}

void llama_batch_allocr::fg153_goose_split_reset() {
    std::unique_ptr<fg153_ba_state,decltype(&fg153_ba_state_free)> owned(fg153_ba_state_new(),&fg153_ba_state_free);
    const int64_t s=(int64_t)(uintptr_t)owned.get();
    g_fg153_calls++;
    fg153_import_state(s);
    fg153_goose_entry(5, s, 0, 0, 0, 0, 0);
    fg153_export_state(s);
}

static llama_ubatch fg153_run_split(llama_batch_allocr & a, int mode, uint32_t n_ubatch,
                                    bool sequential, uint32_t keep) {
    std::unique_ptr<fg153_ba_state,decltype(&fg153_ba_state_free)> owned(fg153_ba_state_new(),&fg153_ba_state_free);
    const int64_t s=(int64_t)(uintptr_t)owned.get();
    g_fg153_calls++;
    a.fg153_import_state(s);
    const int64_t uh = fg153_goose_entry(mode, s, n_ubatch, sequential ? 1 : 0, keep, 0, 0);
    llama_ubatch res = uh ? fg153_to_native(s, uh) : llama_ubatch{};
    if (uh) { fg153_ba_ub_release(uh); g_fg153_ubatches++; }
    a.fg153_export_state(s);
    return res;
}

llama_ubatch llama_batch_allocr::fg153_goose_split_simple(uint32_t n_ubatch) { return fg153_run_split(*this, 1, n_ubatch, false, 0); }
llama_ubatch llama_batch_allocr::fg153_goose_split_equal(uint32_t n_ubatch, bool sequential, uint32_t n_keep_tail) { return fg153_run_split(*this, 2, n_ubatch, sequential, n_keep_tail); }
llama_ubatch llama_batch_allocr::fg153_goose_split_seq(uint32_t n_ubatch) { return fg153_run_split(*this, 3, n_ubatch, false, 0); }

llama_ubatch llama_batch_allocr::fg153_goose_ubatch_reserve(uint32_t n_seq_tokens, uint32_t n_seqs) {
    std::unique_ptr<fg153_ba_state,decltype(&fg153_ba_state_free)> owned(fg153_ba_state_new(),&fg153_ba_state_free);
    const int64_t s=(int64_t)(uintptr_t)owned.get();
    g_fg153_calls++;
    clear();
    fg153_ba_put(s,1,0,0);fg153_ba_put(s,2,0,0);
    fg153_ba_put(s,3,0,n_seqs);fg153_ba_put(s,4,0,n_pos_per_embd);
    fg153_ba_put(s,9,0,1);fg153_ba_put(s,10,0,0);fg153_ba_put(s,11,0,0);
    const int64_t uh = fg153_goose_entry(4, s, n_seq_tokens, n_seqs, 0, 0, 0);
    llama_ubatch res = uh ? fg153_to_native(s, uh) : llama_ubatch{};
    if (uh) { fg153_ba_ub_release(uh); g_fg153_ubatches++; }
    return res;
}

void llama_batch_allocr::fg153_install_test(const std::vector<llama_token> & token,
                                            const std::vector<llama_pos> & pos,
                                            const std::vector<int32_t> & nseq,
                                            const std::vector<std::vector<llama_seq_id>> & seq_ids,
                                            const std::vector<int8_t> & logits,
                                            bool embd_all, bool mark_all_used,
                                            uint32_t n_seq_max_, uint32_t n_embd_) {
    clear();
    n_seq_max = n_seq_max_;
    n_embd = n_embd_;
    n_embd_state = 0;
    static thread_local std::vector<std::vector<llama_seq_id>> seq_store_test;
    static thread_local std::vector<llama_seq_id *> seq_ptr_test;
    static thread_local std::vector<float> embd_test;
    seq_store_test = seq_ids;
    seq_ptr_test.resize(seq_store_test.size());
    for (size_t i = 0; i < seq_store_test.size(); ++i) seq_ptr_test[i] = seq_store_test[i].data();
    batch = {};
    batch.n_tokens = (int32_t) token.size();
    batch.token = embd_all ? nullptr : const_cast<llama_token *>(token.data());
    if (embd_all) { embd_test.assign(token.size() * n_embd_, 0.5f); batch.embd = embd_test.data(); }
    batch.pos = const_cast<llama_pos *>(pos.data());
    batch.n_seq_id = const_cast<int32_t *>(nseq.data());
    batch.seq_id = seq_ptr_test.data();
    batch.logits = const_cast<int8_t *>(logits.data());
    if (embd_all) is_embd_vec.assign(token.size(), 1);
    seq_set.assign(seq_ids.size(), seq_set_t());
    seq_set_map.clear();
    has_cpl = false;
    for (size_t i = 0; i < seq_ids.size(); ++i) {
        for (size_t k = 0; k < seq_ids[i].size(); ++k) {
            seq_set[i].set((size_t) seq_ids[i][k]);
            if (k > 0) has_cpl = true;
        }
        seq_set_map[seq_set[i]].push_back((int32_t) i);
    }
    // Native reset (Goose disabled here to keep install deterministic).
    const char * saved = std::getenv("GGML_GOOSE_BATCH_ALLOCR");
    std::string keep_env = saved ? saved : "";
    setenv("GGML_GOOSE_BATCH_ALLOCR", "0", 1);
    split_reset();
    if (!keep_env.empty()) setenv("GGML_GOOSE_BATCH_ALLOCR", keep_env.c_str(), 1);
    else unsetenv("GGML_GOOSE_BATCH_ALLOCR");
    if (mark_all_used) { used.assign(used.size(), true); n_used = (uint32_t) used.size(); }
}

static void fg153_fill_row(const llama_ubatch & u, fg153p_row & r) {
    r.n_tokens = u.n_tokens;
    r.n_seq_tokens = u.n_seq_tokens;
    r.n_seqs = u.n_seqs;
    r.equal = u.equal_seqs() ? 1 : 0;
    r.has_token = u.token ? 1 : 0;
    int32_t nf = 0;
    r.embd_sum = 0.0;
    for (uint32_t i = 0; i < u.n_tokens; ++i) {
        if (u.token) r.token[i] = u.token[i];
        r.n_seq_id[i] = u.n_seq_id ? u.n_seq_id[i] : 0;
        for (int32_t k = 0; k < r.n_seq_id[i] && u.seq_id; ++k) r.seq_flat[nf++] = u.seq_id[i][k];
        if (u.data && i < u.data->batch_idxs.size()) r.batch_idxs[i] = u.data->batch_idxs[i];
    }
    if (u.pos) for (uint32_t i = 0; i < u.n_tokens * u.n_pos; ++i) r.pos[i] = u.pos[i];
    if (u.embd) for (uint32_t i = 0; i < u.n_tokens * 8; ++i) r.embd_sum += u.embd[i];
}

extern "C" {

FG153P_API int fg153p_run_case(int mode, uint32_t n_ubatch, int sequential, uint32_t n_keep_tail,
                    const int64_t * token, const int64_t * pos,
                    const int64_t * n_seq_id, const int64_t * seq_flat,
                    const int64_t * seq_off, const int64_t * logits,
                    int64_t n, int64_t n_seq_max, int embd_all, int mark_all_used,
                    fg153p_row * rows, int max_rows) {
    std::vector<llama_token> tok((size_t) n, 0);
    std::vector<llama_pos> posv((size_t) n, 0);
    std::vector<int32_t> nsq((size_t) n, 0);
    std::vector<int8_t> log((size_t) n, 0);
    std::vector<std::vector<llama_seq_id>> seqs((size_t) n);
    for (int64_t i = 0; i < n; ++i) {
        tok[(size_t) i] = (llama_token) token[i];
        posv[(size_t) i] = (llama_pos) pos[i];
        nsq[(size_t) i] = (int32_t) n_seq_id[i];
        log[(size_t) i] = (int8_t) logits[i];
        for (int64_t k = seq_off[i]; k < seq_off[i + 1]; ++k) seqs[(size_t) i].push_back((llama_seq_id) seq_flat[k]);
    }
    llama_batch_allocr a(1, false);
    a.fg153_install_test(tok, posv, nsq, seqs, log, embd_all != 0, mark_all_used != 0,
                         (uint32_t) n_seq_max, embd_all ? 8u : 0u);
    int count = 0;
    for (int guard = 0; guard < 10000; ++guard) {
        llama_ubatch u{};
        switch (mode) {
            case 0: u = a.split_simple(n_ubatch); break;
            case 1: u = a.split_equal(n_ubatch, sequential != 0, n_keep_tail); break;
            case 2: u = a.split_seq(n_ubatch); break;
        }
        if (u.n_tokens == 0) break;
        if (count >= max_rows) return -2;
        fg153_fill_row(u, rows[count++]);
    }
    return count;
}

FG153P_API int fg153p_init_case(const int64_t * token, const int64_t * pos,
                     const int64_t * n_seq_id, const int64_t * seq_flat,
                     const int64_t * seq_off, const int64_t * logits,
                     int64_t n, int64_t n_seq_max, int64_t n_vocab,
                     int output_all, fg153p_init_out * out) {
    static thread_local llama_vocab vocab;
    llama_batch_ext ext((size_t) n, 8, 8, (llama_seq_id) n_seq_max, nullptr, (llama_token) n_vocab, 1);
    for (int64_t i = 0; i < n; ++i) {
        const int64_t first = seq_off[i] < seq_off[i + 1] ? seq_flat[seq_off[i]] : 0;
        int32_t idx = ext.add_token((llama_seq_id) first);
        if (idx < 0) return -1;
        for (int64_t k = seq_off[i] + 1; k < seq_off[i + 1]; ++k) ext.add_seq(idx, (llama_seq_id) seq_flat[k]);
        ext.set_token_id(idx, (llama_token) token[i]);
        llama_pos p = (llama_pos) pos[i];
        ext.set_token_pos(idx, &p);
        ext.set_output(idx, logits[i] != 0);
    }
    llama_batch_allocr a(1, false);
    out->ok = a.init(ext, vocab, output_all != 0) ? 1 : 0;
    if (out->ok) {
        const llama_batch & b = a.get_batch();
        out->n_tokens = b.n_tokens;
        out->n_outputs = (int32_t) a.get_n_outputs();
        int32_t nf = 0;
        for (int32_t i = 0; i < b.n_tokens; ++i) {
            out->token[i] = b.token ? b.token[i] : 0;
            out->pos[i] = b.pos ? b.pos[i] : 0;
            out->n_seq_id[i] = b.n_seq_id ? b.n_seq_id[i] : 0;
            out->logits[i] = b.logits ? b.logits[i] : 0;
            out->seq_off[i] = nf;
            for (int32_t k = 0; k < out->n_seq_id[i]; ++k) out->seq_flat[nf++] = b.seq_id[i][k];
        }
        out->seq_off[b.n_tokens] = nf;
    }
    return 0;
}

FG153P_API void fg153p_counters(uint64_t * calls, uint64_t * fallbacks, uint64_t * ubatches) {
    if (calls) *calls = g_fg153_calls.load();
    if (fallbacks) *fallbacks = g_fg153_fallbacks.load();
    if (ubatches) *ubatches = g_fg153_ubatches.load();
}

FG153P_API int64_t fg153p_sizeof(void) { return (int64_t) sizeof(llama_batch_allocr); }

}  // extern "C"
#endif  // FG153_BATCH_ALLOCR_GOOSE

