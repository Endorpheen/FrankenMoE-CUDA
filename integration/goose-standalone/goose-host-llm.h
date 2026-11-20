#pragma once
#include <stdint.h>

struct host_s;

// Host API used by both the HTTP server and the shipping binary.
void hose_host_load(const char * path);   // mmap, GGUF metadata, vocabulary, trunk
int64_t hose_tokenize(const unsigned char * text, int64_t len, int64_t * ids, int64_t cap);
int64_t hose_detokenize(const int64_t * ids, int64_t n, unsigned char * out, int64_t cap);
void hose_reset_state(void);
int64_t hose_step(const int32_t * tokens, int64_t count, float ** logits);
int64_t hose_eog_is(int64_t id);
void hose_stats(int64_t * out5);          // vocab, special, eog, eos, layers

// Generate token IDs and detokenise when out_text is set. Optional timing gets
// prefill seconds, decode seconds and the prompt token count.
int64_t hose_generate(const unsigned char * text, int64_t len, int64_t max_tokens,
                      int64_t * out_ids, unsigned char * out_text, int64_t text_cap,
                      double * timing);
