# Staged S1.3 build: llama.cpp (CPU only) with the Goose P7-04 quant stack
# behind one flag.
#
#   OFF (default): ggml-quants.c and ggml.c run their own bodies; the Goose
#                  object is not compiled and not linked; libggml-base.so
#                  contains no Goose symbol.
#   ON           : three entry points dispatch into the composed Goose unit
#                  (goose-gen/quant-stack.c, generated from
#                  goose-gen/quant-stack.goose, itself composed from
#                  goose/model/quant-*.goose):
#                    dequantize_row_q4_0()   -> fg_goose_quant_dequant_row_q4_0()
#                    quantize_row_q8_0_ref() -> fg_goose_quant_quantize_row_q8_0()
#                    ggml_blck_size()        -> fg_goose_quant_blck_size()
#                  The native bodies stay compiled in both configurations and
#                  answer every refusal from the module.
#
# Nothing here touches the production tree: this file, common/, goose/,
# goose-gen/, tests/ and scripts/ live only in the staged copy.
cmake_minimum_required(VERSION 3.21)

project(fg-goose-staged LANGUAGES C CXX)

option(FG_GOOSE_JSON_SCHEMA
       "Select the Goose json-schema-to-grammar module instead of the native converter"
       OFF)

option(FG_GOOSE_QUANT
       "Select the Goose P7-04 quant stack for three ggml entry points"
       OFF)

# Mutation runs compile the Goose object at -O0 while the rest of the tree
# stays at the Release optimisation level, matching the repository's rule
# "gates at -O1, mutations at -O0".
option(FG_GOOSE_QUANT_O0
       "Compile the Goose quant object at -O0 (mutation runs only)"
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

# --- module 1: the schema-to-grammar converter (S1.1, EXP626) ---------------

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

# --- module 2: the quant stack (S1.3) ---------------------------------------

if(FG_GOOSE_QUANT)
    # One translation unit, one object. The module needs host primitives (load,
    # store, libm, IQ tables), which common/fg-goose-quant-bridge.c provides.
    add_library(fg-goose-quant OBJECT goose-gen/quant-stack.c)
    set_target_properties(fg-goose-quant PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(fg-goose-quant PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_compile_options(fg-goose-quant PRIVATE -w)
    if(FG_GOOSE_QUANT_O0)
        target_compile_options(fg-goose-quant PRIVATE -O0)
    endif()

    # The bridge lives inside ggml-base, next to ggml-quants.c, because the IQ
    # tables it exposes are file-static there.
    target_sources(ggml-base PRIVATE common/fg-goose-quant-bridge.c)
    target_compile_definitions(ggml-base PRIVATE FG_GOOSE_QUANT=1)
    target_include_directories(ggml-base PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
    target_link_libraries(ggml-base PRIVATE fg-goose-quant)
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

# Quant gate (S1.3): byte comparison of the selected path against the native
# body, plus the dispatch counters. Needs no model, no server, no GPU.
add_executable(fg-goose-quant-smoke tests/fg_goose_quant_smoke.c)
target_link_libraries(fg-goose-quant-smoke PRIVATE ggml)
if(UNIX)
    target_link_libraries(fg-goose-quant-smoke PRIVATE m)
endif()
# ggml-quants.h is an internal header; the gate needs the block structs.
target_include_directories(fg-goose-quant-smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/ggml/src)
# The gate reports the counters; with the flag off the header inlines them as 0.
target_include_directories(fg-goose-quant-smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
if(FG_GOOSE_QUANT)
    target_compile_definitions(fg-goose-quant-smoke PRIVATE FG_GOOSE_QUANT=1)
endif()

# Deep arm gate (S1.3): the chunk and dequant entries behind the switched
# ones, against the native library. Needed so that a mutation has somewhere to
# fail; it does not change the smoke gate's numbers.
add_executable(fg-goose-quant-arms tests/fg_goose_quant_arms.c)
target_link_libraries(fg-goose-quant-arms PRIVATE ggml)
if(UNIX)
    target_link_libraries(fg-goose-quant-arms PRIVATE m)
endif()
target_include_directories(fg-goose-quant-arms PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen
    ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/ggml/src)
if(FG_GOOSE_QUANT)
    target_compile_definitions(fg-goose-quant-arms PRIVATE FG_GOOSE_QUANT=1)
endif()

message(STATUS "FG_GOOSE_JSON_SCHEMA = ${FG_GOOSE_JSON_SCHEMA}")
message(STATUS "FG_GOOSE_QUANT       = ${FG_GOOSE_QUANT}")
