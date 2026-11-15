// Runtime switches for the P8-12 server call sites.
//
// AGENTS.md rule 10: an accepted site is on by default and its flag exists only
// to turn the improvement off. The accessor reads its variable once, on first
// use, and caches the answer, so a request never pays for getenv and a decision
// cannot change under a running server.
//
// The value is read as a boolean: unset, empty, "0", "false", "no" and "off"
// keep the Goose path; any other value selects the native body. There is no CLI
// knob because the server's options are one JSON schema; a new key would change
// the schema for a diagnostic that only a build or a bisect needs.
//
// One site is left here. The model status site had a switch of the same shape,
// FG_GOOSE_SERVER_STATUS_NATIVE, and it was removed together with its native
// body once the differential gate went green: with the 7-case switch deleted
// there is nothing left for a flag to restore. Its compile-time option remains,
// as the unit-isolation build.

#pragma once

// Origin localhost test (server-http.cpp, fg_goose_origin_is_localhost).
#define FG_GOOSE_ENV_CORS_NATIVE "FG_GOOSE_SERVER_CORS_NATIVE"

#ifdef __cplusplus
extern "C" {
#endif

// 1 when the site must use its native body, 0 when the Goose module answers
// first. Default 0.
int fg_goose_cors_native(void);

#ifdef __cplusplus
}
#endif
