# Staged S1.1 + S3.2 build: llama.cpp (CPU only) with Goose modules behind flags.
#
#   FG_GOOSE_JSON_SCHEMA (S1.1)
#   OFF (default): the native json-schema-to-grammar.cpp body is selected, the
#                  Goose object is not compiled and not linked.
#   ON           : json_schema_to_grammar() dispatches to the Goose module
#                  (goose/json/format-grammar.goose -> goose-gen/json-schema-grammar.c)
#                  through common/fg-goose-schema-bridge.cpp. The native body
#                  stays compiled under json_schema_to_grammar_native().
#
#   FG_GOOSE_SESSION (S3.2)
#   OFF (default): common/fg-session-decide.cpp calls its own *_native bodies,
#                  the session Goose object is not compiled and not linked.
#   ON           : every session decision (ss_/rts_/nd_) dispatches to the composed
#                  module (goose/misc/{session-lifecycle,runtime-facade,draft-source}.goose
#                  -> goose-gen/session.goose -> goose-gen/session.c) through
#                  common/fg-goose-session-bridge.cpp. The native bodies stay
#                  compiled and reachable as *_native.
#
# Nothing here touches the production tree: this file, tests/ and goose-gen/
# live only in the staged copy.
cmake_minimum_required(VERSION 3.21)

project(fg-goose-staged LANGUAGES C CXX)

option(FG_GOOSE_JSON_SCHEMA
       "Select the Goose json-schema-to-grammar module instead of the native converter"
       OFF)
option(FG_GOOSE_SESSION
       "Select the Goose session/runtime/draft-source decisions instead of the native ones"
       OFF)
set(FG_GATE_OPT "-O1" CACHE STRING "Optimization level of the session gate and its Goose object")

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

# --- the session decisions (S3.2) -------------------------------------------

# The carrier is always built: the native bodies plus the dispatch. With the flag on the
# bridge and one Goose object are added to the same library, so switching the flag is a
# rebuild, not a revert, and nothing native is removed.
add_library(fg-session-core SHARED common/fg-session-decide.cpp)
target_include_directories(fg-session-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_compile_options(fg-session-core PRIVATE ${FG_GATE_OPT})

if(FG_GOOSE_SESSION)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen/session.c)
        message(FATAL_ERROR
            "FG_GOOSE_SESSION=ON needs goose-gen/session.c; run scripts/session-gate.sh")
    endif()
    add_library(fg-goose-session OBJECT goose-gen/session.c)
    set_target_properties(fg-goose-session PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_compile_options(fg-goose-session PRIVATE ${FG_GATE_OPT} -w)
    target_include_directories(fg-goose-session PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)

    target_sources(fg-session-core PRIVATE common/fg-goose-session-bridge.cpp)
    target_compile_definitions(fg-session-core PRIVATE FG_GOOSE_SESSION=1)
    target_link_libraries(fg-session-core PRIVATE fg-goose-session)
endif()

# --- gate -------------------------------------------------------------------

add_executable(fg-session-smoke tests/fg_session_smoke.cpp)
target_link_libraries(fg-session-smoke PRIVATE fg-session-core)
if(FG_GOOSE_SESSION)
    target_compile_definitions(fg-session-smoke PRIVATE FG_GOOSE_SESSION=1)
endif()

# --- gate (S1.1) ------------------------------------------------------------

add_executable(fg-goose-smoke tests/fg_goose_smoke.cpp)
target_link_libraries(fg-goose-smoke PRIVATE llama-common)
if(FG_GOOSE_JSON_SCHEMA)
    target_compile_definitions(fg-goose-smoke PRIVATE FG_GOOSE_JSON_SCHEMA=1)
endif()

# Prints the selected grammar text per corpus schema; the base and OFF builds
# must print the same text (switch-off parity).
add_executable(fg-goose-dump tests/fg_goose_dump.cpp)
target_link_libraries(fg-goose-dump PRIVATE llama-common)

message(STATUS "FG_GOOSE_JSON_SCHEMA = ${FG_GOOSE_JSON_SCHEMA}")
message(STATUS "FG_GOOSE_SESSION = ${FG_GOOSE_SESSION}")
