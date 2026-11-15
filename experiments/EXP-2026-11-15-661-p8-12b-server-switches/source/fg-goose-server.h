// The five server call sites of P8-12, behind one C interface.
//
// Every entry point answers with a byte count on success and -1 when the Goose
// unit is not compiled in (FG_GOOSE_SERVER_RUNTIME off) or cannot answer, so a
// call site always keeps its native body as the fallback. The native body is
// compiled in both configurations; only the early return is conditional.
//
// The wrappers take plain pointers and lengths: the Goose unit's slices are
// (data, len) pairs, and the call sites are C++ strings.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cache-Control policy ids, equal to WEB_POLICY_* in web-assets.goose.
#define FGS_POLICY_IMMUTABLE  0
#define FGS_POLICY_REVALIDATE 1
#define FGS_POLICY_NOCACHE    2

// model-manager: mg_status_str (server-models.h, server_model_status_to_string)
int64_t fgs_model_status_text(int64_t status, char * out, int64_t cap);

// tools-pure: tool_index (server-tools.cpp, find_tool)
int64_t fgs_tool_index(const char * name, int64_t len);

// mime-table: mime_of + mime_text (server-http.cpp, static asset serving)
int64_t fgs_mime_text(const char * name, int64_t len, char * out, int64_t cap);

// web-assets: web_cache_text (server-http.cpp, static asset serving)
int64_t fgs_cache_text(int64_t policy, char * out, int64_t cap);

// cors-policy: cors_is_localhost (server-http.cpp, origin_is_localhost)
int64_t fgs_cors_localhost(const char * host, int64_t len, int64_t parsed);

// Starts the composed unit on the calling thread, with the process arguments.
// The unit keeps process globals (gs_globals_main) and every export panics until
// it is started, so a consumer calls this once at startup, before the first
// switched decision; llama-server does it as the first statement of
// llama_server(). The wrappers also start the unit lazily with no arguments, as
// the fallback for a consumer that has no startup hook (llama-cli links the
// merged queue too). Idempotent and thread-safe: the first call wins.
void fgs_init_early(int argc, char ** argv);

#ifdef __cplusplus
}
#endif
