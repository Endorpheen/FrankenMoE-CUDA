// Speculative-decoding decisions (staged S2.2 trial): the model-free carrier of the
// per-step policy that runtime/core/src/engine/session.cpp owns.
//
//   OFF (default): every fg_spec::* entry runs the native body below its *_native twin.
//   ON           : draft source, accept, emit bound, step plan, rollback, advance and the
//                  end-of-turn trim run inside the Goose module
//                  goose/misc/spec-decode-decisions.goose (exports sdp_*).
//
// The *_native functions are compiled in BOTH configurations and are never deleted, so
// switching the flag off is a rebuild, not a revert. They are also the oracle: the gate
// compares the dispatched result against the native one in the same process.
//
// Not switched, on purpose: logits and argmax, the end-of-generation test itself
// (llama_vocab_is_eog), the KV operations, the MTP block and its confidence
// (sdp_mtp_* is composed into the same unit but unwired), and the llama.cpp verify batch.
// Everything here is integer policy over token ids, positions and widths.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fg_spec {

// The accept decision (session.cpp:1772-1800). `confirmed` is the emitted prefix: `tok`
// itself, then every accepted draft that was not an end-of-generation token.
struct accept_t {
    long long n_acc     = 0; // accepted drafts, in the KV whether or not the caller sees them
    long long confirmed = 1; // tokens the caller receives (eog hit is never one of them)
    long long eog_hit   = 0; // an accepted token was end-of-generation
    long long stop      = 0; // index the loop stopped at; n_draft when it ran to the end
    long long n_draft   = 0;
};

// One step's budget (session.cpp:1675-1740).
struct plan_t {
    long long room    = 0; // n_predict - n_gen - 1
    long long n_max   = 0; // the cap handed to the draft source
    long long call    = 0; // the source is asked at all
    long long wide    = 0; // the verify batch carries drafts
    long long n_draft = 0; // drafts after the defensive cap
    long long width   = 1; // verify width: 1, or 1 + n_draft
};

// The rejected-tail carve (session.cpp:1821-1826).
struct rollback_t {
    long long needed   = 0;
    long long keep     = 0; // first dropped position: n_past + 1 + n_acc
    long long rejected = 0;
};

// Where the next step starts and which logits row seeds it (session.cpp:1888-1889).
struct advance_t {
    long long n_past = 0;
    long long row    = -1;
};

// The end-of-turn trim (session.cpp:1899-1912).
struct trim_t {
    long long emitted_end = 0;
    long long removed     = 0;
};

// ---- native bodies: the pre-integration code, always compiled ------------------------------

int ngram_draft_native(const std::vector<int32_t> & ctx, int32_t last, int n_max,
                       int min_match, int max_match, std::vector<int32_t> & out);

accept_t accept_native(const std::vector<int32_t> & drafts, const std::vector<int32_t> & target,
                       const std::vector<int32_t> & eog, int32_t tok,
                       std::vector<int32_t> * confirmed);

long long emit_count_native(long long n_predict, long long n_gen, long long confirmed);

plan_t step_plan_native(long long n_predict, long long n_gen, long long draft_max,
                        long long spec_on, long long n_draft_in);

rollback_t rollback_native(long long n_past, long long n_draft, long long n_acc);

advance_t advance_native(long long n_past, long long n_acc, long long wide);

trim_t tail_trim_native(long long n_prompt, long long n_gen, long long kv_end);

// ---- dispatched: the selected path, native or Goose ---------------------------------------

int ngram_draft(const std::vector<int32_t> & ctx, int32_t last, int n_max,
                int min_match, int max_match, std::vector<int32_t> & out);

accept_t accept(const std::vector<int32_t> & drafts, const std::vector<int32_t> & target,
                const std::vector<int32_t> & eog, int32_t tok,
                std::vector<int32_t> * confirmed);

long long emit_count(long long n_predict, long long n_gen, long long confirmed);

plan_t step_plan(long long n_predict, long long n_gen, long long draft_max,
                 long long spec_on, long long n_draft_in);

rollback_t rollback(long long n_past, long long n_draft, long long n_acc);

advance_t advance(long long n_past, long long n_acc, long long wide);

trim_t tail_trim(long long n_prompt, long long n_gen, long long kv_end);

// How many decisions were taken by the Goose module in this process. 0 in the OFF build,
// where the bridge is not compiled and not linked.
long long goose_calls();

// ---- the loop: a scripted model, so the decisions can be observed end to end --------------
//
// T(pos) is the model's own continuation and an artificial sender emits
// T(n_past + i + 1) at verify row i. Acceptance is therefore the real rule and the emitted
// sequence must equal plain greedy decoding; no approximation enters, and no llama.cpp,
// model, KV or GPU is involved. KV is a counter (rows stored, exclusive).
struct script_t {
    std::vector<int32_t> tokens;
    int32_t eog = 999;
};

struct run_out {
    std::vector<int32_t> emitted;
    // Per step: n_draft, n_acc, wide, keep, logits row.
    std::vector<long long> trace;
    long long n_past        = 0;
    long long kv_end        = 0;
    long long drafted       = 0;
    long long accepted      = 0;
    long long drafted_steps = 0;
    long long decodes       = 0;
    long long trims         = 0;
    long long eog_hits      = 0;
    long long final_end     = 0;
};

run_out run(const script_t & sc, long long n_prompt, long long n_predict, int draft_max,
            int min_match, bool spec_on);

// The same loop with every decision forced through the native bodies. Comparing the two
// transcripts inside one build is the end-to-end differential of this slice.
run_out run_native(const script_t & sc, long long n_prompt, long long n_predict, int draft_max,
                   int min_match, bool spec_on);

} // namespace fg_spec
