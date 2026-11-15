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
// Two sites keep a native arm and therefore a switch:
//
//   FG_GOOSE_SERVER_TOOLS_NATIVE  built-in tool dispatch (the find_tool loop)
//   FG_GOOSE_SERVER_CORS_NATIVE   Origin localhost test (the three compares)
//
// Three sites have no switch left: each lost it together with the native body
// its differential gate had already certified. The model status site went first
// (EXP661), then the asset content type and the Cache-Control text (EXP662). A
// build without the unit answers the documented stub for those three, and is a
// unit-isolation build, not a product one.

#pragma once

// Origin localhost test (server-http.cpp, fg_goose_origin_is_localhost).
#define FG_GOOSE_ENV_CORS_NATIVE "FG_GOOSE_SERVER_CORS_NATIVE"

// Built-in tool dispatch (server-tools.cpp, find_tool).
#define FG_GOOSE_ENV_TOOLS_NATIVE "FG_GOOSE_SERVER_TOOLS_NATIVE"

#ifdef __cplusplus
extern "C" {
#endif

// 1 when the site must use its native body, 0 when the Goose module answers
// first. Default 0.
int fg_goose_cors_native(void);
int fg_goose_tools_native(void);

#ifdef __cplusplus
}
#endif
