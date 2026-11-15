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

option(FG_GOOSE_METRICS
       "Select the Goose metrics/trace text encoders instead of the native sink formatting"
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

# --- S1.4: metrics/trace text encoders --------------------------------------
#
# The metrics sinks do not live in llama.cpp, so this stage builds them the way the
# runtime does: one shared library from the sink translation units. The pristine
# carrier copies (reference/native/metrics) are a second library and serve as the
# byte oracle; the staged copies under runtime/core/src/metrics are the ones the
# flag switches.
#
#   OFF (default): the native formatting bodies run, the Goose object is neither
#                  compiled nor linked, the bridge defines only the counter stub.
#   ON           : the sinks encode fields and rows through
#                  goose/metrics/metrics-traces.goose -> goose-gen/metrics-traces.c
#                  and common/fg-goose-metrics-bridge.cpp. The native bodies stay
#                  compiled in the same file behind #else.
#
# The staged language is C++17; llama.cpp sets its own standard per target.

# The byte oracle: the carrier's sinks exactly as production compiles them.
add_library(fg-metrics-base SHARED
    reference/native/metrics/decode_trace_sink.cpp
    reference/native/metrics/route_trace_sink.cpp)
target_include_directories(fg-metrics-base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/runtime/core/include)

# The staged sinks plus the bridge; the flag picks the encoders inside them.
add_library(fg-bmoe-metrics SHARED
    runtime/core/src/metrics/decode_trace_sink.cpp
    runtime/core/src/metrics/route_trace_sink.cpp
    common/fg-goose-metrics-bridge.cpp)
target_include_directories(fg-bmoe-metrics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/runtime/core/include)
target_include_directories(fg-bmoe-metrics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
set_target_properties(fg-bmoe-metrics PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

if(FG_GOOSE_METRICS)
    add_library(fg-goose-metrics OBJECT goose-gen/metrics-traces.c)
    set_target_properties(fg-goose-metrics PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(fg-goose-metrics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    # Generated code: keep the host warning set out of it, and keep every mt_*
    # symbol out of the library's dynamic table (the external ABI must not move).
    target_compile_options(fg-goose-metrics PRIVATE -w -fvisibility=hidden)

    target_compile_definitions(fg-bmoe-metrics PRIVATE FG_GOOSE_METRICS=1)
    target_include_directories(fg-bmoe-metrics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_link_libraries(fg-bmoe-metrics PRIVATE fg-goose-metrics)
endif()

# --- gate -------------------------------------------------------------------

add_executable(fg-goose-smoke tests/fg_goose_smoke.cpp)
target_link_libraries(fg-goose-smoke PRIVATE llama-common)
if(FG_GOOSE_JSON_SCHEMA)
    target_compile_definitions(fg-goose-smoke PRIVATE FG_GOOSE_JSON_SCHEMA=1)
endif()

# S1.4 gate: the same source linked against the staged library and the oracle
# library. The reference binary has no bridge, so it reports goose_calls=n/a.
add_executable(fg-goose-metrics-smoke tests/fg_goose_metrics_smoke.cpp)
target_link_libraries(fg-goose-metrics-smoke PRIVATE fg-bmoe-metrics)
target_include_directories(fg-goose-metrics-smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
if(FG_GOOSE_METRICS)
    target_compile_definitions(fg-goose-metrics-smoke PRIVATE FG_GOOSE_METRICS=1)
endif()

add_executable(fg-goose-metrics-smoke-base tests/fg_goose_metrics_smoke.cpp)
target_link_libraries(fg-goose-metrics-smoke-base PRIVATE fg-metrics-base)
target_compile_definitions(fg-goose-metrics-smoke-base PRIVATE FG_METRICS_NO_COUNTER=1)

# Prints the selected grammar text per corpus schema; the base and OFF builds
# must print the same text (switch-off parity).
add_executable(fg-goose-dump tests/fg_goose_dump.cpp)
target_link_libraries(fg-goose-dump PRIVATE llama-common)

message(STATUS "FG_GOOSE_JSON_SCHEMA = ${FG_GOOSE_JSON_SCHEMA}")
message(STATUS "FG_GOOSE_METRICS = ${FG_GOOSE_METRICS}")
