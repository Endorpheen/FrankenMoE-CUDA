#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { HOSE_DEFAULT_CONTEXT = 2048, HOSE_DEFAULT_PREFILL_BATCH = 128 };

// Host API used by both the HTTP server and the shipping binary.
void hose_host_load(const char * path);   // mmap, GGUF metadata, vocabulary, trunk
void hose_host_load_context(const char * path, int64_t capacity);
int64_t hose_tokenize(const unsigned char * text, int64_t len, int64_t * ids, int64_t cap);
int64_t hose_detokenize(const int64_t * ids, int64_t n, unsigned char * out, int64_t cap);
void hose_reset_state(void);
int64_t hose_step(const int32_t * tokens, int64_t count, float ** logits);
int64_t hose_eog_is(int64_t id);
void hose_stats(int64_t * out5);          // vocab, special, eog, eos, layers
int64_t hose_context_capacity(void);
int64_t hose_configure_prefill(int64_t batch, int64_t ubatch);
void hose_prefill_profile(int64_t profile[2]);
int64_t hose_model_residency(int64_t out[4]);  // file bytes, resident bytes, pages, resident pages

// Generate token IDs and detokenise when out_text is set. Optional timing gets
// prefill seconds, decode seconds and the prompt token count.
int64_t hose_generate(const unsigned char * text, int64_t len, int64_t max_tokens,
                      int64_t * out_ids, unsigned char * out_text, int64_t text_cap,
                      double * timing);
int64_t hose_generate_ids(const int64_t * prompt, int64_t count, int64_t max_tokens,
                          bool ignore_eos, int64_t * out_ids, double * timing);
int64_t hose_generate_text(const unsigned char * text, int64_t len, int64_t max_tokens,
                           bool ignore_eos, int64_t * out_ids, double * timing);
