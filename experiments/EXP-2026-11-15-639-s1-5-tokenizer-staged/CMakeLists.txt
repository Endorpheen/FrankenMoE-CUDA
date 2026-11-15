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

option(FG_GOOSE_TOKENIZER
       "Select the Goose tokenizer decisions (vocab type, special partition, strip) instead of the native bodies"
       OFF)

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

# --- the tokenizer decisions (S1.5) -----------------------------------------
#
# One composed unit (goose-gen/tokenizer-decisions.c) holds three decisions:
#   vt_dispatch      vocab type and pre-tokenizer, from src/llama-vocab.cpp
#   stp_partition    special-token partition, from src/llama-vocab.cpp
#   cs_strip         string_strip, from common/common.cpp
#
# The generated C lives in libllama (one object, one definition per export);
# libllama-common links it and calls the strip entry through the bridge. The
# native bodies stay compiled in both configurations.
if(FG_GOOSE_TOKENIZER)
    add_library(fg-goose-tokenizer OBJECT goose-gen/tokenizer-decisions.c)
    set_target_properties(fg-goose-tokenizer PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(fg-goose-tokenizer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    # Generated code: keep llama.cpp's warning set out of it. Hidden visibility
    # keeps the module's exports out of libllama's dynamic symbol table: the
    # only Goose names a consumer can see are the bridge ones, and those are
    # C++ internal names.
    target_compile_options(fg-goose-tokenizer PRIVATE -w -fvisibility=hidden)

    target_sources(llama PRIVATE llama.cpp/src/fg-goose-tokenizer-bridge.cpp)
    target_include_directories(llama PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_compile_definitions(llama PRIVATE FG_GOOSE_TOKENIZER=1)
    target_link_libraries(llama PRIVATE fg-goose-tokenizer)

    target_compile_definitions(llama-common PRIVATE FG_GOOSE_TOKENIZER=1)
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

message(STATUS "FG_GOOSE_JSON_SCHEMA = ${FG_GOOSE_JSON_SCHEMA}")
message(STATUS "FG_GOOSE_TOKENIZER = ${FG_GOOSE_TOKENIZER}")
