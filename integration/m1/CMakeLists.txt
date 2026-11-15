# M1 of P11-01: one Apex-capable runtime, ten Goose stages behind ten flags.
#
# Every flag defaults ON (rule 10): a plain `cmake -B build` gets the ten Goose
# stages, and each flag exists only to turn one of them off for a bisect, an A/B
# or a rollback. A build with any flag off is a diagnostic or isolation build,
# not the shipped one.
#
# ON compiles the ONE composed unit (goose-gen/main.c) and the ONE host adapter
# (host/goose_host.c) exactly once, and every stage bridge calls into them.
# There is one goose_init in the whole link.
#
# The unit and the adapter are one static library, `fg-goose-runtime`. It needs
# nothing from ggml: the 14 IQ table views the unit declares dispatch through a
# descriptor installed at start-up, and the provider that owns the file-static
# tables (ggml-quants.c) is referenced weakly. The edge is one-way,
# ggml-base -> fg-goose-runtime, so no static-library cycle exists and no
# --start-group is needed.
#
# This project is tracked at integration/m1; tools/stage-m1-tree.sh assembles
# the staged tree from it, upstream/llama.cpp and goose/.

cmake_minimum_required(VERSION 3.21)

project(fg-goose-apex-m1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- the ten stage flags, all ON by default ----------------------------------
#
# These are not opt-in switches: they exist so a bisect, an A/B or a rollback can
# take one stage back to its native body. Set one to OFF to disable that stage.

option(FG_GOOSE_JSON_SCHEMA "S1.1 schema-to-GBNF converter"                         ON)
option(FG_GOOSE_GGUF        "S1.2 GGUF writer and split plan"                       ON)
option(FG_GOOSE_QUANT       "S1.3 quant stack (dequant/quantize/blck)"              ON)
option(FG_GOOSE_METRICS     "S1.4 metrics and trace text encoders"                  ON)
option(FG_GOOSE_TOKENIZER   "S1.5 tokenizer decisions (vocab type/partition/strip)" ON)
option(FG_GOOSE_STREAM      "S1.6 stream decisions (xl/dw/ov/rs)"                   ON)
option(FG_GOOSE_CONFIG      "S2.1 config validator"                                 ON)
option(FG_GOOSE_SPEC        "S2.2 speculative-decoding decisions"                   ON)
option(FG_GOOSE_MODELS      "S3.1 model-management decisions"                       ON)
option(FG_GOOSE_SESSION     "S3.2 session/runtime/draft-source decisions"           ON)

set(FG_GOOSE_RUNTIME OFF)
foreach(flag
        FG_GOOSE_JSON_SCHEMA FG_GOOSE_GGUF FG_GOOSE_QUANT FG_GOOSE_METRICS
        FG_GOOSE_TOKENIZER FG_GOOSE_STREAM FG_GOOSE_CONFIG FG_GOOSE_SPEC
        FG_GOOSE_MODELS FG_GOOSE_SESSION)
    # option() treats every non-false value as ON, so `-DFG_GOOSE_X=bogus` would
    # silently enable a stage. Reject the value instead of guessing (P11-05).
    if(DEFINED ${flag} AND NOT "${${flag}}" MATCHES "^(ON|OFF|TRUE|FALSE|YES|NO|1|0)$")
        message(FATAL_ERROR "${flag} must be ON or OFF, got '${${flag}}'")
    endif()
    if(${flag})
        set(FG_GOOSE_RUNTIME ON)
    endif()
endforeach()

# --- llama.cpp scope: CPU only, static libraries, the server linked ----------

set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON    ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER    ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS     ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP       OFF CACHE BOOL "" FORCE)
set(LLAMA_OPENSSL         OFF CACHE BOOL "" FORCE)
set(LLAMA_SUBPROCESS      OFF CACHE BOOL "" FORCE)
set(LLAMA_LLGUIDANCE      OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL            OFF CACHE BOOL "" FORCE)
set(GGML_CUDA             OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE           ON  CACHE BOOL "" FORCE)
set(GGML_CCACHE           OFF CACHE BOOL "" FORCE)

add_subdirectory(llama.cpp)

set(FG_GEN ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
set(FG_COMMON ${CMAKE_CURRENT_SOURCE_DIR}/common)
set(FG_RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
set(FG_GATES ${CMAKE_CURRENT_SOURCE_DIR}/gates)
set(FG_HOST ${CMAKE_CURRENT_SOURCE_DIR}/host)

# One include interface: the merged unit header, the stage bridges and the
# shared host seam. Harmless in OFF (headers only) and the single place that
# knows where the merged unit lives.
add_library(fg-goose-headers INTERFACE)
target_include_directories(fg-goose-headers INTERFACE ${FG_GEN} ${FG_COMMON} ${FG_HOST})

# --- the one unit and the one host adapter, in a library of their own -------
#
# The unit and the adapter form one static library. They need nothing from
# ggml: the 14 IQ table views dispatch through a descriptor installed at
# start-up, and the CPU provider in ggml-quants.c is referenced weakly by the
# runtime. The dependency edge is one-way, ggml-base -> fg-goose-runtime, so
# the static-library cycle M1 avoided by hosting the objects inside ggml-base
# cannot appear at all.
if(FG_GOOSE_RUNTIME)
    add_library(fg-goose-runtime STATIC goose-gen/main.c host/goose_host.c)
    target_include_directories(fg-goose-runtime PRIVATE ${FG_GEN} ${FG_HOST})
    target_link_libraries(fg-goose-runtime PUBLIC m pthread)

    # ggml-base carries the CPU table provider and the S1.2/S1.3 bridges that
    # call into the runtime. PUBLIC puts the one copy on every consumer's link
    # line (llama, llama-common, the gates, llama-server).
    target_link_libraries(ggml-base PUBLIC fg-goose-runtime)
    # FG_GOOSE_RUNTIME also turns on the IQ table provider in ggml-quants.c,
    # which is needed by any of the ten flags, not only by FG_GOOSE_QUANT.
    target_compile_definitions(ggml-base PRIVATE FG_GOOSE_RUNTIME=1)
    target_include_directories(ggml-base PRIVATE ${FG_GEN} ${FG_HOST})
endif()
# Generated code, not edited by anyone: warnings off. The unit is compiled at
# -O1, the gate convention of this repository (mutations use -O0); Release would
# put -O3 on the same command line and this wins because it comes later.
set_source_files_properties(goose-gen/main.c PROPERTIES COMPILE_OPTIONS "-w;-O1")
set_source_files_properties(host/goose_host.c PROPERTIES COMPILE_OPTIONS "-w")

# --- S1.1: the schema-to-GBNF converter -------------------------------------

if(FG_GOOSE_JSON_SCHEMA)
    target_sources(llama-common PRIVATE common/fg-goose-schema-bridge.cpp)
    target_compile_definitions(llama-common PRIVATE FG_GOOSE_JSON_SCHEMA=1)
    target_link_libraries(llama-common PRIVATE fg-goose-headers)
target_include_directories(llama-common PRIVATE ${FG_GEN} ${FG_COMMON})
endif()

# --- S1.2: the GGUF writer and the split plan -------------------------------

if(FG_GOOSE_GGUF)
    target_sources(ggml-base PRIVATE common/fg-goose-gguf-bridge.cpp)
    target_compile_definitions(ggml-base PRIVATE FG_GOOSE_GGUF=1)
    target_include_directories(ggml-base PRIVATE ${FG_GEN} ${FG_COMMON})
endif()

# --- S1.3: the quant stack ---------------------------------------------------

if(FG_GOOSE_QUANT)
    # The bridge lives inside ggml-base, next to ggml-quants.c: the IQ tables it
    # exposes are file-static there.
    target_sources(ggml-base PRIVATE common/fg-goose-quant-bridge.c)
    target_compile_definitions(ggml-base PRIVATE FG_GOOSE_QUANT=1)
    target_include_directories(ggml-base PRIVATE ${FG_GEN} ${FG_COMMON})
endif()

# --- S1.4: the metrics and trace text encoders -------------------------------

# The bridge is compiled in both configurations: its OFF half is the native
# encoder plus the calls() witness that reports 0.
add_library(fg-bmoe-metrics STATIC
    runtime/core/src/metrics/decode_trace_sink.cpp
    runtime/core/src/metrics/route_trace_sink.cpp
    common/fg-goose-metrics-bridge.cpp)
target_link_libraries(fg-bmoe-metrics PUBLIC fg-goose-headers)
target_include_directories(fg-bmoe-metrics PUBLIC ${FG_RUNTIME}/core/include ${FG_COMMON})
set_target_properties(fg-bmoe-metrics PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

if(FG_GOOSE_METRICS)
    target_compile_definitions(fg-bmoe-metrics PRIVATE FG_GOOSE_METRICS=1)
    target_link_libraries(fg-bmoe-metrics PRIVATE fg-goose-headers)
target_include_directories(fg-bmoe-metrics PRIVATE ${FG_GEN})
    target_link_libraries(fg-bmoe-metrics PRIVATE ggml)
endif()

# --- S1.5: the tokenizer decisions ------------------------------------------

# The bridge is compiled inside libllama in both configurations; the flag picks
# the Goose bodies inside it.
target_sources(llama PRIVATE llama.cpp/src/fg-goose-tokenizer-bridge.cpp)
# goose-gen is the header home of the merged unit in both configurations: with
# the flag OFF the bridge includes it and uses none of it.
target_link_libraries(llama PRIVATE fg-goose-headers)
target_include_directories(llama PRIVATE ${FG_COMMON} ${FG_GEN})
if(FG_GOOSE_TOKENIZER)
    # Two carriers: llama/src/llama-vocab.cpp is in libllama, and the strip
    # helper string_strip lives in common/common.cpp, which is llama-common.
    target_compile_definitions(llama PRIVATE FG_GOOSE_TOKENIZER=1)
    target_compile_definitions(llama-common PRIVATE FG_GOOSE_TOKENIZER=1)
    target_include_directories(llama PRIVATE ${FG_GEN})
    target_link_libraries(llama PRIVATE ggml)
endif()

# --- S1.6: the stream decisions ---------------------------------------------

# A side library in the accepted card; here it is a static library so the gate
# and llama-common share one copy of the native transcription.
add_library(fg-stream-policy STATIC common/fg-stream-policy.cpp)
target_link_libraries(fg-stream-policy PUBLIC fg-goose-headers)
target_include_directories(fg-stream-policy PUBLIC ${FG_COMMON})
if(FG_GOOSE_STREAM)
    target_compile_definitions(fg-stream-policy PRIVATE FG_GOOSE_STREAM=1)
    target_link_libraries(fg-stream-policy PRIVATE fg-goose-headers)
target_include_directories(fg-stream-policy PRIVATE ${FG_GEN})
    target_link_libraries(fg-stream-policy PRIVATE ggml)
endif()

# --- S2.1: the config validator ---------------------------------------------

add_library(fg-config-core STATIC runtime/core/src/config.cpp)
target_link_libraries(fg-config-core PUBLIC fg-goose-headers)
target_include_directories(fg-config-core PUBLIC
    ${FG_RUNTIME}/core/include ${FG_COMMON})
if(FG_GOOSE_CONFIG)
    target_sources(fg-config-core PRIVATE common/fg-goose-config-bridge.cpp)
    target_compile_definitions(fg-config-core PRIVATE FG_GOOSE_CONFIG=1)
    target_link_libraries(fg-config-core PRIVATE fg-goose-headers)
target_include_directories(fg-config-core PRIVATE ${FG_GEN})
    target_link_libraries(fg-config-core PRIVATE ggml)
endif()

# --- S2.2: the speculative-decoding decisions -------------------------------

add_library(fg-spec-core STATIC
    common/fg-spec-decide.cpp
    runtime/core/src/engine/ngram_draft.cpp)
target_link_libraries(fg-spec-core PUBLIC fg-goose-headers)
target_include_directories(fg-spec-core PUBLIC
    ${FG_COMMON} ${FG_RUNTIME}/core/include)
if(FG_GOOSE_SPEC)
    target_sources(fg-spec-core PRIVATE common/fg-goose-spec-bridge.cpp)
    target_compile_definitions(fg-spec-core PRIVATE FG_GOOSE_SPEC=1)
    target_link_libraries(fg-spec-core PRIVATE fg-goose-headers)
target_include_directories(fg-spec-core PRIVATE ${FG_GEN})
    target_link_libraries(fg-spec-core PRIVATE ggml)
endif()

# --- S3.1: the model-management decisions -----------------------------------

add_library(fg-model-core STATIC common/fg-model-decide.cpp)
target_link_libraries(fg-model-core PUBLIC fg-goose-headers)
target_include_directories(fg-model-core PUBLIC ${FG_COMMON})
if(FG_GOOSE_MODELS)
    target_sources(fg-model-core PRIVATE common/fg-goose-model-bridge.cpp)
    target_compile_definitions(fg-model-core PRIVATE FG_GOOSE_MODELS=1)
    target_link_libraries(fg-model-core PRIVATE fg-goose-headers)
target_include_directories(fg-model-core PRIVATE ${FG_GEN})
    target_link_libraries(fg-model-core PRIVATE ggml)
endif()

# --- S3.2: the session decisions --------------------------------------------

add_library(fg-session-core STATIC common/fg-session-decide.cpp)
target_link_libraries(fg-session-core PUBLIC fg-goose-headers)
target_include_directories(fg-session-core PUBLIC ${FG_COMMON})
if(FG_GOOSE_SESSION)
    target_sources(fg-session-core PRIVATE common/fg-goose-session-bridge.cpp)
    target_compile_definitions(fg-session-core PRIVATE FG_GOOSE_SESSION=1)
    target_link_libraries(fg-session-core PRIVATE fg-goose-headers)
target_include_directories(fg-session-core PRIVATE ${FG_GEN})
    target_link_libraries(fg-session-core PRIVATE ggml)
endif()

# --- the ten accepted gates, one per flag -----------------------------------
#
# Each gate is the accepted card's own source. It runs in both configurations:
# OFF compares native against native (and must report goose_calls=0), ON
# compares the Goose path against the same oracle and must report goose_calls>0.

function(fg_gate name)
    cmake_parse_arguments(G "" "FLAG" "SOURCES;LIBS;INCLUDES" ${ARGN})
    add_executable(${name} ${G_SOURCES})
    target_link_libraries(${name} PRIVATE ${G_LIBS} fg-goose-headers)
    target_include_directories(${name} PRIVATE ${G_INCLUDES})
    if(G_FLAG AND ${G_FLAG})
        target_compile_definitions(${name} PRIVATE ${G_FLAG}=1)
    endif()
    if(FG_GOOSE_RUNTIME)
        # the unit and the adapter are their own library now
        target_link_libraries(${name} PRIVATE ggml fg-goose-runtime)
    endif()
endfunction()

# S1.1
fg_gate(fg-goose-smoke FLAG FG_GOOSE_JSON_SCHEMA
        SOURCES gates/s11/fg_goose_smoke.cpp
        LIBS llama-common
        INCLUDES ${FG_GATES}/s11 ${FG_COMMON} ${FG_GEN}
                 ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/common)

# S1.2
fg_gate(fg-gguf-smoke FLAG FG_GOOSE_GGUF
        SOURCES gates/s12/fg_gguf_smoke.cpp
        LIBS ggml
        INCLUDES ${FG_GATES}/s12 ${FG_COMMON} ${FG_GEN}
                 ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/ggml/src)

# S1.3
fg_gate(fg-goose-quant-smoke FLAG FG_GOOSE_QUANT
        SOURCES gates/s13/fg_goose_quant_smoke.c
        LIBS ggml
        INCLUDES ${FG_GATES}/s13 ${FG_COMMON} ${FG_GEN}
                 ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/ggml/include
                 ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/ggml/src)

# S1.4
fg_gate(fg-goose-metrics-smoke FLAG FG_GOOSE_METRICS
        SOURCES gates/s14/fg_goose_metrics_smoke.cpp
        LIBS fg-bmoe-metrics ${CMAKE_DL_LIBS}
        INCLUDES ${FG_GATES}/s14 ${FG_COMMON} ${FG_RUNTIME}/core/include)

# S1.5
fg_gate(fg-goose-tokenizer-smoke FLAG FG_GOOSE_TOKENIZER
        SOURCES gates/s15/fg_goose_tokenizer_smoke.cpp
        gates/s15/native-dispatch-base.cpp gates/s15/native-partition-base.cpp
        gates/s15/native-strip-base.cpp gates/s15/native-dispatch-staged.cpp
        gates/s15/native-partition-staged.cpp gates/s15/native-strip-staged.cpp
        LIBS llama-common
        INCLUDES ${FG_GATES}/s15 ${FG_COMMON} ${FG_GEN}
                 ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/src
                 ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/include
                 ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/common)

# S1.6
fg_gate(fg-goose-stream-gate FLAG FG_GOOSE_STREAM
        SOURCES gates/s16/fg_goose_stream_gate.cpp
        LIBS fg-stream-policy
        INCLUDES ${FG_GATES}/s16 ${FG_COMMON} ${FG_GEN})

# S2.1
fg_gate(fg-config-smoke FLAG FG_GOOSE_CONFIG
        SOURCES gates/s21/fg_config_smoke.cpp
        LIBS fg-config-core
        INCLUDES ${FG_GATES}/s21 ${FG_COMMON} ${FG_RUNTIME}/core/include)

# S2.2
fg_gate(fg-spec-smoke FLAG FG_GOOSE_SPEC
        SOURCES gates/s22/fg_spec_smoke.cpp
        LIBS fg-spec-core
        INCLUDES ${FG_GATES}/s22 ${FG_COMMON} ${FG_RUNTIME}/core/include)

# S3.1
fg_gate(fg-model-smoke FLAG FG_GOOSE_MODELS
        SOURCES gates/s31/fg_model_smoke.cpp
        LIBS fg-model-core
        INCLUDES ${FG_GATES}/s31 ${FG_COMMON})

# S3.2
fg_gate(fg-session-smoke FLAG FG_GOOSE_SESSION
        SOURCES gates/s32/fg_session_smoke.cpp
        LIBS fg-session-core
        INCLUDES ${FG_GATES}/s32 ${FG_COMMON})

# --- M1's own gates ---------------------------------------------------------
#
# host-smoke: two threads, one start-up, the descriptor installed by the
# embedding entry point. iq-seam: the data path itself - the unit's 14 views
# against the native tables, plus the unbound-abort negative control.

if(FG_GOOSE_RUNTIME)
    add_executable(fg-host-smoke gates/m1/host_smoke.c)
    target_link_libraries(fg-host-smoke PRIVATE ggml fg-goose-runtime fg-goose-headers m pthread)
    target_include_directories(fg-host-smoke PRIVATE ${FG_HOST})
    set_source_files_properties(gates/m1/host_smoke.c PROPERTIES COMPILE_OPTIONS "-w")

    add_executable(fg-iq-seam gates/m1/iq_seam.c)
    target_link_libraries(fg-iq-seam PRIVATE ggml fg-goose-runtime fg-goose-headers m pthread)
    target_include_directories(fg-iq-seam PRIVATE
        ${FG_HOST} ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/ggml/include)
    set_source_files_properties(gates/m1/iq_seam.c PROPERTIES COMPILE_OPTIONS "-w")
endif()

message(STATUS "M1 flags: schema=${FG_GOOSE_JSON_SCHEMA} gguf=${FG_GOOSE_GGUF} "
               "quant=${FG_GOOSE_QUANT} metrics=${FG_GOOSE_METRICS} "
               "tokenizer=${FG_GOOSE_TOKENIZER} stream=${FG_GOOSE_STREAM} "
               "config=${FG_GOOSE_CONFIG} spec=${FG_GOOSE_SPEC} "
               "models=${FG_GOOSE_MODELS} session=${FG_GOOSE_SESSION} "
               "runtime=${FG_GOOSE_RUNTIME}")
