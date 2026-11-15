# Staged S1.1 build: llama.cpp (CPU only) with one Goose module behind a flag.
#
#   OFF (default): the native json-schema-to-grammar.cpp body is selected, the
#                  Goose object is not compiled and not linked.
#   ON           : json_schema_to_grammar() dispatches to the Goose module
#                  (goose/json/format-grammar.goose -> goose-gen/json-schema-grammar.c)
#                  through common/fg-goose-schema-bridge.cpp. The native body
#                  stays compiled under json_schema_to_grammar_native().
#
# Nothing here touches the production tree: this file, tests/ and goose-gen/
# live only in the staged copy.
cmake_minimum_required(VERSION 3.21)

project(fg-goose-staged LANGUAGES C CXX)

option(FG_GOOSE_JSON_SCHEMA
       "Select the Goose json-schema-to-grammar module instead of the native converter"
       OFF)

# S2.2: the speculative-decoding decisions. OFF (default) runs the native bodies, ON runs
# goose/misc/spec-decode-decisions.goose (exports sdp_*) through common/fg-goose-spec-bridge.cpp.
# The native bodies stay compiled in both configurations; the flag only selects the path.
option(FG_GOOSE_SPEC
       "Select the Goose speculative-decoding decisions (sdp_*) instead of the native ones"
       OFF)

# The repository gate convention: the gate runs at -O1, mutation runs override this to -O0.
set(FG_GATE_OPT "-O1" CACHE STRING "Optimization level of the spec gate and the Goose object")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# llama.cpp scope: CPU only, libraries only, no server, no model tools, no
# network, no LLGuidance. These are the same switches the base build uses.
set(LLAMA_BUILD_COMMON   ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP      OFF CACHE BOOL "" FORCE)
set(LLAMA_OPENSSL        OFF CACHE BOOL "" FORCE)
set(LLAMA_SUBPROCESS     OFF CACHE BOOL "" FORCE)
set(LLAMA_LLGUIDANCE     OFF CACHE BOOL "" FORCE)
set(GGML_CUDA            OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE          ON  CACHE BOOL "" FORCE)

add_subdirectory(llama.cpp)

# --- the one Goose module ---------------------------------------------------

if(FG_GOOSE_JSON_SCHEMA)
    # Generated C is one translation unit and one object of its own; the Goose
    # runtime is self-contained (no host hook is needed by this module).
    add_library(fg-goose-json-schema OBJECT goose-gen/json-schema-grammar.c)
    set_target_properties(fg-goose-json-schema PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(fg-goose-json-schema PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    # Generated code: keep llama.cpp's warning set out of it.
    target_compile_options(fg-goose-json-schema PRIVATE -w)

    target_sources(llama-common PRIVATE common/fg-goose-schema-bridge.cpp)
    target_compile_definitions(llama-common PRIVATE FG_GOOSE_JSON_SCHEMA=1)
    target_include_directories(llama-common PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_link_libraries(llama-common PRIVATE fg-goose-json-schema)
endif()

# --- gate -------------------------------------------------------------------

add_executable(fg-goose-smoke tests/fg_goose_smoke.cpp)
target_link_libraries(fg-goose-smoke PRIVATE llama-common)
if(FG_GOOSE_JSON_SCHEMA)
    target_compile_definitions(fg-goose-smoke PRIVATE FG_GOOSE_JSON_SCHEMA=1)
endif()

# Prints the selected grammar text per corpus schema; the base and OFF builds
# must print the same text (switch-off parity).
add_executable(fg-goose-dump tests/fg_goose_dump.cpp)
target_link_libraries(fg-goose-dump PRIVATE llama-common)

# --- S2.2: the speculative-decoding decisions ---------------------------------
#
# A model-free library: no llama.cpp, no ggml, no CUDA and no model on its link line. It
# carries the native decision bodies, the dispatch, and the loop model. runtime/core/src/
# engine/ngram_draft.cpp is the shipped draft source, compiled unchanged.
add_library(fg-spec-core SHARED
    common/fg-spec-decide.cpp
    runtime/core/src/engine/ngram_draft.cpp)
target_include_directories(fg-spec-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/core/include)
target_compile_options(fg-spec-core PRIVATE ${FG_GATE_OPT})

if(FG_GOOSE_SPEC)
    add_library(fg-goose-spec OBJECT goose-gen/spec-decide.c)
    set_target_properties(fg-goose-spec PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(fg-goose-spec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_compile_options(fg-goose-spec PRIVATE ${FG_GATE_OPT} -w)
    target_sources(fg-spec-core PRIVATE common/fg-goose-spec-bridge.cpp)
    target_compile_definitions(fg-spec-core PRIVATE FG_GOOSE_SPEC=1)
    target_include_directories(fg-spec-core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_link_libraries(fg-spec-core PRIVATE fg-goose-spec)
endif()

add_executable(fg-spec-smoke tests/fg_spec_smoke.cpp)
target_link_libraries(fg-spec-smoke PRIVATE fg-spec-core)
if(FG_GOOSE_SPEC)
    target_compile_definitions(fg-spec-smoke PRIVATE FG_GOOSE_SPEC=1)
endif()

message(STATUS "FG_GOOSE_JSON_SCHEMA = ${FG_GOOSE_JSON_SCHEMA}")
message(STATUS "FG_GOOSE_SPEC = ${FG_GOOSE_SPEC}")
